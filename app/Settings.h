#pragma once

class Settings
{
public:
    DualsenseUtils::InputFeatures ControllerInput;
    bool AudioToLED = false;
    bool DiscoMode = false;
    int lrumble = 0;
    int rrumble = 0;
    const char *lmodestr = "Off";
    const char *rmodestr = "Off";
    bool UseUDP = false;
    EmuStatus emuStatus = None;
};
