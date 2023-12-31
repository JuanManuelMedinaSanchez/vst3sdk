//------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again/source/again.cpp
// Created by  : Steinberg, 04/2005
// Description : AGain Example for VST SDK 3
//-----------------------------------------------------------------------------
#include "again.h"
#include "againcids.h" // for class ids
#include "againparamids.h"
#include "againprocess.h"

#include "public.sdk/source/vst/vstaudioprocessoralgo.h"
#include "public.sdk/source/vst/vsthelpers.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h" // for UString128
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstpresetkeys.h" // for use of IStreamAttributes

#include "base/source/fstreamer.h"

#include <cstdio>

namespace Steinberg {
namespace Vst {

// AGain constructor
AGain::AGain()
    : fGain(1.f) //->Initial value for the gain parameter (default gain = 1.0)
    , fGainReduction(0.f) //->Initial value for the gain reduction parameter (default gain reduction = 0.0)
    , fVuPPMOld(0.f) //->Initial value for the old VU meter value (default VU meter = 0.0)
    , currentProcessMode(-1) //-> -1 means not initialized
{
    //-> Register the editor class for the plugin (the same as used in againentry.cpp)
    setControllerClass(AGainControllerUID);
}

//-> AGain destructor
AGain::~AGain()
{
    //-> Nothing to do here yet..
}

//-> AGain initialize function
tresult PLUGIN_API AGain::initialize(FUnknown* context)
{
    //->Always initialize the parent class (AudioEffect)
    tresult result = AudioEffect::initialize(context);
    //-> If everything is OK, continue
    if (result != kResultOk)
    {
        return result;
    }

    //-> Create Audio In/Out busses
    //-> We want a stereo Input and a Stereo Output
    addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

    //-> Create Event In/Out busses (1 bus with only 1 channel)
    addEventInput(STR16("Event In"), 1);

    return kResultOk;
}

//-> AGain terminate function
tresult PLUGIN_API AGain::terminate()
{
    //-> Nothing to do here yet... except calling our parent terminate
    return AudioEffect::terminate();
}

//-> AGain setActive function
tresult PLUGIN_API AGain::setActive(TBool state)
{
    if (state)
    {
        //-> Send a text message to indicate that the plugin is set to active (true)
        sendTextMessage("AGain::setActive (true)");
    }
    else
    {
        //-> Send a text message to indicate that the plugin is set to inactive (false)
        sendTextMessage("AGain::setActive (false)");
    }

    //-> Reset the VU Meter value to 0
    fVuPPMOld = 0.f;

    //-> Call our parent setActive function
    return AudioEffect::setActive(state);
}

//-------------->AGain process function

tresult PLUGIN_API AGain::process(ProcessData& data)
{
    //-> Finally, the process function
    //-> In this example, there are 4 steps:
    //-> 1) Read input parameters coming from the host (to adapt model values)
    //-> 2) Read input events coming from the host (apply gain reduction based on the velocity of pressed keys)
    //-> 3) Process the gain of the input buffer to the output buffer
    //-> 4) Write the new VU meter value to the output parameters queue

    //-> Step 1: Read input parameter changes

    if (IParameterChanges* paramChanges = data.inputParameterChanges)
    {
        int32 numParamsChanged = paramChanges->getParameterCount();
        //-> For each parameter that has changes in this audio block:
        for (int32 i = 0; i < numParamsChanged; i++)
        {
            if (IParamValueQueue* paramQueue = paramChanges->getParameterData(i))
            {
                ParamValue value;
                int32 sampleOffset;
                int32 numPoints = paramQueue->getPointCount();
                //-> Process the changes for different parameters (e.g., gain and bypass)
                switch (paramQueue->getParameterId())
                {
                    case kGainId:
                        //-> Use the last point of the queue (in this example) to update the gain value
                        if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue)
                        {
                            fGain = (float)value;
                        }
                        break;
                    case kBypassId:
                        //-> Use the last point of the queue (in this example) to update the bypass value
                        if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue)
                        {
                            bBypass = (value > 0.5f);
                        }
                        break;
                }
            }
        }
    }

    //-> Step 2: Read input events
    if (IEventList* eventList = data.inputEvents)
    {
        int32 numEvent = eventList->getEventCount();
        for (int32 i = 0; i < numEvent; i++)
        {
            Event event;
            if (eventList->getEvent(i, event) == kResultOk)
            {
                //-> Process different event types (e.g., Note On and Note Off events)
                switch (event.type)
                {
                    case Event::kNoteOnEvent:
                        //-> Use the velocity of the Note On event to apply gain reduction
                        fGainReduction = event.noteOn.velocity;
                        break;
                    case Event::kNoteOffEvent:
                        //-> Note Off event resets the gain reduction
                        fGainReduction = 0.f;
                        break;
                }
            }
        }
    }

    // Step 3: Process Audio
    if (data.numInputs == 0 || data.numOutputs == 0)
    {
        // Nothing to do if there are no input or output channels
        return kResultOk;
    }

    int32 numChannels = data.inputs[0].numChannels;

    //-> Get audio buffers
    uint32 sampleFramesSize = getSampleFramesSizeInBytes(processSetup, data.numSamples);
    void** in = getChannelBuffersPointer(processSetup, data.inputs[0]);
    void** out = getChannelBuffersPointer(processSetup, data.outputs[0]);
    float fVuPPM = 0.f;

    //-> Check if all channels are silent, then process as silent
    if (data.inputs[0].silenceFlags == getChannelMask(data.inputs[0].numChannels))
    {
        //-> Mark output as silent too (it will help the host to propagate the silence)
        data.outputs[0].silenceFlags = data.inputs[0].silenceFlags;

        //-> If the input buffers are not the same as the output buffers, clear the output buffers
        for (int32 i = 0; i < numChannels; i++)
        {
            if (in[i] != out[i])
            {
                memset(out[i], 0, sampleFramesSize);
            }
        }
        //-> Set the VU Meter value to 0 in this case
        fVuPPM = 0.f;
    }
    else // We have to process (no silence)
    {
        //-> Mark our outputs as not silent
        data.outputs[0].silenceFlags = 0;

        //-> If in bypass mode, the outputs should be like the inputs (copy input to output)
        if (bBypass)
        {
            for (int32 i = 0; i < numChannels; i++)
            {
                if (in[i] != out[i])
                {
                    //-> Copy the input buffer to the output buffer
                    memcpy(out[i], in[i], sampleFramesSize);
                }
            }

            //-> Calculate the VU Meter value based on the input samples
            if (data.symbolicSampleSize == kSample32)
                fVuPPM = processVuPPM<Sample32>((Sample32**)in, numChannels, data.numSamples);
            else
                fVuPPM = processVuPPM<Sample64>((Sample64**)in, numChannels, data.numSamples);
        }
        else
        {
            //-> Apply gain factor to the input buffer to the output buffer
            float gain = (fGain - fGainReduction);
            if (bHalfGain)
            {
                gain = gain * 0.5f;
            }

            //-> If the applied gain is nearly zero, set the output buffers to zero and set silence flags
            if (gain < 0.0000001)
            {
                for (int32 i = 0; i < numChannels; i++)
                {
                    memset(out[i], 0, sampleFramesSize);
                }
                //-> Set the silence flags to 1 for all channels
                data.outputs[0].silenceFlags = getChannelMask(data.outputs[0].numChannels);
            }
            else //-> Process audio with the applied gain factor
            {
                if (data.symbolicSampleSize == kSample32)
                    fVuPPM = processAudio<Sample32>((Sample32**)in, (Sample32**)out, numChannels,
                        data.numSamples, gain);
                else
                    fVuPPM = processAudio<Sample64>((Sample64**)in, (Sample64**)out, numChannels,
                        data.numSamples, gain);
            }
        }
    }

    //-> Step 4: Write outputs parameter changes
    IParameterChanges* outParamChanges = data.outputParameterChanges;
    //-> If there are output parameter changes and the VU Meter value has changed
    if (outParamChanges && fVuPPMOld != fVuPPM)
    {
        int32 index = 0;
        //-> Add a new value of VU Meter to the output parameter changes
        IParamValueQueue* paramQueue = outParamChanges->addParameterData(kVuPPMId, index);
        if (paramQueue)
        {
            int32 index2 = 0;
            //-> Add the VU Meter value to the parameter queue at sample offset 0
            paramQueue->addPoint(0, fVuPPM, index2);
        }
    }
    //-> Update the old VU Meter value with the current VU Meter value
    fVuPPMOld = fVuPPM;

    return kResultOk;
}

//------------------------------------------------------------------------
tresult AGain::receiveText (const char* text)
{
	// received from Controller
	// Print the received text message to the standard error stream
	fprintf (stderr, "[AGain] received: ");
	fprintf (stderr, "%s", text);
	fprintf (stderr, "\n");

	// Toggle the bHalfGain flag (set it to its opposite value)
	bHalfGain = !bHalfGain;

	// Return kResultOk to indicate successful processing
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AGain::setState (IBStream* state)
{
	// called when we load a preset, the model has to be reloaded

	// Create an IBStreamer object to read the state data from the IBStream
	IBStreamer streamer (state, kLittleEndian);

	// Read the savedGain value from the state data
	float savedGain = 0.f;
	if (streamer.readFloat (savedGain) == false)
		return kResultFalse;

	// Read the savedGainReduction value from the state data
	float savedGainReduction = 0.f;
	if (streamer.readFloat (savedGainReduction) == false)
		return kResultFalse;

	// Read the savedBypass value from the state data
	int32 savedBypass = 0;
	if (streamer.readInt32 (savedBypass) == false)
		return kResultFalse;

	// Restore the model's state using the values read from the state data
	fGain = savedGain;
	fGainReduction = savedGainReduction;
	bBypass = savedBypass > 0;

	// Check if we are in the context of loading a project
	if (Helpers::isProjectState (state) == kResultTrue)
	{
		// We are in the project loading context...

		// Example of using the IStreamAttributes interface
		// Get the IStreamAttributes interface from the state
		FUnknownPtr<IStreamAttributes> stream (state);
		if (stream)
		{
			// Get the IAttributeList from the IStreamAttributes
			if (IAttributeList* list = stream->getAttributes ())
			{
				// Get the full file path of this state from the attribute list
				TChar fullPath[1024];
				memset (fullPath, 0, 1024 * sizeof (TChar));
				if (list->getString (PresetAttributes::kFilePathStringType, fullPath,
				                     1024 * sizeof (TChar)) == kResultTrue)
				{
					// Here we have the full path of the state file...
					// You can use this information for any custom processing.
				}
			}
		}
	}

	// Return kResultOk to indicate successful processing
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AGain::getState (IBStream* state)
{
	// Here we need to save the model

	// Create an IBStreamer object to write the model state to the IBStream
	IBStreamer streamer (state, kLittleEndian);

	// Write the fGain value to the state data
	streamer.writeFloat (fGain);

	// Write the fGainReduction value to the state data
	streamer.writeFloat (fGainReduction);

	// Write the bBypass flag as an int32 value (1 if true, 0 if false)
	streamer.writeInt32 (bBypass ? 1 : 0);

	// Return kResultOk to indicate successful processing
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AGain::setupProcessing (ProcessSetup& newSetup)
{
	// Called before the process call, always in a disable state (not active)

	// Here we keep a trace of the processing mode (offline,...) for example.
	// Update the currentProcessMode member variable with the processing mode obtained from newSetup.
	currentProcessMode = newSetup.processMode;

	// Call the setupProcessing function of the base class AudioEffect to perform any necessary setup procedures.
	return AudioEffect::setupProcessing (newSetup);
}
//------------------------------------------------------------------------

tresult PLUGIN_API AGain::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                              SpeakerArrangement* outputs, int32 numOuts)
{
	// This function is called to set the bus arrangements for the plugin.
	// It is responsible for configuring the audio inputs and outputs based on the host's requirements.
	
	if (numIns == 1 && numOuts == 1)
	{
		// The host wants Mono => Mono (1 channel -> 1 channel).
		if (SpeakerArr::getChannelCount(inputs[0]) == 1 && SpeakerArr::getChannelCount(outputs[0]) == 1)
		{
			// Check if the bus arrangements are Mono => Mono; if not, recreate the busses with the new arrangements.
			auto* bus = FCast<AudioBus>(audioInputs.at(0));
			if (bus)
			{
				if (bus->getArrangement() != inputs[0])
				{
					getAudioInput(0)->setArrangement(inputs[0]);
					getAudioInput(0)->setName(STR16("Mono In"));
					getAudioOutput(0)->setArrangement(inputs[0]);
					getAudioOutput(0)->setName(STR16("Mono Out"));
				}
				return kResultOk;
			}
		}
		// The host wants something else than Mono => Mono.
		// In this case, we always configure the plugin as Stereo => Stereo.
		else
		{
			auto* bus = FCast<AudioBus>(audioInputs.at(0));
			if (bus)
			{
				tresult result = kResultFalse;

				// The host wants 2->2 (could be LsRs -> LsRs).
				if (SpeakerArr::getChannelCount(inputs[0]) == 2 && SpeakerArr::getChannelCount(outputs[0]) == 2)
				{
					getAudioInput(0)->setArrangement(inputs[0]);
					getAudioInput(0)->setName(STR16("Stereo In"));
					getAudioOutput(0)->setArrangement(outputs[0]);
					getAudioOutput(0)->setName(STR16("Stereo Out"));
					result = kResultTrue;
				}
				// The host wants something different than 1->1 or 2->2; in this case, we want stereo.
				else if (bus->getArrangement() != SpeakerArr::kStereo)
				{
					getAudioInput(0)->setArrangement(SpeakerArr::kStereo);
					getAudioInput(0)->setName(STR16("Stereo In"));
					getAudioOutput(0)->setArrangement(SpeakerArr::kStereo);
					getAudioOutput(0)->setName(STR16("Stereo Out"));
					result = kResultFalse;
				}

				return result;
			}
		}
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AGain::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                              SpeakerArrangement* outputs, int32 numOuts)
{
	// This function is called to set the bus arrangements for the plugin.
	// It is responsible for configuring the audio inputs and outputs based on the host's requirements.
	
	if (numIns == 1 && numOuts == 1)
	{
		// The host wants Mono => Mono (1 channel -> 1 channel).
		if (SpeakerArr::getChannelCount(inputs[0]) == 1 && SpeakerArr::getChannelCount(outputs[0]) == 1)
		{
			// Check if the bus arrangements are Mono => Mono; if not, recreate the busses with the new arrangements.
			auto* bus = FCast<AudioBus>(audioInputs.at(0));
			if (bus)
			{
				if (bus->getArrangement() != inputs[0])
				{
					getAudioInput(0)->setArrangement(inputs[0]);
					getAudioInput(0)->setName(STR16("Mono In"));
					getAudioOutput(0)->setArrangement(inputs[0]);
					getAudioOutput(0)->setName(STR16("Mono Out"));
				}
				return kResultOk;
			}
		}
		// The host wants something else than Mono => Mono.
		// In this case, we always configure the plugin as Stereo => Stereo.
		else
		{
			auto* bus = FCast<AudioBus>(audioInputs.at(0));
			if (bus)
			{
				tresult result = kResultFalse;

				// The host wants 2->2 (could be LsRs -> LsRs).
				if (SpeakerArr::getChannelCount(inputs[0]) == 2 && SpeakerArr::getChannelCount(outputs[0]) == 2)
				{
					getAudioInput(0)->setArrangement(inputs[0]);
					getAudioInput(0)->setName(STR16("Stereo In"));
					getAudioOutput(0)->setArrangement(outputs[0]);
					getAudioOutput(0)->setName(STR16("Stereo Out"));
					result = kResultTrue;
				}
				// The host wants something different than 1->1 or 2->2; in this case, we want stereo.
				else if (bus->getArrangement() != SpeakerArr::kStereo)
				{
					getAudioInput(0)->setArrangement(SpeakerArr::kStereo);
					getAudioInput(0)->setName(STR16("Stereo In"));
					getAudioOutput(0)->setArrangement(SpeakerArr::kStereo);
					getAudioOutput(0)->setName(STR16("Stereo Out"));
					result = kResultFalse;
				}

				return result;
			}
		}
	}
	return kResultFalse;
}
//------------------------------------------------------------------------
tresult PLUGIN_API AGain::canProcessSampleSize(int32 symbolicSampleSize)
{
	// This function checks if the plugin supports the given sample size.
	// If the sample size is either 32-bit or 64-bit (kSample32 or kSample64),
	// it returns kResultTrue, indicating that the plugin can process audio with that sample size.

	if (symbolicSampleSize == kSample32)
		return kResultTrue;

	// The plugin also supports double processing if the sample size is 64-bit.
	if (symbolicSampleSize == kSample64)
		return kResultTrue;

	// For any other sample size, return kResultFalse, indicating that the plugin cannot process audio with that size.
	return kResultFalse;
}
//------------------------------------------------------------------------
tresult PLUGIN_API AGain::notify(IMessage* message)
{
	// This function is called when the plugin receives a notification or message from the host application.
	// It checks if the received message is of type "BinaryMessage" and extracts binary data from the message.
	// If the message contains a binary data tag "MyData" with a size of 100 and the second byte is equal to 1,
	// it prints a message to the standard error stream (stderr) indicating that it received the binary message.
	// If the message is not of type "BinaryMessage" or does not meet the specified conditions, it calls the base class's notify function.

	if (!message)
		return kInvalidArgument;

	if (strcmp(message->getMessageID(), "BinaryMessage") == 0)
	{
		const void* data;
		uint32 size;
		if (message->getAttributes()->getBinary("MyData", data, size) == kResultOk)
		{
			// We are in the UI thread
			// Size should be 100
			if (size == 100 && ((char*)data)[1] == 1) // yeah...
			{
				fprintf(stderr, "[AGain] received the binary message!\n");
			}
			return kResultOk;
		}
	}

	return AudioEffect::notify(message);
}


