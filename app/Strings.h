#include "MyUtils.h"

using json = nlohmann::json;

std::vector<std::string> languages = {"en", "pl", "it", "es", "pt", "fr", "ja", "zh", "ko"};

class Strings
{
public:
    // GUI Strings
    std::string UseControllerSpeakerInsteadOfSystemAudio = "Use speaker audio peaks instead of system audio";
    std::string UseControllerActuatorsInsteadOfSystemAudio = "Use actuator audio peaks instead of system audio";
    std::string RunAudioToHapticsOnStart = "Run audio to haptics on start";
    std::string Style = "Style";
    std::string SaveStyleToFile = "Save style to file";
    std::string LoadStyleFromFile = "Load style from file";
    std::string SetDefaultStyleFile = "Set default style file";
    std::string ResetStyle = "Reset style";
    std::string RemoveDefaultStyle = "Remove default style config";
    std::string DisablePlayerLED = "Disable player LED";
    std::string Speed = "Speed";
    std::string AudioEngineNotInitializedError = "Audio engine wasn't initialized, haptic feedback not available";
    std::string ScreenshotSoundVolume = "Screenshot sound volume";
    std::string QuietColor = "Quiet color";
    std::string MediumColor = "Medium color";
    std::string LoudColor = "Loud Color";
    std::string ShowConsole = "Show debug console";
    std::string UseHeadset = "Use headset";
    std::string TouchpadShortcut = "Touchpad + Mic button = Touchpad as mouse";
    std::string TouchpadAsSelectStart = "Touchpad as select/start";
    std::string DualShock4V2emu = "DualShock4 V2 (CUH-ZCT2x)";
    std::string LeftAnalogStickDeadZone = "Left analog stick deadzone";
    std::string RightAnalogStickDeadZone = "Right analog stick deadzone";
    std::string TriggersAsButtons = "Triggers as buttons";
    std::string SpeakerVolume = "Speaker volume";
    std::string HapticsUnavailableNoAudioDevice = "Couldn't find the DualSense Wireless Controller speaker associated with this controller. Haptic feedback not available";
    std::string HapticsToTriggers = "Haptics To Adaptive Triggers";
    std::string RumbleToAT_RigidMode = "Rigid trigger mode";
    std::string ControllerNumberText = "Controller No.";
    std::string USBorBTconnectionType = "Connection type";
    std::string BatteryLevel = "Battery level";
    std::string InstallLatestUpdate = "Install latest update";
    std::string LeftTriggerMode = "Left Trigger Mode";
    std::string RightTriggerMode = "Right Trigger Mode";
    std::string LEDandATunavailableUDP = "LED and Adaptive Trigger settings are unavailable while UDP is active";
    std::string HapticsUnavailable = "Haptic Feedback features are unavailable in Bluetooth mode.";
    std::string X360emu = "Start X360 emulation";
    std::string DS4emu = "Start DS4 emulation";
    std::string STOPemu = "Stop emulation";
    std::string ControllerEmuUserMode = "Application is not running as administrator, real controller will not be hidden from other apps durning controller emulation";
    std::string ControllerEmuAppAsAdmin = "Controller will be hidden only if HidHide Driver is installed";
    std::string Language = "Language";
    std::string Sensitivity = "Sensitivity";
    std::string GeneralData = "General data";
    std::string TouchPacketNum = "Touch packet number";
    std::string Touch = "Touch";
    std::string StandardRumble = "Standard rumble";
    std::string MicButton = "Microphone button";
    std::string LED_DS4_Unavailable = "LED settings are unavailable while emulating DualShock 4";
    std::string UDPStatus = "UDP Status";
    std::string _Touchpad = "Touchpad";
    std::string TouchpadToMouse = "Touchpad as mouse";
    std::string Active = "Active";
    std::string Inactive = "Inactive";
    std::string RumbleToAT = "Rumble To Adaptive Triggers";
    std::string SwapTriggersRumbleToAT = "Swap triggers";
    std::string MaxLeftIntensity = "Max left trigger intensity";
    std::string MaxLeftFrequency = "Max left trigger frequency";
    std::string MaxRightIntensity = "Max right trigger intensity";
    std::string MaxRightFrequency = "Max right trigger frequency";
    std::string LedSection = "LED";
    std::string AudioToLED = "Audio to LED";
    std::string DiscoMode = "Disco Mode";
    std::string BatteryLightbar = "Lightbar battery status";
    std::string LED_Red = "Red";
    std::string LED_Green = "Green";
    std::string LED_Blue = "Blue";
    std::string AdaptiveTriggers = "Adaptive Triggers";
    std::string HapticFeedback = "Haptic Feedback";
    std::string LeftMotor = "Left \"Motor\"";
    std::string RightMotor = "Right \"Motor\"";
    std::string StartAudioToHaptics = "Start [Audio To Haptics]";
    std::string TakeScreenshot = "Take screenshot";
    std::string RealMicFunctionality = "Real functionality";
    std::string SwapTriggersShortcut = "Up D-pad + Mic button = Swap triggers in \"Rumble To AT\" option";
    std::string X360Shortcut = "Left D-pad + Mic button = X360 Controller Emulation";
    std::string DS4Shortcut = "Right D-pad + Mic button = DS4 Emulation";
    std::string StopEmuShortcut = "Down D-pad + Mic button = Stop emulation";
    std::string EmulationHeader = "Controller emulation (DS4/X360)";
    std::string ConfigHeader = "Config";
    std::string SaveConfig = "Save current configuration";
    std::string LoadConfig = "Load configuration";
    std::string SetDefaultConfig = "Set default config for this port";
    std::string RemoveDefaultConfig = "Remove default config";
    std::string RunAsAdmin = "Run as administrator";
    std::string HideToTray = "Hide to tray";
    std::string HideWindowOnStartup = "Hide window on startup";
    std::string RunWithWindows = "Run with Windows";
    std::string Scale = "Scale";

    // Tooltips
    std::string Tooltip_Dualshock4V2 = "Emulates the second revision of DualShock 4, older games will not recognise it";
    std::string Tooltip_TriggersAsButtons = "Sets trigger resistance to very hard and L2/R2 to maximum on slightest trigger pull";
    std::string Tooltip_SpeakerVolume = "Sets speaker volume at hardware level, affects everything played on this controller";
    std::string Tooltip_HapticsToTriggers = "Turns haptic feedback signal to Adaptive Trigger effects, works only for games that don't write to the controller (Ex. Zenless Zone Zero) or Audio To Haptics feature";
    std::string Tooltip_RumbleToAT_RigidMode = "Sets rigid trigger effect";
    std::string Tooltip_RumbleToAT = "Translates rumble vibrations to adaptive triggers, really good in games that don't support them natively";
    std::string Tooltip_SwapTriggersRumbleToAT = "Sets left motor to right trigger and right motor to left trigger";
    std::string Tooltip_MaxIntensity = "Sets maximum trigger vibration sensitivity for Rumble To Adaptive Triggers option";
    std::string Tooltip_MaxFrequency = "Sets maximum trigger vibration frequency for Rumble To Adaptive Triggers option";
    std::string Tooltip_AudioToLED = "Sync the lightbar with audio levels";
    std::string Tooltip_DiscoMode = "Animated color transition, gamers' favorite.";
    std::string Tooltip_BatteryLightbar = "Display Battery Status with color-coded LED indicators.";
    std::string Tooltip_HapticFeedback = "Controls standard rumble values of your DualSense. This is what your controller does to emulate DualShock 4 rumble motors.";
    std::string Tooltip_StartAudioToHaptics = "Creates haptic feedback from your system audio.";
    std::string Tooltip_TakeScreenshot = "Takes screenshot on mic button click. It's saved to your clipboard and your Pictures directory";
    std::string Tooltip_RealMicFunctionality = "Mimics microphone button functionality from the PS5, only works on this controller's microphone.";
    std::string Tooltip_SwapTriggersShortcut = "Shortcut to toggle Swap Triggers in \"Rumble To AT\"";
    std::string Tooltip_X360Shortcut = "Shortcut to start X360 Controller Emulation";
    std::string Tooltip_DS4Shortcut = "Shortcut to start DS4 Controller Emulation";
    std::string Tooltip_StopEmuShortcut = "Shortcut to stop controller emulation";
    std::string Tooltip_SaveConfig = "Saves current values to a file";
    std::string Tooltip_LoadConfig = "Loads a selected configuration file";
    std::string Tooltip_SetDefaultConfig = "Sets default config for this port. Auto-loads when the controller is connected.";
    std::string Tooltip_RemoveDefaultConfig = "Removes default configuration for this port";
    std::string Tooltip_RunAsAdmin = "Runs the app as administrator on startup (requires restart)";
    std::string Tooltip_HideToTray = "Hides window after minimizing; click tray icon to restore";
    std::string Tooltip_HideWindowOnStartup = "Hides window at startup";
    std::string Tooltip_RunWithWindows = "Runs application on startup";
    std::string Tooltip_TouchpadToMouse = "Use dualsense touchpad like a laptop touchpad";

    json to_json() const;
    static Strings from_json(const json &j);
};

json Strings::to_json() const
{
    json j;
    #define ADD_MEMBER(name) j[#name] = name;
    
    // GUI Strings
    ADD_MEMBER(UseControllerSpeakerInsteadOfSystemAudio);
    ADD_MEMBER(UseControllerActuatorsInsteadOfSystemAudio);
    ADD_MEMBER(RunAudioToHapticsOnStart);
    ADD_MEMBER(Style);
    ADD_MEMBER(SaveStyleToFile);
    ADD_MEMBER(LoadStyleFromFile);
    ADD_MEMBER(SetDefaultStyleFile);
    ADD_MEMBER(ResetStyle);
    ADD_MEMBER(RemoveDefaultStyle);
    ADD_MEMBER(DisablePlayerLED);
    ADD_MEMBER(Speed);
    ADD_MEMBER(AudioEngineNotInitializedError);
    ADD_MEMBER(ScreenshotSoundVolume);
    ADD_MEMBER(QuietColor);
    ADD_MEMBER(MediumColor);
    ADD_MEMBER(LoudColor);
    ADD_MEMBER(ShowConsole);
    ADD_MEMBER(UseHeadset);
    ADD_MEMBER(TouchpadShortcut);
    ADD_MEMBER(TouchpadAsSelectStart);
    ADD_MEMBER(DualShock4V2emu);
    ADD_MEMBER(LeftAnalogStickDeadZone);
    ADD_MEMBER(RightAnalogStickDeadZone);
    ADD_MEMBER(TriggersAsButtons);
    ADD_MEMBER(SpeakerVolume);
    ADD_MEMBER(HapticsUnavailableNoAudioDevice);
    ADD_MEMBER(HapticsToTriggers);
    ADD_MEMBER(RumbleToAT_RigidMode);
    ADD_MEMBER(ControllerNumberText);
    ADD_MEMBER(USBorBTconnectionType);
    ADD_MEMBER(BatteryLevel);
    ADD_MEMBER(InstallLatestUpdate);
    ADD_MEMBER(LeftTriggerMode);
    ADD_MEMBER(RightTriggerMode);
    ADD_MEMBER(LEDandATunavailableUDP);
    ADD_MEMBER(HapticsUnavailable);
    ADD_MEMBER(X360emu);
    ADD_MEMBER(DS4emu);
    ADD_MEMBER(STOPemu);
    ADD_MEMBER(ControllerEmuUserMode);
    ADD_MEMBER(ControllerEmuAppAsAdmin);
    ADD_MEMBER(Language);
    ADD_MEMBER(Sensitivity);
    ADD_MEMBER(GeneralData);
    ADD_MEMBER(TouchPacketNum);
    ADD_MEMBER(Touch);
    ADD_MEMBER(StandardRumble);
    ADD_MEMBER(MicButton);
    ADD_MEMBER(LED_DS4_Unavailable);
    ADD_MEMBER(UDPStatus);
    j["Touchpad"] = _Touchpad;
    ADD_MEMBER(TouchpadToMouse);
    ADD_MEMBER(Active);
    ADD_MEMBER(Inactive);
    ADD_MEMBER(RumbleToAT);
    ADD_MEMBER(SwapTriggersRumbleToAT);
    ADD_MEMBER(MaxLeftIntensity);
    ADD_MEMBER(MaxLeftFrequency);
    ADD_MEMBER(MaxRightIntensity);
    ADD_MEMBER(MaxRightFrequency);
    ADD_MEMBER(LedSection);
    ADD_MEMBER(AudioToLED);
    ADD_MEMBER(DiscoMode);
    ADD_MEMBER(BatteryLightbar);
    ADD_MEMBER(LED_Red);
    ADD_MEMBER(LED_Green);
    ADD_MEMBER(LED_Blue);
    ADD_MEMBER(AdaptiveTriggers);
    ADD_MEMBER(HapticFeedback);
    ADD_MEMBER(LeftMotor);
    ADD_MEMBER(RightMotor);
    ADD_MEMBER(StartAudioToHaptics);
    ADD_MEMBER(TakeScreenshot);
    ADD_MEMBER(RealMicFunctionality);
    ADD_MEMBER(SwapTriggersShortcut);
    ADD_MEMBER(X360Shortcut);
    ADD_MEMBER(DS4Shortcut);
    ADD_MEMBER(StopEmuShortcut);
    ADD_MEMBER(EmulationHeader);
    ADD_MEMBER(ConfigHeader);
    ADD_MEMBER(SaveConfig);
    ADD_MEMBER(LoadConfig);
    ADD_MEMBER(SetDefaultConfig);
    ADD_MEMBER(RemoveDefaultConfig);
    ADD_MEMBER(RunAsAdmin);
    ADD_MEMBER(HideToTray);
    ADD_MEMBER(HideWindowOnStartup);
    ADD_MEMBER(RunWithWindows);
    ADD_MEMBER(Scale);

    // Tooltips
    ADD_MEMBER(Tooltip_Dualshock4V2);
    ADD_MEMBER(Tooltip_TriggersAsButtons);
    ADD_MEMBER(Tooltip_SpeakerVolume);
    ADD_MEMBER(Tooltip_HapticsToTriggers);
    ADD_MEMBER(Tooltip_RumbleToAT_RigidMode);
    ADD_MEMBER(Tooltip_RumbleToAT);
    ADD_MEMBER(Tooltip_SwapTriggersRumbleToAT);
    ADD_MEMBER(Tooltip_MaxIntensity);
    ADD_MEMBER(Tooltip_MaxFrequency);
    ADD_MEMBER(Tooltip_AudioToLED);
    ADD_MEMBER(Tooltip_DiscoMode);
    ADD_MEMBER(Tooltip_BatteryLightbar);
    ADD_MEMBER(Tooltip_HapticFeedback);
    ADD_MEMBER(Tooltip_StartAudioToHaptics);
    ADD_MEMBER(Tooltip_TakeScreenshot);
    ADD_MEMBER(Tooltip_RealMicFunctionality);
    ADD_MEMBER(Tooltip_SwapTriggersShortcut);
    ADD_MEMBER(Tooltip_X360Shortcut);
    ADD_MEMBER(Tooltip_DS4Shortcut);
    ADD_MEMBER(Tooltip_StopEmuShortcut);
    ADD_MEMBER(Tooltip_SaveConfig);
    ADD_MEMBER(Tooltip_LoadConfig);
    ADD_MEMBER(Tooltip_SetDefaultConfig);
    ADD_MEMBER(Tooltip_RemoveDefaultConfig);
    ADD_MEMBER(Tooltip_RunAsAdmin);
    ADD_MEMBER(Tooltip_HideToTray);
    ADD_MEMBER(Tooltip_HideWindowOnStartup);
    ADD_MEMBER(Tooltip_RunWithWindows);
    ADD_MEMBER(Tooltip_TouchpadToMouse);

    return j;
}

Strings Strings::from_json(const json &j)
{
    Strings strings;
    #define GET_MEMBER(name) if (j.contains(#name)) j.at(#name).get_to(strings.name);

    // GUI Strings
    GET_MEMBER(UseControllerSpeakerInsteadOfSystemAudio);
    GET_MEMBER(UseControllerActuatorsInsteadOfSystemAudio);
    GET_MEMBER(RunAudioToHapticsOnStart);
    GET_MEMBER(Scale);
    GET_MEMBER(Style);
    GET_MEMBER(SaveStyleToFile);
    GET_MEMBER(LoadStyleFromFile);
    GET_MEMBER(SetDefaultStyleFile);
    GET_MEMBER(ResetStyle);
    GET_MEMBER(RemoveDefaultStyle);
    GET_MEMBER(DisablePlayerLED);
    GET_MEMBER(Speed);
    GET_MEMBER(AudioEngineNotInitializedError);
    GET_MEMBER(ScreenshotSoundVolume);
    GET_MEMBER(QuietColor);
    GET_MEMBER(MediumColor);
    GET_MEMBER(LoudColor);
    GET_MEMBER(ShowConsole);
    GET_MEMBER(UseHeadset);
    GET_MEMBER(TouchpadShortcut);
    GET_MEMBER(TouchpadAsSelectStart);
    GET_MEMBER(DualShock4V2emu);
    GET_MEMBER(LeftAnalogStickDeadZone);
    GET_MEMBER(RightAnalogStickDeadZone);
    GET_MEMBER(TriggersAsButtons);
    GET_MEMBER(SpeakerVolume);
    GET_MEMBER(HapticsUnavailableNoAudioDevice);
    GET_MEMBER(HapticsToTriggers);
    GET_MEMBER(RumbleToAT_RigidMode);
    GET_MEMBER(ControllerNumberText);
    GET_MEMBER(USBorBTconnectionType);
    GET_MEMBER(BatteryLevel);
    GET_MEMBER(InstallLatestUpdate);
    GET_MEMBER(LeftTriggerMode);
    GET_MEMBER(RightTriggerMode);
    GET_MEMBER(LEDandATunavailableUDP);
    GET_MEMBER(HapticsUnavailable);
    GET_MEMBER(X360emu);
    GET_MEMBER(DS4emu);
    GET_MEMBER(STOPemu);
    GET_MEMBER(ControllerEmuUserMode);
    GET_MEMBER(ControllerEmuAppAsAdmin);
    GET_MEMBER(Language);
    GET_MEMBER(Sensitivity);
    GET_MEMBER(GeneralData);
    GET_MEMBER(TouchPacketNum);
    GET_MEMBER(Touch);
    GET_MEMBER(StandardRumble);
    GET_MEMBER(MicButton);
    GET_MEMBER(LED_DS4_Unavailable);
    GET_MEMBER(UDPStatus);
    if (j.contains("Touchpad")) j.at("Touchpad").get_to(strings._Touchpad);
    GET_MEMBER(TouchpadToMouse);
    GET_MEMBER(Active);
    GET_MEMBER(Inactive);
    GET_MEMBER(RumbleToAT);
    GET_MEMBER(SwapTriggersRumbleToAT);
    GET_MEMBER(MaxLeftIntensity);
    GET_MEMBER(MaxLeftFrequency);
    GET_MEMBER(MaxRightIntensity);
    GET_MEMBER(MaxRightFrequency);
    GET_MEMBER(LedSection);
    GET_MEMBER(AudioToLED);
    GET_MEMBER(DiscoMode);
    GET_MEMBER(BatteryLightbar);
    GET_MEMBER(LED_Red);
    GET_MEMBER(LED_Green);
    GET_MEMBER(LED_Blue);
    GET_MEMBER(AdaptiveTriggers);
    GET_MEMBER(HapticFeedback);
    GET_MEMBER(LeftMotor);
    GET_MEMBER(RightMotor);
    GET_MEMBER(StartAudioToHaptics);
    GET_MEMBER(TakeScreenshot);
    GET_MEMBER(RealMicFunctionality);
    GET_MEMBER(SwapTriggersShortcut);
    GET_MEMBER(X360Shortcut);
    GET_MEMBER(DS4Shortcut);
    GET_MEMBER(StopEmuShortcut);
    GET_MEMBER(EmulationHeader);
    GET_MEMBER(ConfigHeader);
    GET_MEMBER(SaveConfig);
    GET_MEMBER(LoadConfig);
    GET_MEMBER(SetDefaultConfig);
    GET_MEMBER(RemoveDefaultConfig);
    GET_MEMBER(RunAsAdmin);
    GET_MEMBER(HideToTray);
    GET_MEMBER(HideWindowOnStartup);
    GET_MEMBER(RunWithWindows);

    // Tooltips
    GET_MEMBER(Tooltip_Dualshock4V2);
    GET_MEMBER(Tooltip_TriggersAsButtons);
    GET_MEMBER(Tooltip_SpeakerVolume);
    GET_MEMBER(Tooltip_HapticsToTriggers);
    GET_MEMBER(Tooltip_RumbleToAT_RigidMode);
    GET_MEMBER(Tooltip_RumbleToAT);
    GET_MEMBER(Tooltip_SwapTriggersRumbleToAT);
    GET_MEMBER(Tooltip_MaxIntensity);
    GET_MEMBER(Tooltip_MaxFrequency);
    GET_MEMBER(Tooltip_AudioToLED);
    GET_MEMBER(Tooltip_DiscoMode);
    GET_MEMBER(Tooltip_BatteryLightbar);
    GET_MEMBER(Tooltip_HapticFeedback);
    GET_MEMBER(Tooltip_StartAudioToHaptics);
    GET_MEMBER(Tooltip_TakeScreenshot);
    GET_MEMBER(Tooltip_RealMicFunctionality);
    GET_MEMBER(Tooltip_SwapTriggersShortcut);
    GET_MEMBER(Tooltip_X360Shortcut);
    GET_MEMBER(Tooltip_DS4Shortcut);
    GET_MEMBER(Tooltip_StopEmuShortcut);
    GET_MEMBER(Tooltip_SaveConfig);
    GET_MEMBER(Tooltip_LoadConfig);
    GET_MEMBER(Tooltip_SetDefaultConfig);
    GET_MEMBER(Tooltip_RemoveDefaultConfig);
    GET_MEMBER(Tooltip_RunAsAdmin);
    GET_MEMBER(Tooltip_HideToTray);
    GET_MEMBER(Tooltip_HideWindowOnStartup);
    GET_MEMBER(Tooltip_RunWithWindows);
    GET_MEMBER(Tooltip_TouchpadToMouse);

    return strings;
}

static Strings ReadStringsFromFile(const std::string& language)
{
    Strings strings;
    std::string fileLocation = MyUtils::GetExecutableFolderPath() + "\\localizations\\" + language + ".json";
    
    std::ifstream file(fileLocation);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << fileLocation << std::endl;
        return strings;  // Return default strings or throw exception
    }

    try {
        json j;
        file >> j;
        strings = Strings::from_json(j);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }
    file.close();
    return strings;
}
