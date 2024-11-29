#include "MyUtils.h"

std::vector<std::string> languages = {"en", "pl", "it", "es", "pt", "fr", "ja"};

class Strings
{
public:
    // GUI Strings
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
    std::string Touchpad = "Touchpad";
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
    std::string SwapTriggersShortcut =
        "Up D-pad + Mic button = Swap triggers in \"Rumble To AT\" option";
    std::string X360Shortcut =
        "Left D-pad + Mic button = X360 Controller Emulation";
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

    // Tooltips
    std::string Tooltip_Dualshock4V2 = "Emulates the second revision of DualShock 4, older games will not recognise it";
    std::string Tooltip_TriggersAsButtons = "Sets trigger resistance to very hard and L2/R2 to maximum on slightest trigger pull";
    std::string Tooltip_SpeakerVolume = "Sets speaker volume at hardware level, affects everything played on this controller";
    std::string Tooltip_HapticsToTriggers = "Turns haptic feedback signal to Adaptive Trigger effects, works only for games that don't write to the controller (Ex. Zenless Zone Zero) or Audio To Haptics feature";
    std::string Tooltip_RumbleToAT_RigidMode = "Sets rigid trigger effect";
    std::string Tooltip_RumbleToAT =
        "Translates rumble vibrations to adaptive triggers, really good in "
        "games that don't support them natively";
    std::string Tooltip_SwapTriggersRumbleToAT =
        "Sets left motor to right trigger and right motor to left trigger";
    std::string Tooltip_MaxIntensity =
        "Sets maximum trigger vibration sensitivity for Rumble To Adaptive "
        "Triggers option";
    std::string Tooltip_MaxFrequency =
        "Sets maximum trigger vibration frequency for Rumble To Adaptive "
        "Triggers option";
    std::string Tooltip_AudioToLED = "Sync the lightbar with audio levels.";
    std::string Tooltip_DiscoMode =
        "Animated color transition, gamers' favorite.";
    std::string Tooltip_BatteryLightbar =
        "Display Battery Status with color-coded LED indicators.";
    std::string Tooltip_HapticFeedback =
        "Controls standard rumble values of your DualSense. This is what your "
        "controller does to emulate DualShock 4 rumble motors.";
    std::string Tooltip_StartAudioToHaptics =
        "Creates haptic feedback from your system audio.";
    std::string Tooltip_TakeScreenshot =
        "Takes screenshot on mic button click. It's saved to your clipboard "
        "and your Pictures directory";
    std::string Tooltip_RealMicFunctionality =
        "Mimics microphone button functionality from the PS5, only works on "
        "this controller's microphone.";
    std::string Tooltip_SwapTriggersShortcut =
        "Shortcut to toggle Swap Triggers in \"Rumble To AT\"";
    std::string Tooltip_X360Shortcut =
        "Shortcut to start X360 Controller Emulation";
    std::string Tooltip_DS4Shortcut =
        "Shortcut to start DS4 Controller Emulation";
    std::string Tooltip_StopEmuShortcut =
        "Shortcut to stop controller emulation";
    std::string Tooltip_SaveConfig = "Saves current values to a file";
    std::string Tooltip_LoadConfig = "Loads a selected configuration file";
    std::string Tooltip_SetDefaultConfig =
        "Sets default config for this port. Auto-loads when the controller is "
        "connected.";
    std::string Tooltip_RemoveDefaultConfig =
        "Removes default configuration for this port";
    std::string Tooltip_RunAsAdmin =
        "Runs the app as administrator on startup (requires restart)";
    std::string Tooltip_HideToTray =
        "Hides window after minimizing; click tray icon to restore";
    std::string Tooltip_HideWindowOnStartup = "Hides window at startup";
    std::string Tooltip_RunWithWindows = "Runs application on startup";
    std::string Tooltip_TouchpadToMouse = "Use dualsense touchpad like a laptop touchpad";

    nlohmann::json to_json() const
    {
        nlohmann::json j;
        // GUI Strings
        j["UseHeadset"] = UseHeadset;
        j["TouchpadShortcut"] = TouchpadShortcut;
        j["TouchpadAsSelectStart"] = TouchpadAsSelectStart;
        j["DualShock4V2emu"] = DualShock4V2emu;
        j["Tooltip_Dualshock4V2"] = Tooltip_Dualshock4V2;
        j["LeftAnalogStickDeadZone"] = LeftAnalogStickDeadZone;
        j["RightAnalogStickDeadZone"] = RightAnalogStickDeadZone;
        j["Tooltip_TriggersAsButtons"] = Tooltip_TriggersAsButtons;
        j["TriggersAsButtons"] = TriggersAsButtons;
        j["SpeakerVolume"] = SpeakerVolume;
        j["Tooltip_SpeakerVolume"] = Tooltip_SpeakerVolume;
        j["HapticsUnavailableNoAudioDevice"] = HapticsUnavailableNoAudioDevice;
        j["HapticsToTriggers"] = HapticsToTriggers;
        j["Tooltip_RumbleToAT_RigidMode"] = Tooltip_RumbleToAT_RigidMode;
        j["RumbleToAT_RigidMode"] = RumbleToAT_RigidMode;
        j["ControllerNumberText"] = ControllerNumberText;
        j["USBorBTconnectionType"] = USBorBTconnectionType;
        j["BatteryLevel"] = BatteryLevel;
        j["InstallLatestUpdate"] = InstallLatestUpdate;
        j["LeftTriggerMode"] = LeftTriggerMode;
        j["RightTriggerMode"] = RightTriggerMode;
        j["LEDandATunavailableUDP"] = LEDandATunavailableUDP;
        j["HapticsUnavailable"] = HapticsUnavailable;
        j["X360emu"] = X360emu;
        j["DS4emu"] = DS4emu;
        j["STOPemu"] = STOPemu;
        j["ControllerEmuUserMode"] = ControllerEmuUserMode;
        j["ControllerEmuAppAsAdmin"] = ControllerEmuAppAsAdmin;
        j["Language"] = Language;
        j["Sensitivity"] = Sensitivity;
        j["GeneralData"] = GeneralData;
        j["TouchPacketNum"] = TouchPacketNum;
        j["Touch"] = Touch;  
        j["MicButton"] = MicButton;   
        j["TouchpadToMouse"] = TouchpadToMouse;       
        j["StandardRumble"] = StandardRumble;
        j["LED_DS4_Unavailable"] = LED_DS4_Unavailable;
        j["UDPStatus"] = UDPStatus;
        j["Active"] = Active;
        j["Inactive"] = Inactive;
        j["RumbleToAT"] = RumbleToAT;
        j["SwapTriggersRumbleToAT"] = SwapTriggersRumbleToAT;
        j["MaxLeftIntensity"] = MaxLeftIntensity;
        j["MaxLeftFrequency"] = MaxLeftFrequency;
        j["MaxRightIntensity"] = MaxRightIntensity;
        j["MaxRightFrequency"] = MaxRightFrequency;
        j["LedSection"] = LedSection;
        j["AudioToLED"] = AudioToLED;
        j["DiscoMode"] = DiscoMode;
        j["BatteryLightbar"] = BatteryLightbar;
        j["LED_Red"] = LED_Red;
        j["LED_Green"] = LED_Green;
        j["LED_Blue"] = LED_Blue;
        j["AdaptiveTriggers"] = AdaptiveTriggers;
        j["HapticFeedback"] = HapticFeedback;
        j["LeftMotor"] = LeftMotor;
        j["RightMotor"] = RightMotor;
        j["StartAudioToHaptics"] = StartAudioToHaptics;
        j["TakeScreenshot"] = TakeScreenshot;
        j["RealMicFunctionality"] = RealMicFunctionality;
        j["SwapTriggersShortcut"] = SwapTriggersShortcut;
        j["X360Shortcut"] = X360Shortcut;
        j["DS4Shortcut"] = DS4Shortcut;
        j["StopEmuShortcut"] = StopEmuShortcut;
        j["EmulationHeader"] = EmulationHeader;
        j["ConfigHeader"] = ConfigHeader;
        j["SaveConfig"] = SaveConfig;
        j["LoadConfig"] = LoadConfig;
        j["SetDefaultConfig"] = SetDefaultConfig;
        j["RemoveDefaultConfig"] = RemoveDefaultConfig;
        j["RunAsAdmin"] = RunAsAdmin;
        j["HideToTray"] = HideToTray;
        j["HideWindowOnStartup"] = HideWindowOnStartup;
        j["RunWithWindows"] = RunWithWindows;

        // Tooltips
        j["Tooltip_RumbleToAT"] = Tooltip_RumbleToAT;
        j["Tooltip_SwapTriggersRumbleToAT"] = Tooltip_SwapTriggersRumbleToAT;
        j["Tooltip_MaxIntensity"] = Tooltip_MaxIntensity;
        j["Tooltip_MaxFrequency"] = Tooltip_MaxFrequency;
        j["Tooltip_AudioToLED"] = Tooltip_AudioToLED;
        j["Tooltip_DiscoMode"] = Tooltip_DiscoMode;
        j["Tooltip_BatteryLightbar"] = Tooltip_BatteryLightbar;
        j["Tooltip_HapticFeedback"] = Tooltip_HapticFeedback;
        j["Tooltip_StartAudioToHaptics"] = Tooltip_StartAudioToHaptics;
        j["Tooltip_TakeScreenshot"] = Tooltip_TakeScreenshot;
        j["Tooltip_RealMicFunctionality"] = Tooltip_RealMicFunctionality;
        j["Tooltip_SwapTriggersShortcut"] = Tooltip_SwapTriggersShortcut;
        j["Tooltip_X360Shortcut"] = Tooltip_X360Shortcut;
        j["Tooltip_DS4Shortcut"] = Tooltip_DS4Shortcut;
        j["Tooltip_StopEmuShortcut"] = Tooltip_StopEmuShortcut;
        j["Tooltip_SaveConfig"] = Tooltip_SaveConfig;
        j["Tooltip_LoadConfig"] = Tooltip_LoadConfig;
        j["Tooltip_SetDefaultConfig"] = Tooltip_SetDefaultConfig;
        j["Tooltip_RemoveDefaultConfig"] = Tooltip_RemoveDefaultConfig;
        j["Tooltip_RunAsAdmin"] = Tooltip_RunAsAdmin;
        j["Tooltip_HideToTray"] = Tooltip_HideToTray;
        j["Tooltip_HideWindowOnStartup"] = Tooltip_HideWindowOnStartup;
        j["Tooltip_RunWithWindows"] = Tooltip_RunWithWindows;
        j["Tooltip_TouchpadToMouse"] = Tooltip_TouchpadToMouse;

        return j;
    }

    static Strings from_json(const nlohmann::json &j)
    {
        Strings strings;

        if (j.contains("UseHeadset"))
            j.at("UseHeadset").get_to(strings.UseHeadset);

        if (j.contains("TouchpadShortcut"))
            j.at("TouchpadShortcut").get_to(strings.TouchpadShortcut);

        if (j.contains("TouchpadAsSelectStart"))
            j.at("TouchpadAsSelectStart").get_to(strings.TouchpadAsSelectStart);

        if (j.contains("DualShock4V2emu"))
            j.at("DualShock4V2emu").get_to(strings.DualShock4V2emu);

        if (j.contains("Tooltip_Dualshock4V2"))
            j.at("Tooltip_Dualshock4V2").get_to(strings.Tooltip_Dualshock4V2);

        if (j.contains("LeftAnalogStickDeadZone"))
            j.at("LeftAnalogStickDeadZone").get_to(strings.LeftAnalogStickDeadZone);

        if (j.contains("RightAnalogStickDeadZone"))
            j.at("RightAnalogStickDeadZone").get_to(strings.RightAnalogStickDeadZone);

        if (j.contains("TriggersAsButtons"))
            j.at("TriggersAsButtons").get_to(strings.TriggersAsButtons);

        if (j.contains("Tooltip_TriggersAsButtons"))
            j.at("Tooltip_TriggersAsButtons").get_to(strings.Tooltip_TriggersAsButtons);

        if (j.contains("SpeakerVolume"))
            j.at("SpeakerVolume").get_to(strings.SpeakerVolume);

        if (j.contains("Tooltip_SpeakerVolume"))
            j.at("Tooltip_SpeakerVolume").get_to(strings.Tooltip_SpeakerVolume);

        if (j.contains("HapticsUnavailableNoAudioDevice"))
            j.at("HapticsUnavailableNoAudioDevice").get_to(strings.HapticsUnavailableNoAudioDevice);

        if (j.contains("HapticsToTriggers"))
            j.at("HapticsToTriggers").get_to(strings.HapticsToTriggers);

        if (j.contains("Tooltip_RumbleToAT_RigidMode"))
            j.at("Tooltip_RumbleToAT_RigidMode").get_to(strings.Tooltip_RumbleToAT_RigidMode);

        if (j.contains("RumbleToAT_RigidMode"))
            j.at("RumbleToAT_RigidMode").get_to(strings.RumbleToAT_RigidMode);

        if (j.contains("ControllerNumberText"))
            j.at("ControllerNumberText").get_to(strings.ControllerNumberText);

        if (j.contains("USBorBTconnectionType"))
            j.at("USBorBTconnectionType").get_to(strings.USBorBTconnectionType);

        if (j.contains("BatteryLevel"))
            j.at("BatteryLevel").get_to(strings.BatteryLevel);

        if (j.contains("InstallLatestUpdate"))
            j.at("InstallLatestUpdate").get_to(strings.InstallLatestUpdate);

        if (j.contains("LeftTriggerMode"))
            j.at("LeftTriggerMode").get_to(strings.LeftTriggerMode);

        if (j.contains("RightTriggerMode"))
            j.at("RightTriggerMode").get_to(strings.RightTriggerMode);

        if (j.contains("LEDandATunavailableUDP"))
            j.at("LEDandATunavailableUDP").get_to(strings.LEDandATunavailableUDP);

        if (j.contains("HapticsUnavailable"))
            j.at("HapticsUnavailable").get_to(strings.HapticsUnavailable);

        if (j.contains("X360emu"))
            j.at("X360emu").get_to(strings.X360emu);

        if (j.contains("DS4emu"))
            j.at("DS4emu").get_to(strings.DS4emu);

        if (j.contains("STOPemu"))
            j.at("STOPemu").get_to(strings.STOPemu);

        if (j.contains("ControllerEmuUserMode"))
            j.at("ControllerEmuUserMode").get_to(strings.ControllerEmuUserMode);

        if (j.contains("ControllerEmuAppAsAdmin"))
            j.at("ControllerEmuAppAsAdmin").get_to(strings.ControllerEmuAppAsAdmin);

        if (j.contains("Language"))
            j.at("Language").get_to(strings.Language);

        if (j.contains("Sensitivity"))
            j.at("Sensitivity").get_to(strings.Sensitivity);

        if (j.contains("GeneralData"))
            j.at("GeneralData").get_to(strings.GeneralData);

        if (j.contains("TouchPacketNum"))
            j.at("TouchPacketNum").get_to(strings.TouchPacketNum);

        if (j.contains("Touch"))
            j.at("Touch").get_to(strings.Touch);

        if (j.contains("MicButton"))
            j.at("MicButton").get_to(strings.MicButton);

        if (j.contains("TouchpadToMouse"))
            j.at("TouchpadToMouse").get_to(strings.TouchpadToMouse);

        if (j.contains("Tooltip_TouchpadToMouse"))
            j.at("Tooltip_TouchpadToMouse").get_to(strings.Tooltip_TouchpadToMouse);

        if (j.contains("StandardRumble"))
            j.at("StandardRumble").get_to(strings.StandardRumble);

        if (j.contains("LED_DS4_Unavailable"))
            j.at("LED_DS4_Unavailable").get_to(strings.LED_DS4_Unavailable);

        if (j.contains("UDPStatus"))
            j.at("UDPStatus").get_to(strings.UDPStatus);

        if (j.contains("Active"))
            j.at("Active").get_to(strings.Active);

        if (j.contains("Inactive"))
            j.at("Inactive").get_to(strings.Inactive);

        if (j.contains("RumbleToAT"))
            j.at("RumbleToAT").get_to(strings.RumbleToAT);
        if (j.contains("SwapTriggersRumbleToAT"))
            j.at("SwapTriggersRumbleToAT").get_to(strings.SwapTriggersRumbleToAT);
        if (j.contains("MaxLeftIntensity"))
            j.at("MaxLeftIntensity").get_to(strings.MaxLeftIntensity);
        if (j.contains("MaxLeftFrequency"))
            j.at("MaxLeftFrequency").get_to(strings.MaxLeftFrequency);
       if (j.contains("MaxRightIntensity"))
            j.at("MaxRightIntensity").get_to(strings.MaxRightIntensity);
        if (j.contains("MaxRightFrequency"))
            j.at("MaxRightFrequency").get_to(strings.MaxRightFrequency);
        if (j.contains("LedSection"))
            j.at("LedSection").get_to(strings.LedSection);
        if (j.contains("AudioToLED"))
            j.at("AudioToLED").get_to(strings.AudioToLED);
        if (j.contains("DiscoMode"))
            j.at("DiscoMode").get_to(strings.DiscoMode);
        if (j.contains("BatteryLightbar"))
            j.at("BatteryLightbar").get_to(strings.BatteryLightbar);
        if (j.contains("LED_Red"))
            j.at("LED_Red").get_to(strings.LED_Red);
        if (j.contains("LED_Green"))
            j.at("LED_Green").get_to(strings.LED_Green);
        if (j.contains("LED_Blue"))
            j.at("LED_Blue").get_to(strings.LED_Blue);
        if (j.contains("AdaptiveTriggers"))
            j.at("AdaptiveTriggers").get_to(strings.AdaptiveTriggers);
        if (j.contains("HapticFeedback"))
            j.at("HapticFeedback").get_to(strings.HapticFeedback);
        if (j.contains("LeftMotor"))
            j.at("LeftMotor").get_to(strings.LeftMotor);
        if (j.contains("RightMotor"))
            j.at("RightMotor").get_to(strings.RightMotor);
        if (j.contains("StartAudioToHaptics"))
            j.at("StartAudioToHaptics").get_to(strings.StartAudioToHaptics);
        if (j.contains("TakeScreenshot"))
            j.at("TakeScreenshot").get_to(strings.TakeScreenshot);
        if (j.contains("RealMicFunctionality"))
            j.at("RealMicFunctionality").get_to(strings.RealMicFunctionality);
        if (j.contains("SwapTriggersShortcut"))
            j.at("SwapTriggersShortcut").get_to(strings.SwapTriggersShortcut);
        if (j.contains("X360Shortcut"))
            j.at("X360Shortcut").get_to(strings.X360Shortcut);
        if (j.contains("DS4Shortcut"))
            j.at("DS4Shortcut").get_to(strings.DS4Shortcut);
        if (j.contains("StopEmuShortcut"))
            j.at("StopEmuShortcut").get_to(strings.StopEmuShortcut);
        if (j.contains("EmulationHeader"))
            j.at("EmulationHeader").get_to(strings.EmulationHeader);
        if (j.contains("ConfigHeader"))
            j.at("ConfigHeader").get_to(strings.ConfigHeader);
        if (j.contains("SaveConfig"))
            j.at("SaveConfig").get_to(strings.SaveConfig);
        if (j.contains("LoadConfig"))
            j.at("LoadConfig").get_to(strings.LoadConfig);
        if (j.contains("SetDefaultConfig"))
            j.at("SetDefaultConfig").get_to(strings.SetDefaultConfig);
        if (j.contains("RemoveDefaultConfig"))
            j.at("RemoveDefaultConfig").get_to(strings.RemoveDefaultConfig);
        if (j.contains("RunAsAdmin"))
            j.at("RunAsAdmin").get_to(strings.RunAsAdmin);
        if (j.contains("HideToTray"))
            j.at("HideToTray").get_to(strings.HideToTray);
        if (j.contains("HideWindowOnStartup"))
            j.at("HideWindowOnStartup").get_to(strings.HideWindowOnStartup);
        if (j.contains("RunWithWindows"))
            j.at("RunWithWindows").get_to(strings.RunWithWindows);

        if (j.contains("Tooltip_RumbleToAT"))
            j.at("Tooltip_RumbleToAT").get_to(strings.Tooltip_RumbleToAT);
        if (j.contains("Tooltip_RumbleToAT"))
            j.at("Tooltip_RumbleToAT").get_to(strings.Tooltip_RumbleToAT);
        if (j.contains("Tooltip_SwapTriggersRumbleToAT"))
            j.at("Tooltip_SwapTriggersRumbleToAT")
                .get_to(strings.Tooltip_SwapTriggersRumbleToAT);
        if (j.contains("Tooltip_MaxIntensity"))
            j.at("Tooltip_MaxIntensity").get_to(strings.Tooltip_MaxIntensity);
        if (j.contains("Tooltip_MaxFrequency"))
            j.at("Tooltip_MaxFrequency").get_to(strings.Tooltip_MaxFrequency);
        if (j.contains("Tooltip_AudioToLED"))
            j.at("Tooltip_AudioToLED").get_to(strings.Tooltip_AudioToLED);
        if (j.contains("Tooltip_DiscoMode"))
            j.at("Tooltip_DiscoMode").get_to(strings.Tooltip_DiscoMode);
        if (j.contains("Tooltip_BatteryLightbar"))
            j.at("Tooltip_BatteryLightbar")
                .get_to(strings.Tooltip_BatteryLightbar);
        if (j.contains("Tooltip_HapticFeedback"))
            j.at("Tooltip_HapticFeedback")
                .get_to(strings.Tooltip_HapticFeedback);
        if (j.contains("Tooltip_StartAudioToHaptics"))
            j.at("Tooltip_StartAudioToHaptics")
                .get_to(strings.Tooltip_StartAudioToHaptics);
        if (j.contains("Tooltip_TakeScreenshot"))
            j.at("Tooltip_TakeScreenshot")
                .get_to(strings.Tooltip_TakeScreenshot);
        if (j.contains("Tooltip_RealMicFunctionality"))
            j.at("Tooltip_RealMicFunctionality")
                .get_to(strings.Tooltip_RealMicFunctionality);
        if (j.contains("Tooltip_SwapTriggersShortcut"))
            j.at("Tooltip_SwapTriggersShortcut")
                .get_to(strings.Tooltip_SwapTriggersShortcut);
        if (j.contains("Tooltip_X360Shortcut"))
            j.at("Tooltip_X360Shortcut").get_to(strings.Tooltip_X360Shortcut);
        if (j.contains("Tooltip_DS4Shortcut"))
            j.at("Tooltip_DS4Shortcut").get_to(strings.Tooltip_DS4Shortcut);
        if (j.contains("Tooltip_StopEmuShortcut"))
            j.at("Tooltip_StopEmuShortcut")
                .get_to(strings.Tooltip_StopEmuShortcut);
        if (j.contains("Tooltip_SaveConfig"))
            j.at("Tooltip_SaveConfig").get_to(strings.Tooltip_SaveConfig);
        if (j.contains("Tooltip_LoadConfig"))
            j.at("Tooltip_LoadConfig").get_to(strings.Tooltip_LoadConfig);
        if (j.contains("Tooltip_SetDefaultConfig"))
            j.at("Tooltip_SetDefaultConfig")
                .get_to(strings.Tooltip_SetDefaultConfig);
        if (j.contains("Tooltip_RemoveDefaultConfig"))
            j.at("Tooltip_RemoveDefaultConfig")
                .get_to(strings.Tooltip_RemoveDefaultConfig);
        if (j.contains("Tooltip_RunAsAdmin"))
            j.at("Tooltip_RunAsAdmin").get_to(strings.Tooltip_RunAsAdmin);
        if (j.contains("Tooltip_HideToTray"))
            j.at("Tooltip_HideToTray").get_to(strings.Tooltip_HideToTray);
        if (j.contains("Tooltip_HideWindowOnStartup"))
            j.at("Tooltip_HideWindowOnStartup")
                .get_to(strings.Tooltip_HideWindowOnStartup);
        if (j.contains("Tooltip_RunWithWindows"))
            j.at("Tooltip_RunWithWindows")
                .get_to(strings.Tooltip_RunWithWindows);

        return strings;
    }
};

static Strings ReadStringsFromFile(std::string language)
{ // example "pl", "en"
    Strings strings;
    nlohmann::json j;
    std::string fileLocation = MyUtils::GetExecutableFolderPath() +
                               "\\localizations\\" + language + ".json";
    ifstream file(fileLocation);

    if (file.is_open())
    {
        j = j.parse(file);
        file.close();
        strings = strings.from_json(j);
        return strings;
    }

    return strings;
}
