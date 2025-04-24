#include "Settings.h"
#include <cstring>

Settings::Settings() {
    std::memset(L2Feedback, 0, sizeof(L2Feedback));
    std::memset(R2Feedback, 0, sizeof(R2Feedback));
    std::memset(L2WeaponFeedback, 0, sizeof(L2WeaponFeedback));
    std::memset(R2WeaponFeedback, 0, sizeof(R2WeaponFeedback));
    std::memset(L2VibrationFeedback, 0, sizeof(L2VibrationFeedback));
    std::memset(R2VibrationFeedback, 0, sizeof(R2VibrationFeedback));
    std::memset(L2SlopeFeedback, 0, sizeof(L2SlopeFeedback));
    std::memset(R2SlopeFeedback, 0, sizeof(R2SlopeFeedback));
    std::memset(L2MultiplePositionFeedback, 0, sizeof(L2MultiplePositionFeedback));
    std::memset(R2MultiplePositionFeedback, 0, sizeof(R2MultiplePositionFeedback));
    std::memset(L2MultipleVibrationFeedback, 0, sizeof(L2MultipleVibrationFeedback));
    std::memset(R2MultipleVibrationFeedback, 0, sizeof(R2MultipleVibrationFeedback));
    std::memset(L2DSXForces, 0, sizeof(L2DSXForces));
    std::memset(R2DSXForces, 0, sizeof(R2DSXForces));

    AudioToLED = false;
    DiscoMode = false;
    lrumble = 0;
    rrumble = 0;
    lmodestr = "Off";
    rmodestr = "Off";
    lmodestrSony = "Off";
    rmodestrSony = "Off";
    UseUDP = false;
    CurrentlyUsingUDP = false;
    MicScreenshot = false;
    MicFunc = false;
    RumbleToAT = false;
    RumbleToAT_RigidMode = false;
    BatteryLightbar = false;
    TouchpadToMouse = false;
    X360Shortcut = false;
    DS4Shortcut = false;
    TouchpadToMouseShortcut = false;
    StopEmuShortcut = false;
    SwapTriggersRumbleToAT = false;
    MaxLeftRumbleToATIntensity = 255;
    MaxLeftRumbleToATFrequency = 25;
    MaxRightRumbleToATIntensity = 255;
    MaxRightRumbleToATFrequency = 25;
    MaxLeftTriggerRigid = 255;
    MaxRightTriggerRigid = 255;
    DualShock4V2 = false;
    TouchpadAsSelectStart = false;
    TouchpadAsSelect = false;
    TouchpadAsStart = false;
    SwapTriggersShortcut = false;
    UseHeadset = false;
    swipeThreshold = 0.50f;
    HapticsToTriggers = false;
    LeftAnalogDeadzone = 0;
    RightAnalogDeadzone = 0;
    TriggersAsButtons = false;
    AudioToTriggersMaxFloatValue = 5.0f;
    QUIET_THRESHOLD = 85;
    MEDIUM_THRESHOLD = 170;
    QUIET_COLOR[0] = 10; QUIET_COLOR[1] = 10; QUIET_COLOR[2] = 10;
    MEDIUM_COLOR[0] = 125; MEDIUM_COLOR[1] = 125; MEDIUM_COLOR[2] = 125;
    LOUD_COLOR[0] = 255; LOUD_COLOR[1] = 255; LOUD_COLOR[2] = 255;
    ScreenshotSoundLoudness = 1.0f;
    DiscoSpeed = 1;
    DisablePlayerLED = false;
    RunAudioToHapticsOnStart = false;
    WasAudioToHapticsRan = false;
    WasAudioToHapticsCheckboxChecked = false;
    HapticsToLED = false;
    SpeakerToLED = false;
    HapticsAndSpeakerToLedScale = 0.5f;
    OverrideDS4Lightbar = false;
    TouchpadToRXRY = false;
    FullyRetractTriggers = true;
    TouchpadToHaptics = false;
    TouchpadToHapticsFrequency = 20.0f;
    GyroToMouse = false;
    GyroToMouseSensitivity = 0.010f;
    GyroToRightAnalogStick = false;
    L2Threshold = 100;
    GyroToRightAnalogStickSensitivity = 5.0f;
    GyroToRightAnalogStickDeadzone = 0.005f;
    GyroToRightAnalogStickMinimumStickValue = 64;
    StopWriting = false;
    StopWritingShortcut = false;
    L2Deadzone = 0;
    R2Deadzone = 0;
    L2UDPDeadzone = 0;
    R2UDPDeadzone = 0;
    GyroToMouseShortcut = false;
    GyroToRightAnalogStickShortcut = false;
    R2ToMouseClick = false;
    MaxLeftMotor = 255;
    MaxRightMotor = 255;
    curTrigger = "L2";
    triggerFormat = "Sony";
    LeftStickToMouse = false;
    RightStickToMouse = false;
    LeftStickToMouseSensitivity = 0.03f;
    DisconnectControllerShortcut = false;
    colorPickerColor[0] = 0; colorPickerColor[1] = 0; colorPickerColor[2] = 0;

    CurrentHapticFile = "";
    DontPlayIfAlreadyPlaying = false;
    Loop = false;

    emuStatus = None;
}

nlohmann::json Settings::to_json() const {
    nlohmann::json j;
    j["ControllerInput"] = ControllerInput.to_json();

    // Use the macro to serialize members
    #define X(name) j[#name] = name;
    SETTINGS_JSON_MEMBERS
    #undef X

    return j;
}

Settings Settings::from_json(const nlohmann::json& j) {
    Settings settings; // Default constructor initializes all members

    if (j.contains("ControllerInput")) {
        settings.ControllerInput = DualsenseUtils::InputFeatures::from_json(j.at("ControllerInput"));
    }

    // Use the macro to deserialize members
    #define X(name) if (j.contains(#name)) j.at(#name).get_to(settings.name);
    SETTINGS_JSON_MEMBERS
    #undef X

    return settings;
}
