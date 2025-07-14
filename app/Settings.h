#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "DualSense.h"
#include "ControllerEmulation.h"

enum AudioEditType {
    Pitch,
    Volume,
    Stop,
    StopAll,
};

struct AudioEdit {
    std::string File;
    AudioEditType EditType;
    float Value;
};

struct AudioPlay {
    std::string File;
    bool DontPlayIfAlreadyPlaying;
    bool Loop;
};

#define SETTINGS_JSON_MEMBERS \
    X(R2Feedback) \
    X(R2WeaponFeedback) \
    X(R2VibrationFeedback) \
    X(R2SlopeFeedback) \
    X(R2MultiplePositionFeedback) \
    X(R2MultipleVibrationFeedback) \
    X(R2DSXForces) \
    X(L2Feedback) \
    X(L2WeaponFeedback) \
    X(L2VibrationFeedback) \
    X(L2SlopeFeedback) \
    X(L2MultiplePositionFeedback) \
    X(L2MultipleVibrationFeedback) \
    X(L2DSXForces) \
    X(MaxLeftMotor) \
    X(triggerFormat) \
    X(MaxRightMotor) \
    X(R2ToMouseClick) \
    X(GyroToRightAnalogStickShortcut) \
    X(GyroToMouseShortcut) \
    X(L2Deadzone) \
    X(R2Deadzone) \
    X(StopWritingShortcut) \
    X(StopWriting) \
    X(GyroToRightAnalogStickSensitivity) \
    X(GyroToRightAnalogStickDeadzone) \
    X(GyroToRightAnalogStickMinimumStickValue) \
    X(GyroToMouseSensitivity) \
    X(GyroToMouse) \
    X(GyroToRightAnalogStick) \
    X(L2Threshold) \
    X(TouchpadToHaptics) \
    X(TouchpadToHapticsFrequency) \
    X(FullyRetractTriggers) \
    X(OverrideDS4Lightbar) \
    X(TouchpadToRXRY) \
    X(HapticsAndSpeakerToLedScale) \
    X(SpeakerToLED) \
    X(HapticsToLED) \
    X(RunAudioToHapticsOnStart) \
    X(DisablePlayerLED) \
    X(DiscoSpeed) \
    X(ScreenshotSoundLoudness) \
    X(QUIET_COLOR) \
    X(MEDIUM_COLOR) \
    X(LOUD_COLOR) \
    X(TouchpadAsSelectStart) \
    X(TouchpadAsSelect) \
    X(TouchpadAsStart) \
    X(UseHeadset) \
    X(TouchpadToMouseShortcut) \
    X(DualShock4V2) \
    X(LeftAnalogDeadzone) \
    X(RightAnalogDeadzone) \
    X(TriggersAsButtons) \
    X(AudioToLED) \
    X(DiscoMode) \
    X(lrumble) \
    X(rrumble) \
    X(lmodestr) \
    X(rmodestr) \
    X(lmodestrSony) \
    X(rmodestrSony) \
    X(UseUDP) \
    X(MicScreenshot) \
    X(MicFunc) \
    X(RumbleToAT) \
    X(RumbleToAT_RigidMode) \
    X(BatteryLightbar) \
    X(BatteryPlayerLed) \
    X(swipeThreshold) \
    X(TouchpadToMouse) \
    X(X360Shortcut) \
    X(DS4Shortcut) \
    X(StopEmuShortcut) \
    X(SwapTriggersRumbleToAT) \
    X(MaxLeftRumbleToATIntensity) \
    X(MaxLeftRumbleToATFrequency) \
    X(MaxRightRumbleToATIntensity) \
    X(MaxRightRumbleToATFrequency) \
    X(MaxLeftTriggerRigid) \
    X(MaxRightTriggerRigid) \
    X(SwapTriggersShortcut) \
    X(AudioToTriggersMaxFloatValue) \
    X(HapticsToTriggers) \
    X(LeftStickToMouse) \
    X(RightStickToMouse) \
    X(LeftStickToMouseSensitivity) \
    X(DisconnectControllerShortcut) \
    X(colorPickerColor) \
    X(emuStatus)

class Settings {
public:
    // Constructor
    Settings();

    // Member variables (declarations only)
    DualsenseUtils::InputFeatures ControllerInput;
    int L2Feedback[2];
    int R2Feedback[2];
    int L2WeaponFeedback[3];
    int R2WeaponFeedback[3];
    int L2VibrationFeedback[3];
    int R2VibrationFeedback[3];
    int L2SlopeFeedback[4];
    int R2SlopeFeedback[4];
    int L2MultiplePositionFeedback[10];
    int R2MultiplePositionFeedback[10];
    int L2MultipleVibrationFeedback[11];
    int R2MultipleVibrationFeedback[11];
    int L2DSXForces[11];
    int R2DSXForces[11];
    bool AudioToLED;
    bool DiscoMode;
    int lrumble;
    int rrumble;
    std::string lmodestr;
    std::string rmodestr;
    std::string lmodestrSony;
    std::string rmodestrSony;
    bool UseUDP;
    bool CurrentlyUsingUDP;
    bool MicScreenshot;
    bool MicFunc;
    bool RumbleToAT;
    bool RumbleToAT_RigidMode;
    bool BatteryLightbar;
    bool BatteryPlayerLed;
    bool TouchpadToMouse;
    bool X360Shortcut;
    bool DS4Shortcut;
    bool TouchpadToMouseShortcut;
    bool StopEmuShortcut;
    bool SwapTriggersRumbleToAT;
    int MaxLeftRumbleToATIntensity;
    int MaxLeftRumbleToATFrequency;
    int MaxRightRumbleToATIntensity;
    int MaxRightRumbleToATFrequency;
    int MaxLeftTriggerRigid;
    int MaxRightTriggerRigid;
    bool DualShock4V2;
    bool TouchpadAsSelectStart;
    bool TouchpadAsSelect;
    bool TouchpadAsStart;
    bool SwapTriggersShortcut;
    bool UseHeadset;
    float swipeThreshold;
    bool HapticsToTriggers;
    int LeftAnalogDeadzone;
    int RightAnalogDeadzone;
    bool TriggersAsButtons;
    float AudioToTriggersMaxFloatValue;
    int QUIET_THRESHOLD;
    int MEDIUM_THRESHOLD;
    int QUIET_COLOR[3];
    int MEDIUM_COLOR[3];
    int LOUD_COLOR[3];   
    float ScreenshotSoundLoudness;
    int DiscoSpeed;
    bool DisablePlayerLED;
    bool RunAudioToHapticsOnStart;
    bool WasAudioToHapticsRan;
    bool WasAudioToHapticsCheckboxChecked;
    bool HapticsToLED;
    bool SpeakerToLED;
    float HapticsAndSpeakerToLedScale;
    bool OverrideDS4Lightbar;
    bool TouchpadToRXRY;
    bool FullyRetractTriggers;
    bool TouchpadToHaptics;
    float TouchpadToHapticsFrequency;
    bool GyroToMouse;
    float GyroToMouseSensitivity;
    bool GyroToRightAnalogStick;
    int L2Threshold;
    float GyroToRightAnalogStickSensitivity;
    float GyroToRightAnalogStickDeadzone;
    int GyroToRightAnalogStickMinimumStickValue;
    bool StopWriting;
    bool StopWritingShortcut;
    int L2Deadzone;
    int R2Deadzone;
    int L2UDPDeadzone;
    int R2UDPDeadzone;
    bool GyroToMouseShortcut;
    bool GyroToRightAnalogStickShortcut;
    bool R2ToMouseClick;
    bool LeftStickToMouse;
    bool RightStickToMouse;
    int MaxLeftMotor;
    int MaxRightMotor;
    const char* curTrigger;
    std::string triggerFormat;
    float LeftStickToMouseSensitivity;
    bool DisconnectControllerShortcut;
    float colorPickerColor[3];

    // UDP Haptics (not saved to JSON)
    std::string CurrentHapticFile;
    bool DontPlayIfAlreadyPlaying;
    bool Loop;
    std::vector<AudioEdit> AudioEditQueue;
    std::vector<AudioPlay> AudioPlayQueue;

    EmuStatus emuStatus;

    // Method declarations
    nlohmann::json to_json() const;
    static Settings from_json(const nlohmann::json& j);
};
