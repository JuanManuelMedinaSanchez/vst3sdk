#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/base/futils.h"
#include "gainparameter.h" // Include your custom GainParameter header file here

using namespace VSTGUI;

namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
// GainParameter Declaration
//------------------------------------------------------------------------
// Create a custom parameter class for GainParameter that overwrites toString and fromString methods
class GainParameter : public Parameter
{
public:
	GainParameter (int32 flags, int32 id);

	// Override methods for custom parameter handling
	void toString (ParamValue normValue, String128 string) const SMTG_OVERRIDE;
	bool fromString (const TChar* string, ParamValue& normValue) const SMTG_OVERRIDE;
};

//------------------------------------------------------------------------
// GainParameter Implementation
//------------------------------------------------------------------------
GainParameter::GainParameter (int32 flags, int32 id)
{
	// Set the title and units for the GainParameter
	Steinberg::UString (info.title, USTRINGSIZE (info.title)).assign (USTRING ("Gain"));
	Steinberg::UString (info.units, USTRINGSIZE (info.units)).assign (USTRING ("dB"));

	// Set the parameter information (flags, ID, stepCount, defaultNormalizedValue, unitId)
	info.flags = flags;
	info.id = id;
	info.stepCount = 0;
	info.defaultNormalizedValue = 0.5f;
	info.unitId = kRootUnitId;

	// Set the normalized value to 1.0 (maximum)
	setNormalized (1.f);
}

// Implementation of toString method for GainParameter
void GainParameter::toString (ParamValue normValue, String128 string) const
{
	// Convert normalized value to dB and convert it to a string
	char text[32];
	if (normValue > 0.0001)
	{
		snprintf (text, 32, "%.2f", 20 * log10f ((float)normValue));
	}
	else
	{
		strcpy (text, "-oo");
	}

	// Convert the string to a UTF-16 string and store it in the output string buffer
	Steinberg::UString (string, 128).fromAscii (text);
}

// Implementation of fromString method for GainParameter
bool GainParameter::fromString (const TChar* string, ParamValue& normValue) const
{
	// Convert the input string to a floating-point value
	String wrapper ((TChar*)string); // don't know buffer size here!
	double tmp = 0.0;
	if (wrapper.scanFloat (tmp))
	{
		// Convert the dB value to a normalized value between 0 and 1
		if (tmp > 0.0)
		{
			tmp = -tmp;
		}
		normValue = expf (logf (10.f) * (float)tmp / 20.f);
		return true;
	}
	return false;
}

//------------------------------------------------------------------------
// AGainController Implementation
//------------------------------------------------------------------------
// Initialize the AGainController
tresult PLUGIN_API AGainController::initialize (FUnknown* context)
{
	// Call the base class's initialize method
	tresult result = EditControllerEx1::initialize (context);
	if (result != kResultOk)
	{
		return result;
	}

	// Create Units
	UnitInfo unitInfo;
	Unit* unit;

	// Create a unit1 for the gain
	unitInfo.id = 1;
	unitInfo.parentUnitId = kRootUnitId; // attached to the root unit
	Steinberg::UString (unitInfo.name, USTRINGSIZE (unitInfo.name)).assign (USTRING ("Unit1"));
	unitInfo.programListId = kNoProgramListId;

	unit = new Unit (unitInfo);
	addUnit (unit);

	// Create Parameters

	// Gain parameter
	auto* gainParam = new GainParameter (ParameterInfo::kCanAutomate, kGainId);
	parameters.addParameter (gainParam);
	gainParam->setUnitID (1);

	// VuMeter parameter
	int32 stepCount = 0;
	ParamValue defaultVal = 0;
	int32 flags = ParameterInfo::kIsReadOnly;
	int32 tag = kVuPPMId;
	parameters.addParameter (STR16 ("VuPPM"), nullptr, stepCount, defaultVal, flags, tag);

	// Bypass parameter
	stepCount = 1;
	defaultVal = 0;
	flags = ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass;
	tag = kBypassId;
	parameters.addParameter (STR16 ("Bypass"), nullptr, stepCount, defaultVal, flags, tag);

	// Custom state initialization
	String str ("Mi primer plugin :')");
	str.copyTo16 (defaultMessageText, 0, 127);

	return result;
}

// Terminate the AGainController
tresult PLUGIN_API AGainController::terminate ()
{
	return EditControllerEx1::terminate ();
}

// Set the component state of the AGainController
tresult PLUGIN_API AGainController::setComponentState (IBStream* state)
{
	// ... (Implementation of setComponentState)

	return kResultOk;
}

// Create the view for the AGainController
IPlugView* PLUGIN_API AGainController::createView (const char* _name)
{
	// ... (Implementation of createView)

	return nullptr;
}

// Create a sub-controller for the AGainController
IController* AGainController::createSubController (UTF8StringPtr name,
                                                   const IUIDescription* /*description*/,
                                                   VST3Editor* /*editor*/)
{
	// ... (Implementation of createSubController)

	return nullptr;
}

// Set the state of the AGainController
tresult PLUGIN_API AGainController::setState (IBStream* state)
{
	// ... (Implementation of setState)

	return kResultTrue;
}

// Get the state of the AGainController
tresult PLUGIN_API AGainController::getState (IBStream* state)
{
	// ... (Implementation of getState)

	return kResultTrue;
}

// Receive text for the AGainController
tresult AGainController::receiveText (const char* text)
{
	// ... (Implementation of receiveText)

	return kResultOk;
}

// Set the normalized value of a parameter in the AGainController
tresult PLUGIN_API AGainController::setParamNormalized (ParamID tag, ParamValue value)
{
	// ... (Implementation of setParamNormalized)

	return kResultOk;
}

// Get the string representation of a parameter value in the AGainController
tresult PLUGIN_API AGainController::getParamStringByValue (ParamID tag, ParamValue valueNormalized,
                                                           String128 string)
{
	// ... (Implementation of getParamStringByValue)

	return kResultTrue;
}

// Get the normalized value of a parameter from its string representation in the AGainController
tresult PLUGIN_API AGainController::getParamValueByString (ParamID tag, TChar* string,
                                                           ParamValue& valueNormalized)
{
	// ... (Implementation of getParamValueByString)

	return kResultTrue;
}

// Add a UIMessageController to the AGainController
void AGainController::addUIMessageController (UIMessageController* controller)
{
	uiMessageControllers.push_back (controller);
}

// Remove a UIMessageController from the AGainController
void AGainController::removeUIMessageController (UIMessageController* controller)
{
	UIMessageControllerList::const_iterator it =
	    std::find (uiMessageControllers.begin (), uiMessageControllers.end (), controller);
	if (it != uiMessageControllers.end ())
		uiMessageControllers.erase (it);
}

// Set the default message text in the AGainController
void AGainController::setDefaultMessageText (String128 text)
{
	String tmp (text);
	tmp.copyTo16 (defaultMessageText, 0, 127);
}

// Get the default message text from the AGainController
TChar* AGainController::getDefaultMessageText ()
{
	return defaultMessageText;
}

// Query interface for the AGainController
tresult PLUGIN_API AGainController::queryInterface (const char* iid, void** obj)
{
	// ... (Implementation of queryInterface)

	return EditControllerEx1::queryInterface (iid, obj);
}

// Get MIDI controller assignment for the AGainController
tresult PLUGIN_API AGainController::getMidiControllerAssignment (int32 busIndex,
                                                                 int16 /*midiChannel*/,
                                                                 CtrlNumber midiControllerNumber,
                                                                 ParamID& tag)
{
	// ... (Implementation of getMidiControllerAssignment)

	return kResultFalse;
}

//------------------------------------------------------------------------
} // namespace Vst
} // namespace Steinberg
