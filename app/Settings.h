#pragma once
#include <nlohmann/json.hpp>

class Settings
{
public:
    DualsenseUtils::InputFeatures ControllerInput;
    bool AudioToLED = false;
    bool DiscoMode = false;
    int lrumble = 0;
    int rrumble = 0;
    string lmodestr = "Off";
    string rmodestr = "Off";
    bool UseUDP = false;
    bool CurrentlyUsingUDP = false;
    bool MicScreenshot = false;
    bool MicFunc = false;
    bool RumbleToAT = false;
    bool BatteryLightbar = false;
    bool TouchpadToMouse = false;
    bool X360Shortcut = false;
    bool DS4Shortcut = false;
    bool StopEmuShortcut = false;
    float swipeThreshold = 0.50f;
    EmuStatus emuStatus = None;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["ControllerInput"] = ControllerInput.to_json();
        j["AudioToLED"] = AudioToLED;
        j["DiscoMode"] = DiscoMode;
        j["lrumble"] = lrumble;
        j["rrumble"] = rrumble;
        j["lmodestr"] = lmodestr;
        j["rmodestr"] = rmodestr;
        j["UseUDP"] = UseUDP;
        j["MicScreenshot"] = MicScreenshot;
        j["MicFunc"] = MicFunc;
        j["RumbleToAT"] = RumbleToAT;
        j["BatteryLightbar"] = BatteryLightbar;
        j["swipeThreshold"] = swipeThreshold;
        j["TouchpadToMouse"] = TouchpadToMouse;
        j["X360Shortcut"] = X360Shortcut;
        j["DS4Shortcut"] = DS4Shortcut;
        j["StopEmuShortcut"] = StopEmuShortcut;
        j["emuStatus"] = static_cast<int>(emuStatus); // Assuming EmuStatus is an enum
        return j;
    }

    static Settings from_json(const nlohmann::json& j) {
        Settings settings;

        // Parse ControllerInput first
        if (j.contains("ControllerInput")) settings.ControllerInput = DualsenseUtils::InputFeatures::from_json(j.at("ControllerInput"));

        if (j.contains("AudioToLED"))       j.at("AudioToLED").get_to(settings.AudioToLED);
        if (j.contains("DiscoMode"))        j.at("DiscoMode").get_to(settings.DiscoMode);
        if (j.contains("lrumble"))          j.at("lrumble").get_to(settings.lrumble);
        if (j.contains("rrumble"))          j.at("rrumble").get_to(settings.rrumble);
        if (j.contains("lmodestr"))         j.at("lmodestr").get_to(settings.lmodestr);
        if (j.contains("rmodestr"))         j.at("rmodestr").get_to(settings.rmodestr);
        if (j.contains("UseUDP"))           j.at("UseUDP").get_to(settings.UseUDP);
        if (j.contains("MicScreenshot"))    j.at("MicScreenshot").get_to(settings.MicScreenshot);
        if (j.contains("MicFunc"))          j.at("MicFunc").get_to(settings.MicFunc);
        if (j.contains("RumbleToAT"))          j.at("RumbleToAT").get_to(settings.RumbleToAT);
        if (j.contains("BatteryLightbar"))          j.at("BatteryLightbar").get_to(settings.BatteryLightbar);
        if (j.contains("swipeThreshold"))          j.at("swipeThreshold").get_to(settings.swipeThreshold);
        if (j.contains("TouchpadToMouse"))          j.at("TouchpadToMouse").get_to(settings.TouchpadToMouse);
        if (j.contains("X360Shortcut"))          j.at("X360Shortcut").get_to(settings.X360Shortcut);
        if (j.contains("DS4Shortcut"))          j.at("DS4Shortcut").get_to(settings.DS4Shortcut);
        if (j.contains("StopEmuShortcut"))          j.at("StopEmuShortcut").get_to(settings.StopEmuShortcut);
        if (j.contains("emuStatus"))        j.at("emuStatus").get_to(settings.emuStatus);

        return settings;
    }
};
