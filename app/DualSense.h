#include <Propvarutil.h>
#include <chrono>
#include <hidapi.h>
#include <iostream>
#include <mmdeviceapi.h>
#include <thread>
#include <vector>
#pragma comment(lib, "Propsys.lib")
#include <atlbase.h>
#include <functiondiscoverykeys_devpkey.h>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <algorithm>
#include <array>
#include <cfgmgr32.h>
#include <cstdint>
#include <regex>
#include <setupapi.h>
#include <sstream>
#include <string>
#include <usbiodef.h>
#include <nlohmann/json.hpp>

using namespace std;

// get-parent-device -> https://github.com/mkielar/get-parent-device/blob/master/get-parent-device/main.cpp
constexpr int ERR_BAD_ARGUMENTS = 1;
constexpr int ERR_NO_DEVICES_FOUND = 2;
constexpr int ERR_NO_DEVICE_INFO = 3;
constexpr wchar_t GUID_DISK_DRIVE_STRING[] =
    L"{21EC2020-3AEA-1069-A2DD-08002B30309D}";
BOOL GetParentDeviceInstanceId(_Out_ std::wstring &parentDeviceInstanceId,
                               _Out_ DEVINST &parentDeviceInstanceIdHandle,
                               _In_ DEVINST currentDeviceInstanceId)
{
    CONFIGRET result = CM_Get_Parent(&parentDeviceInstanceIdHandle,
                                     currentDeviceInstanceId,
                                     0);
    if (result == CR_SUCCESS)
    {
        parentDeviceInstanceId.clear();
        parentDeviceInstanceId.resize(MAX_DEVICE_ID_LEN);
        result = CM_Get_Device_IDW(parentDeviceInstanceIdHandle,
                                   (PWSTR)parentDeviceInstanceId.data(),
                                   MAX_DEVICE_ID_LEN,
                                   0);
        if (result == CR_SUCCESS)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// Tests whether given Device Instance ID matches the pattern.
BOOL DeviceIdMatchesPattern(const std::wstring &deviceInstanceId,
                            const std::wstring &pattern)
{
    std::wregex regexPattern(pattern);
    return std::regex_match(deviceInstanceId, regexPattern);
}

// Searches for a parent device with a matching Device Instance ID pattern
int FindParentDeviceInstanceId(
    const std::wstring &searchedDeviceInstanceId,
    const std::wstring &parentDeviceInstanceIdPattern,
    std::wstring &foundDeviceInstanceId)
{
    GUID diskDriveGuid;
    CLSIDFromString(GUID_DISK_DRIVE_STRING, &diskDriveGuid);

    HDEVINFO devInfo = SetupDiGetClassDevs(&diskDriveGuid,
                                           NULL,
                                           NULL,
                                           DIGCF_PRESENT | DIGCF_ALLCLASSES);
    std::wstring deviceInstanceId(MAX_DEVICE_ID_LEN, L'\0');

    if (devInfo != INVALID_HANDLE_VALUE)
    {
        DWORD devIndex = 0;
        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        while (SetupDiEnumDeviceInfo(devInfo, devIndex, &devInfoData))
        {
            std::fill(deviceInstanceId.begin(), deviceInstanceId.end(), L'\0');
            SetupDiGetDeviceInstanceIdW(devInfo,
                                        &devInfoData,
                                        (PWSTR)deviceInstanceId.data(),
                                        MAX_PATH,
                                        0);

            if (_wcsicmp(searchedDeviceInstanceId.c_str(),
                         deviceInstanceId.c_str()) == 0)
            {
                DEVINST currentDeviceInstanceId = devInfoData.DevInst;
                DEVINST parentDeviceInstanceId = 0;
                std::wstring parentDeviceInstanceIdStr;

                while (true)
                {
                    std::fill(deviceInstanceId.begin(),
                              deviceInstanceId.end(),
                              L'\0');
                    parentDeviceInstanceId = 0;
                    if (GetParentDeviceInstanceId(parentDeviceInstanceIdStr,
                                                  parentDeviceInstanceId,
                                                  currentDeviceInstanceId))
                    {
                        if (DeviceIdMatchesPattern(
                                parentDeviceInstanceIdStr,
                                parentDeviceInstanceIdPattern))
                        {
                            foundDeviceInstanceId =
                                parentDeviceInstanceIdStr; // Found ID
                            return 0;
                        }
                        currentDeviceInstanceId = parentDeviceInstanceId;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            devIndex++;
        }

        if (devIndex == 0)
        {
            return ERR_NO_DEVICES_FOUND;
        }
    }
    else
    {
        return ERR_NO_DEVICE_INFO;
    }
    return ERR_NO_DEVICES_FOUND;
}

#define MAX_READ_LENGTH 64
#define MAX_USB_WRITE_LENGTH 48
#define MAX_BT_WRITE_LENGTH 78

const UINT32 hashTable[256] =
    { // Hash table from -> https://github.com/Ohjurot/DualSense-Windows/blob/main/VS19_Solution/DualSenseWindows/src/DualSenseWindows/DS_CRC32.cpp
        0xd202ef8d, 0xa505df1b, 0x3c0c8ea1, 0x4b0bbe37, 0xd56f2b94, 0xa2681b02,
        0x3b614ab8, 0x4c667a2e, 0xdcd967bf, 0xabde5729, 0x32d70693, 0x45d03605,
        0xdbb4a3a6, 0xacb39330, 0x35bac28a, 0x42bdf21c, 0xcfb5ffe9, 0xb8b2cf7f,
        0x21bb9ec5, 0x56bcae53, 0xc8d83bf0, 0xbfdf0b66, 0x26d65adc, 0x51d16a4a,
        0xc16e77db, 0xb669474d, 0x2f6016f7, 0x58672661, 0xc603b3c2, 0xb1048354,
        0x280dd2ee, 0x5f0ae278, 0xe96ccf45, 0x9e6bffd3, 0x762ae69,  0x70659eff,
        0xee010b5c, 0x99063bca, 0xf6a70,    0x77085ae6, 0xe7b74777, 0x90b077e1,
        0x9b9265b,  0x7ebe16cd, 0xe0da836e, 0x97ddb3f8, 0xed4e242,  0x79d3d2d4,
        0xf4dbdf21, 0x83dcefb7, 0x1ad5be0d, 0x6dd28e9b, 0xf3b61b38, 0x84b12bae,
        0x1db87a14, 0x6abf4a82, 0xfa005713, 0x8d076785, 0x140e363f, 0x630906a9,
        0xfd6d930a, 0x8a6aa39c, 0x1363f226, 0x6464c2b0, 0xa4deae1d, 0xd3d99e8b,
        0x4ad0cf31, 0x3dd7ffa7, 0xa3b36a04, 0xd4b45a92, 0x4dbd0b28, 0x3aba3bbe,
        0xaa05262f, 0xdd0216b9, 0x440b4703, 0x330c7795, 0xad68e236, 0xda6fd2a0,
        0x4366831a, 0x3461b38c, 0xb969be79, 0xce6e8eef, 0x5767df55, 0x2060efc3,
        0xbe047a60, 0xc9034af6, 0x500a1b4c, 0x270d2bda, 0xb7b2364b, 0xc0b506dd,
        0x59bc5767, 0x2ebb67f1, 0xb0dff252, 0xc7d8c2c4, 0x5ed1937e, 0x29d6a3e8,
        0x9fb08ed5, 0xe8b7be43, 0x71beeff9, 0x6b9df6f,  0x98dd4acc, 0xefda7a5a,
        0x76d32be0, 0x1d41b76,  0x916b06e7, 0xe66c3671, 0x7f6567cb, 0x862575d,
        0x9606c2fe, 0xe101f268, 0x7808a3d2, 0xf0f9344,  0x82079eb1, 0xf500ae27,
        0x6c09ff9d, 0x1b0ecf0b, 0x856a5aa8, 0xf26d6a3e, 0x6b643b84, 0x1c630b12,
        0x8cdc1683, 0xfbdb2615, 0x62d277af, 0x15d54739, 0x8bb1d29a, 0xfcb6e20c,
        0x65bfb3b6, 0x12b88320, 0x3fba6cad, 0x48bd5c3b, 0xd1b40d81, 0xa6b33d17,
        0x38d7a8b4, 0x4fd09822, 0xd6d9c998, 0xa1def90e, 0x3161e49f, 0x4666d409,
        0xdf6f85b3, 0xa868b525, 0x360c2086, 0x410b1010, 0xd80241aa, 0xaf05713c,
        0x220d7cc9, 0x550a4c5f, 0xcc031de5, 0xbb042d73, 0x2560b8d0, 0x52678846,
        0xcb6ed9fc, 0xbc69e96a, 0x2cd6f4fb, 0x5bd1c46d, 0xc2d895d7, 0xb5dfa541,
        0x2bbb30e2, 0x5cbc0074, 0xc5b551ce, 0xb2b26158, 0x4d44c65,  0x73d37cf3,
        0xeada2d49, 0x9ddd1ddf, 0x3b9887c,  0x74beb8ea, 0xedb7e950, 0x9ab0d9c6,
        0xa0fc457,  0x7d08f4c1, 0xe401a57b, 0x930695ed, 0xd62004e,  0x7a6530d8,
        0xe36c6162, 0x946b51f4, 0x19635c01, 0x6e646c97, 0xf76d3d2d, 0x806a0dbb,
        0x1e0e9818, 0x6909a88e, 0xf000f934, 0x8707c9a2, 0x17b8d433, 0x60bfe4a5,
        0xf9b6b51f, 0x8eb18589, 0x10d5102a, 0x67d220bc, 0xfedb7106, 0x89dc4190,
        0x49662d3d, 0x3e611dab, 0xa7684c11, 0xd06f7c87, 0x4e0be924, 0x390cd9b2,
        0xa0058808, 0xd702b89e, 0x47bda50f, 0x30ba9599, 0xa9b3c423, 0xdeb4f4b5,
        0x40d06116, 0x37d75180, 0xaede003a, 0xd9d930ac, 0x54d13d59, 0x23d60dcf,
        0xbadf5c75, 0xcdd86ce3, 0x53bcf940, 0x24bbc9d6, 0xbdb2986c, 0xcab5a8fa,
        0x5a0ab56b, 0x2d0d85fd, 0xb404d447, 0xc303e4d1, 0x5d677172, 0x2a6041e4,
        0xb369105e, 0xc46e20c8, 0x72080df5, 0x50f3d63,  0x9c066cd9, 0xeb015c4f,
        0x7565c9ec, 0x262f97a,  0x9b6ba8c0, 0xec6c9856, 0x7cd385c7, 0xbd4b551,
        0x92dde4eb, 0xe5dad47d, 0x7bbe41de, 0xcb97148,  0x95b020f2, 0xe2b71064,
        0x6fbf1d91, 0x18b82d07, 0x81b17cbd, 0xf6b64c2b, 0x68d2d988, 0x1fd5e91e,
        0x86dcb8a4, 0xf1db8832, 0x616495a3, 0x1663a535, 0x8f6af48f, 0xf86dc419,
        0x660951ba, 0x110e612c, 0x88073096, 0xff000000};

const UINT32 crcSeed = 0xeada2d49;

UINT32 compute(unsigned char *buffer, size_t len)
{
    UINT32 result = crcSeed;

    for (size_t i = 0; i < len; i++)
    {
        result =
            hashTable[((unsigned char)result) ^ ((unsigned char)buffer[i])] ^
            (result >> 8);
    }

    return result;
}

class Touchpad
{
public:
    int RawTrackingNum;
    bool IsActive;
    int ID;
    int X;
    int Y;

    Touchpad()
    {
        RawTrackingNum = 0;
        IsActive = false;
        ID = 0;
        X = 0;
        Y = 0;
    }
};

class Gyro
{
public:
    int X;
    int Y;
    int Z;

    Gyro()
    {
        X = 0;
        Y = 0;
        Z = 0;
    }
};

class Accelerometer
{
public:
    int16_t X;
    int16_t Y;
    int16_t Z;
    uint32_t SensorTimestamp;

    Accelerometer()
    {
        X = 0;
        Y = 0;
        Z = 0;
        SensorTimestamp = 0;
    }
};

enum class BatteryState : uint8_t
{
    POWER_SUPPLY_STATUS_DISCHARGING = 0x0,
    POWER_SUPPLY_STATUS_CHARGING = 0x2,
    POWER_SUPPLY_STATUS_FULL = 0x1,
    POWER_SUPPLY_STATUS_NOT_CHARGING = 0xb,
    POWER_SUPPLY_STATUS_ERROR = 0xf,
    POWER_SUPPLY_TEMP_OR_VOLTAGE_OUT_OF_RANGE = 0xa,
    POWER_SUPPLY_STATUS_UNKNOWN = 0x0
};

struct Battery
{
    BatteryState State;
    int Level;

    Battery() : State(BatteryState::POWER_SUPPLY_STATUS_UNKNOWN), Level(0)
    {
    }
};

class ButtonState
{
public:
    bool square = false, triangle = false, circle = false, cross = false, 
     DpadUp = false, DpadDown = false, DpadLeft = false, DpadRight = false,
     L1 = false, L3 = false, R1 = false, R3 = false, R2Btn = false, 
     L2Btn = false, share = false, options = false, ps = false, 
     touchBtn = false, micBtn = false;
    uint8_t L2 = 0, R2 = 0;
    int RX = 0, RY = 0, LX = 0, LY = 0, TouchPacketNum = 0;
    std::string PathIdentifier = "";
    Touchpad trackPadTouch0;
    Touchpad trackPadTouch1;
    Gyro gyro;
    Accelerometer accelerometer;
    Battery battery;

    void SetDPadState(int dpad_state)
    {
        switch (dpad_state)
        {
        case 0:
            DpadUp = true;
            DpadDown = false;
            DpadLeft = false;
            DpadRight = false;
            break;

        case 1:
            DpadUp = true;
            DpadDown = false;
            DpadLeft = false;
            DpadRight = true;
            break;

        case 2:
            DpadUp = false;
            DpadDown = false;
            DpadLeft = false;
            DpadRight = true;
            break;

        case 3:
            DpadUp = false;
            DpadDown = true;
            DpadLeft = false;
            DpadRight = true;
            break;

        case 4:
            DpadUp = false;
            DpadDown = true;
            DpadLeft = false;
            DpadRight = false;
            break;

        case 5:
            DpadUp = false;
            DpadDown = true;
            DpadLeft = true;
            DpadRight = false;
            break;

        case 6:
            DpadUp = false;
            DpadDown = false;
            DpadLeft = true;
            DpadRight = false;
            break;

        case 7:
            DpadUp = true;
            DpadDown = false;
            DpadLeft = true;
            DpadRight = false;
            break;

        default:
            DpadUp = false;
            DpadDown = false;
            DpadLeft = false;
            DpadRight = false;
            break;
        }
    }
};

namespace Feature
{
enum MuteLight : uint8_t
{
    Off = 0,
    On,
    Breathing,
    DoNothing, // literally nothing, this input is ignored,
               // though it might be a faster blink in other versions
    NoAction4,
    NoAction5,
    NoAction6,
    NoAction7 = 7
};

enum class LightBrightness : uint8_t
{
    Bright = 0,
    Mid,
    Dim,
    BNoAction3,
    BNoAction4,
    BNoAction5,
    BNoAction6,
    BNoAction7 = 7
};

enum LightFadeAnimation : uint8_t
{
    Nothing = 0,
    FadeIn, // from black to blue
    FadeOut // from blue to black
};

enum RumbleMode : uint8_t
{
    StandardRumble = 0xFF,
    Haptic_Feedback = 0xFC
};

enum FeatureType : int8_t
{
    Full = 0x57,
    LetGo = 0x08
};

enum AudioOutput : uint8_t
{
    Headset = 0x05,
    Speaker = 0x31
};

enum MicrophoneLED : uint8_t
{
    Pulse = 0x2,
    MLED_On = 0x1,
    MLED_Off = 0x0
};

enum MicrophoneStatus : uint8_t
{
    MSTATUS_On = 0,
    MSTATUS_Off = 0x10
};

enum Brightness : uint8_t
{
    High = 0x0,
    Medium = 0x1,
    Low = 0x2
};

enum PlayerLED : uint8_t
{
    PLAYER_OFF = 0x0,
    PLAYER_1 = 0x01,
    PLAYER_2 = 0x02,
    PLAYER_3 = 0x03,
    PLAYER_4 = 0x04,
    PLAYER_5 = 0x05,
};

enum DeviceType : uint8_t
{
    DualSense = 0,
    DualSense_Edge = 1,
    //DualShock4 = 2,
};

enum ConnectionType : uint8_t
{
    BT = 0x0,
    USB = 0x1
};
}; // namespace Feature

namespace Trigger
{
enum TriggerMode : uint8_t
{
    Off = 0x0,
    Rigid = 0x1,
    Pulse = 0x2,
    Rigid_A = 0x1 | 0x20,
    Rigid_B = 0x1 | 0x04,
    Rigid_AB = 0x1 | 0x20 | 0x04,
    Pulse_A = 0x2 | 0x20,
    Pulse_B = 0x2 | 0x04,
    Pulse_AB = 0x2 | 0x20 | 0x04,
    Calibration = 0xFC
};
}

namespace DualsenseUtils
{
class InputFeatures
{
public:
    Feature::RumbleMode VibrationType = Feature::Haptic_Feedback;
    Feature::FeatureType Features = Feature::FeatureType::Full;
    uint8_t _RightMotor = 0;
    uint8_t _LeftMotor = 0;
    uint8_t SpeakerVolume = 100;
    int MicrophoneVolume = 80;
    Feature::AudioOutput audioOutput = Feature::Speaker;
    Feature::MicrophoneLED micLED = Feature::MicrophoneLED::MLED_Off;
    Feature::MicrophoneStatus micStatus = Feature::MicrophoneStatus::MSTATUS_On;
    Trigger::TriggerMode RightTriggerMode = Trigger::TriggerMode::Off;
    Trigger::TriggerMode LeftTriggerMode = Trigger::TriggerMode::Off;
    int RightTriggerForces[7] = {0};
    int LeftTriggerForces[7] = {0};
    Feature::Brightness _Brightness = Feature::Brightness::High;
    Feature::PlayerLED playerLED = Feature::PlayerLED::PLAYER_OFF;
    unsigned char PlayerLED_Bitmask = 0x0;
    int Red = 0;
    int Green = 0;
    int Blue = 0;
    string ID = ""; // Optional ID for arrays n' stuff

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["VibrationType"] = VibrationType;
        j["Features"] = Features;
        j["_RightMotor"] = _RightMotor;
        j["_LeftMotor"] = _LeftMotor;
        j["SpeakerVolume"] = SpeakerVolume;
        j["MicrophoneVolume"] = MicrophoneVolume;
        j["audioOutput"] = audioOutput;
        j["micLED"] = micLED;
        j["micStatus"] = micStatus;
        j["RightTriggerMode"] = RightTriggerMode;
        j["LeftTriggerMode"] = LeftTriggerMode;
        j["RightTriggerForces"] = std::vector<int>(std::begin(RightTriggerForces), std::end(RightTriggerForces));
        j["LeftTriggerForces"] = std::vector<int>(std::begin(LeftTriggerForces), std::end(LeftTriggerForces));
        j["_Brightness"] = _Brightness;
        j["playerLED"] = playerLED;
        j["PlayerLED_Bitmask"] = PlayerLED_Bitmask;
        j["Red"] = Red;
        j["Green"] = Green;
        j["Blue"] = Blue;
        j["ID"] = ID;
        return j;
    }

    static InputFeatures from_json(const nlohmann::json& j) {
        InputFeatures features;
        j.at("VibrationType").get_to(features.VibrationType);
        j.at("Features").get_to(features.Features);
        j.at("_RightMotor").get_to(features._RightMotor);
        j.at("_LeftMotor").get_to(features._LeftMotor);
        j.at("SpeakerVolume").get_to(features.SpeakerVolume);
        j.at("MicrophoneVolume").get_to(features.MicrophoneVolume);
        j.at("audioOutput").get_to(features.audioOutput);
        j.at("micLED").get_to(features.micLED);
        j.at("micStatus").get_to(features.micStatus);
        j.at("RightTriggerMode").get_to(features.RightTriggerMode);
        j.at("LeftTriggerMode").get_to(features.LeftTriggerMode);
        for (int i = 0; i < 7; ++i) {
            features.RightTriggerForces[i] = j.at("RightTriggerForces")[i];
            features.LeftTriggerForces[i] = j.at("LeftTriggerForces")[i];
        }
        j.at("_Brightness").get_to(features._Brightness);
        j.at("playerLED").get_to(features.playerLED);
        j.at("PlayerLED_Bitmask").get_to(features.PlayerLED_Bitmask);
        j.at("Red").get_to(features.Red);
        j.at("Green").get_to(features.Green);
        j.at("Blue").get_to(features.Blue);
        j.at("ID").get_to(features.ID);
        return features;
    }
};

int GetControllerCount()
{
    int i = 0;
    // DualSense
    hid_device_info *info = hid_enumerate(0x054c, 0x0ce6);
    hid_device_info *cur = info;

    while (cur)
    {
        cur = cur->next;
        i++;
    }

    // DualSense Edge
    hid_device_info *info2 = hid_enumerate(0x054c, 0x0df2);
    hid_device_info *cur2 = info2;

    while (cur2)
    {
        cur2 = cur2->next;
        i++;
    }

    hid_free_enumeration(cur);
    hid_free_enumeration(info);
    hid_free_enumeration(cur2);
    hid_free_enumeration(info2);

    return i;
}

std::vector<std::string> EnumerateControllerIDs()
{
    std::vector<std::string> v; // Use std::string to manage memory automatically
    hid_device_info *initial = hid_enumerate(0x054c, 0x0ce6);
    hid_device_info *initialEdge = hid_enumerate(0x054c, 0x0df2);

    // DualSense
    if (initial != NULL)
    {
        hid_device_info *current = initial; // Use a temporary pointer to iterate
        while (current)
        {
            // Copying path into std::string to ensure proper memory management
            v.push_back(current->path); // emplace_back constructs the string in place
            current = current->next;
        }
        hid_free_enumeration(initial); // Free the original head
    }

    // DualSense Edge
    if (initialEdge != NULL)
    {
        hid_device_info *currentEdge = initialEdge; // Use a temporary pointer for iteration
        while (currentEdge)
        {
            // Copying path into std::string to ensure proper memory management
            v.push_back(currentEdge->path); // emplace_back constructs the string in place
            currentEdge = currentEdge->next;
        }
        hid_free_enumeration(initialEdge); // Free the original head
    }

    return v;
}


enum Reconnect_Result
{
    Reconnected = 0,
    Fail = 1,
    No_Change = 2
};
} // namespace DualsenseUtils

std::wstring ctow(const char *src)
{
    return std::wstring(src, src + strlen(src));
}

std::string convertDeviceId(const std::string &originalId)
{
    // Check for the expected prefix
    const std::string prefix = "\\\\?\\HID#";
    if (originalId.find(prefix) != 0)
    {
        std::cerr << "Invalid format: " << originalId << std::endl;
        return "";
    }

    // Remove the prefix
    std::string trimmedId = originalId.substr(prefix.length());

    // Split the ID at '#' to separate the main components
    size_t hashPos = trimmedId.find('#');
    if (hashPos == std::string::npos)
    {
        std::cerr << "Invalid format: " << originalId << std::endl;
        return "";
    }

    // Extract the first part (e.g., VID and PID)
    std::string mainPart = trimmedId.substr(0, hashPos);

    // Split to get the VID and PID
    std::string vidPid = mainPart; // Keep the VID and PID part

    // Find the next '#' for the instance part
    size_t nextHashPos = trimmedId.find('#', hashPos + 1);
    std::string instancePart =
        trimmedId.substr(hashPos + 1, nextHashPos - hashPos - 1); // Instance ID

    // Extract the last part (e.g., the GUID)
    std::string guidPart;
    if (nextHashPos != std::string::npos)
    {
        guidPart = trimmedId.substr(nextHashPos);
    }

    // Reconstruct the new device ID
    std::string newDeviceId = "USB\\" + vidPid + "\\" + instancePart;

    return newDeviceId;
}

// Function to replace "USB" with "HID"
std::wstring replaceUSBwithHID(const std::wstring &input)
{
    std::wstring result;
    size_t length = input.length();
    for (size_t i = 0; i < length; ++i)
    {
        // Check for the substring "USB"
        if (input.substr(i, 3) == L"USB")
        {
            result += L"HID"; // Replace "USB" with "HID"
            i += 2;           // Skip the next two characters of "USB"
        }
        else
        {
            result += input[i]; // Add the character as it is
        }
    }
    return result;
}

bool compareDeviceIDs(const std::wstring &id1, const std::wstring &id2)
{
    std::wregex pattern(LR"(MI_\d{2}|&\d{3}|&\d{2}(&|$))");
    std::wregex zerozero(LR"(00)");
    std::wregex zerothree(LR"(03)");

    // Remove the unwanted parts from both strings
    std::wstring strippedId1 = std::regex_replace(id1, pattern, L"");
    std::wstring strippedId2 = std::regex_replace(id2, pattern, L"");

    // Remove last two numbers from both string
    strippedId1 = std::regex_replace(strippedId1, zerozero, L"");
    strippedId2 = std::regex_replace(strippedId2, zerothree, L"");

    // Compare the stripped IDs
    return strippedId1 == strippedId2;
}

class Dualsense
{
private:
    int offset = 0;
    uint8_t connectionType;
    bool Running = false;
    const wchar_t *DefaultError = L"Success";
    volatile const wchar_t *error = L"";
    int res = 0;
    std::string path = "";
    wstring lastKnownParent;
    hid_device *handle;
    bool bt_initialized = false;
    bool AudioInitialized = false;
    bool WasConnectedAtLeastOnce = false;
    bool DisconnectedByError = false;

    DualsenseUtils::InputFeatures CurSettings;

    // Audio variables
    ma_context context;
    ma_result result;
    ma_engine engine;
    ma_device device;

public:
    bool Connected = false;
    ButtonState State;
    Dualsense(std::string Path = "")
    {
        if (Path != "")
        {
            res = hid_init();

            if (res != 0)
            {
                std::cerr << "HIDAPI initialization failed!" << std::endl;
            }

            handle = hid_open_path(Path.c_str());

            if (!handle)
            {
                path = "";
            }
            else
            {
                DefaultError = hid_error(handle);
                hid_device_info *info = hid_get_device_info(handle);

                if (info->bus_type == 1) // USB
                {
                    connectionType = Feature::USB;
                    offset = 0;
                }
                else if (info->bus_type == 2) // Bluetooth
                {
                    connectionType = Feature::BT;
                    offset = 1;
                }

                if (connectionType == Feature::USB)
                {
                    wstring parent;
                    string stringPath(Path);
                    string convertedPath = convertDeviceId(stringPath);
                    wstring finalS = ctow(convertedPath.c_str());

                    int statusf =
                        FindParentDeviceInstanceId(replaceUSBwithHID(finalS),
                                                   L".*",
                                                   parent);
                    wcout << "Device parent: " << parent << endl;
                    lastKnownParent = parent;
                    wcout << lastKnownParent << endl;
                }
                else
                {
                    cout << "Bluetooth connection detected." << endl;
                }

                std::wcout << L"Vendor ID: " << info->manufacturer_string
                           << "\nPath: " << info->path << std::endl;

                Running = true;
                Connected = true;
            }

            path = Path;
        }
    };

    wstring GetKnownAudioParentInstanceID()
    {
        return lastKnownParent;
    }

    void Read()
    {
        const wchar_t *error = hid_error(handle);
        if (error == DefaultError && handle)
        {
            unsigned char ButtonStates[MAX_READ_LENGTH];
            ButtonState buttonState;
            res = hid_read(handle, ButtonStates, MAX_READ_LENGTH);

            buttonState.LX = ButtonStates[1 + offset];
            buttonState.LY = ButtonStates[2 + offset];
            buttonState.RX = ButtonStates[3 + offset];
            buttonState.RY = ButtonStates[4 + offset];
            buttonState.L2 = ButtonStates[5 + offset];
            buttonState.R2 = ButtonStates[6 + offset];

            uint8_t buttonButtonState = ButtonStates[8 + offset];
            buttonState.triangle = (buttonButtonState & (1 << 7)) != 0;
            buttonState.circle = (buttonButtonState & (1 << 6)) != 0;
            buttonState.cross = (buttonButtonState & (1 << 5)) != 0;
            buttonState.square = (buttonButtonState & (1 << 4)) != 0;

            uint8_t dpad_ButtonState = (uint8_t)(buttonButtonState & 0x0F);
            buttonState.SetDPadState(dpad_ButtonState);

            uint8_t misc = ButtonStates[9 + offset];
            buttonState.R3 = (misc & (1 << 7)) != 0;
            buttonState.L3 = (misc & (1 << 6)) != 0;
            buttonState.options = (misc & (1 << 5)) != 0;
            buttonState.share = (misc & (1 << 4)) != 0;
            buttonState.R2Btn = (misc & (1 << 3)) != 0;
            buttonState.L2Btn = (misc & (1 << 2)) != 0;
            buttonState.R1 = (misc & (1 << 1)) != 0;
            buttonState.L1 = (misc & (1 << 0)) != 0;

            uint8_t misc2 = ButtonStates[10 + offset];
            buttonState.ps = (misc2 & (1 << 0)) != 0;
            buttonState.touchBtn = (misc2 & 0x02) != 0;
            buttonState.micBtn = (misc2 & 0x04) != 0;

            buttonState.trackPadTouch0.RawTrackingNum =
                (uint8_t)(ButtonStates[33 + offset]);
            buttonState.trackPadTouch0.ID =
                (uint8_t)(ButtonStates[33 + offset] & 0x7F);
            buttonState.trackPadTouch0.IsActive =
                (ButtonStates[33 + offset] & 0x80) == 0;
            buttonState.trackPadTouch0.X =
                ((ButtonStates[35 + offset] & 0x0F) << 8) |
                ButtonStates[34 + offset];
            buttonState.trackPadTouch0.Y =
                ((ButtonStates[36 + offset]) << 4) |
                ((ButtonStates[35 + offset] & 0xF0) >> 4);

            buttonState.trackPadTouch1.RawTrackingNum =
                (uint8_t)(ButtonStates[37 + offset]);
            buttonState.trackPadTouch1.ID =
                (uint8_t)(ButtonStates[37 + offset] & 0x7F);
            buttonState.trackPadTouch1.IsActive =
                (ButtonStates[37 + offset] & 0x80) == 0;
            buttonState.trackPadTouch1.X =
                ((ButtonStates[39 + offset] & 0x0F) << 8) |
                ButtonStates[38 + offset];
            buttonState.trackPadTouch1.Y =
                ((ButtonStates[40 + offset]) << 4) |
                ((ButtonStates[39 + offset] & 0xF0) >> 4);
            buttonState.TouchPacketNum = (uint8_t)(ButtonStates[41 + offset]);

            buttonState.gyro.X =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[16 + offset],
                                                    ButtonStates[17 + offset]});
            buttonState.gyro.Y =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[18 + offset],
                                                    ButtonStates[19 + offset]});
            buttonState.gyro.Z =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[20 + offset],
                                                    ButtonStates[21 + offset]});

            buttonState.accelerometer.X =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[22 + offset],
                                                    ButtonStates[23 + offset]});
            buttonState.accelerometer.Y =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[24 + offset],
                                                    ButtonStates[25 + offset]});
            buttonState.accelerometer.Z =
                bit_cast<int16_t>(array<uint8_t, 2>{ButtonStates[26 + offset],
                                                    ButtonStates[27 + offset]});

            buttonState.accelerometer.SensorTimestamp =
                (static_cast<uint32_t>(ButtonStates[28 + offset])) |
                (static_cast<uint32_t>(ButtonStates[29 + offset]) << 8) |
                static_cast<uint32_t>(ButtonStates[30 + offset] << 16);

            buttonState.battery.State =
                (BatteryState)((uint8_t)(ButtonStates[53 + offset] & 0xF0) >>
                               4);
            buttonState.battery.Level =
                min((int)((ButtonStates[53 + offset] & 0x0F) * 10 + 5), 100);

            buttonState.PathIdentifier = path;

            State = buttonState;
        }
        else
        {
            ButtonState buttonState;
            Connected = false;
            buttonState.square = false;
            buttonState.triangle = false;
            buttonState.circle = false;
            buttonState.cross = false;
            buttonState.DpadUp = false;
            buttonState.DpadDown = false;
            buttonState.DpadLeft = false;
            buttonState.DpadRight = false;
            buttonState.L1 = false;
            buttonState.L3 = false;
            buttonState.R1 = false;
            buttonState.R3 = false;
            buttonState.R2Btn = false;
            buttonState.L2Btn = false;
            buttonState.share = false;
            buttonState.options = false;
            buttonState.ps = false;
            buttonState.touchBtn = false;
            buttonState.micBtn = false;
            buttonState.L2 = 0;
            buttonState.R2 = 0;
            buttonState.trackPadTouch0.IsActive = false;
            buttonState.trackPadTouch1.IsActive = false;
            State = buttonState;
        }
    }

    bool Write()
    {
        error = hid_error(handle);

        if (error == DefaultError)
        {
            if (connectionType == Feature::USB)
            {
                uint8_t outReport[MAX_USB_WRITE_LENGTH];
                outReport[0] = 2;
                outReport[1] = CurSettings.VibrationType;
                outReport[2] = CurSettings.Features;
                outReport[3] = CurSettings._RightMotor;
                outReport[4] = CurSettings._LeftMotor;
                outReport[5] = 0x7C; // Headset volume
                outReport[6] = (unsigned char)CurSettings.SpeakerVolume;
                outReport[7] = CurSettings.MicrophoneVolume;
                outReport[8] = CurSettings.audioOutput;
                outReport[9] = CurSettings.micLED;
                outReport[10] = CurSettings.micStatus;
                outReport[11] = CurSettings.RightTriggerMode;
                outReport[12] = CurSettings.RightTriggerForces[0];
                outReport[13] = CurSettings.RightTriggerForces[1];
                outReport[14] = CurSettings.RightTriggerForces[2];
                outReport[15] = CurSettings.RightTriggerForces[3];
                outReport[16] = CurSettings.RightTriggerForces[4];
                outReport[17] = CurSettings.RightTriggerForces[5];
                outReport[20] = CurSettings.RightTriggerForces[6];
                outReport[22] = CurSettings.LeftTriggerMode;
                outReport[23] = CurSettings.LeftTriggerForces[0];
                outReport[24] = CurSettings.LeftTriggerForces[1];
                outReport[25] = CurSettings.LeftTriggerForces[2];
                outReport[26] = CurSettings.LeftTriggerForces[3];
                outReport[27] = CurSettings.LeftTriggerForces[4];
                outReport[28] = CurSettings.LeftTriggerForces[5];
                outReport[31] = CurSettings.LeftTriggerForces[6];
                outReport[32] = 0; // one of these lowers haptic intesity
                outReport[33] = 0;
                outReport[34] = 0;
                outReport[35] = 0;
                outReport[36] = 0;
                outReport[37] = 0;
                outReport[38] = 0;
                outReport[39] = CurSettings._Brightness;
                outReport[41] = CurSettings._Brightness;
                outReport[42] = CurSettings.micLED;
                outReport[43] = CurSettings._Brightness;
                outReport[44] = CurSettings.PlayerLED_Bitmask;
                outReport[45] = CurSettings.Red;
                outReport[46] = CurSettings.Green;
                outReport[47] = CurSettings.Blue;

                try
                {
                    res = hid_write(handle, outReport, MAX_USB_WRITE_LENGTH);
                }
                catch (...)
                {
                    Connected = false;
                }
            }
            else if (connectionType == Feature::BT)
            {
                uint8_t outReport[MAX_BT_WRITE_LENGTH];
                outReport[0] = 0x31;
                outReport[1] = 2;
                outReport[2] = CurSettings.VibrationType;
                if (bt_initialized == false)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(
                        500)); // doesn't always work without sleep
                    outReport[3] = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x40;
                    bt_initialized = true;
                }
                else if (bt_initialized == true)
                {
                    outReport[3] = CurSettings.Features;
                }
                outReport[4] = CurSettings._RightMotor;
                outReport[5] = CurSettings._LeftMotor;
                outReport[6] = 0x7C; // Headset volume
                outReport[7] = (unsigned char)CurSettings.SpeakerVolume;
                outReport[8] = CurSettings.MicrophoneVolume;
                outReport[9] = CurSettings.audioOutput;
                outReport[10] = CurSettings.micLED;
                outReport[11] = CurSettings.micStatus;
                outReport[12] = CurSettings.RightTriggerMode;
                outReport[13] = CurSettings.RightTriggerForces[0];
                outReport[14] = CurSettings.RightTriggerForces[1];
                outReport[15] = CurSettings.RightTriggerForces[2];
                outReport[16] = CurSettings.RightTriggerForces[3];
                outReport[17] = CurSettings.RightTriggerForces[4];
                outReport[18] = CurSettings.RightTriggerForces[5];
                outReport[21] = CurSettings.RightTriggerForces[6];
                outReport[23] = CurSettings.LeftTriggerMode;
                outReport[24] = CurSettings.LeftTriggerForces[0];
                outReport[25] = CurSettings.LeftTriggerForces[1];
                outReport[26] = CurSettings.LeftTriggerForces[2];
                outReport[27] = CurSettings.LeftTriggerForces[3];
                outReport[28] = CurSettings.LeftTriggerForces[4];
                outReport[29] = CurSettings.LeftTriggerForces[5];
                outReport[32] = CurSettings.LeftTriggerForces[6];
                outReport[33] = 0; // one of these lowers haptic intesity
                outReport[34] = 0;
                outReport[35] = 0;
                outReport[36] = 0;
                outReport[37] = 0;
                outReport[38] = 0;
                outReport[39] = 0;
                outReport[40] = CurSettings._Brightness;
                outReport[43] = CurSettings._Brightness;
                outReport[44] = CurSettings.micLED;
                outReport[45] = CurSettings.PlayerLED_Bitmask;
                outReport[46] = CurSettings.Red;
                outReport[47] = CurSettings.Green;
                outReport[48] = CurSettings.Blue;

                const UINT32 crcChecksum = compute(outReport, 74);

                outReport[0x4A] =
                    (unsigned char)((crcChecksum & 0x000000FF) >> 0UL);
                outReport[0x4B] =
                    (unsigned char)((crcChecksum & 0x0000FF00) >> 8UL);
                outReport[0x4C] =
                    (unsigned char)((crcChecksum & 0x00FF0000) >> 16UL);
                outReport[0x4D] =
                    (unsigned char)((crcChecksum & 0xFF000000) >> 24UL);

                res = hid_write(handle, outReport, MAX_BT_WRITE_LENGTH);
                error = hid_error(handle);
            }
        }
        else
        {
            Connected = false;
        }
    }

    std::string GetPath()
    {
        return path;
    }

    Feature::ConnectionType GetConnectionType()
    {
        return (Feature::ConnectionType)connectionType;
    }

    void InitializeAudioEngine(bool InitEngine = true)
    {
        if (!AudioInitialized && connectionType == Feature::USB && Connected)
        {
            CComPtr<IMMDeviceEnumerator> pEnumerator;
            CComPtr<IMMDeviceCollection> pDevices;
            CComPtr<IMMDevice> pDevice;
            CComPtr<IPropertyStore> pProps;
            PROPVARIANT varName;
            HRESULT hr = CoInitialize(nullptr); // Initialize COM library

            if (FAILED(hr))
            {
                std::wcerr << L"Failed to initialize COM library." << std::endl;
            }

            hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                  nullptr,
                                  CLSCTX_ALL,
                                  IID_PPV_ARGS(&pEnumerator));
            if (FAILED(hr))
            {
                std::wcerr << L"Failed to create IMMDeviceEnumerator."
                           << std::endl;
            }

            hr = pEnumerator->EnumAudioEndpoints(eRender,
                                                 DEVICE_STATE_ACTIVE,
                                                 &pDevices);
            if (FAILED(hr))
            {
                std::wcerr << L"Failed to enumerate audio endpoints: "
                           << std::hex << hr << std::endl;
            }

            UINT count;
            hr = pDevices->GetCount(&count);
            if (FAILED(hr))
            {
                std::wcerr << L"Failed to get device count: " << std::hex << hr
                           << std::endl;
            }

            std::wcout << L"Number of devices: " << count << std::endl;

            LPWSTR ID;
            bool found = false;
            for (UINT i = 0; i < count; ++i)
            {
                pDevice
                    .Release(); // Make sure to release any previous reference
                hr = pDevices->Item(i, &pDevice);
                if (FAILED(hr) || pDevice == nullptr)
                {
                    std::wcerr << L"Failed to get device at index " << i
                               << L": " << std::hex << hr << std::endl;
                    continue;
                }
                else
                {
                    // Open the property store of the device
                    CComPtr<IPropertyStore> pProps;
                    hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
                    hr = pDevice->GetId(&ID);
                    if (FAILED(hr))
                    {
                        std::wcerr << L"Failed to open property store for "
                                      L"device at index "
                                   << i << L": " << std::hex << hr << std::endl;
                        continue;
                    }
                    else
                    {
                        wstring correctInstanceID =
                            L"SWD\\MMDEVAPI\\" + wstring(ID);
                        wstring foundID;
                        FindParentDeviceInstanceId(correctInstanceID,
                                                   L".*",
                                                   foundID);

                        if (compareDeviceIDs(foundID, lastKnownParent))
                        {
                            wcout << "Found matching parents for dualsense "
                                     "speaker: "
                                  << foundID << endl;
                            found = true;
                            break;
                        }
                    }
                }
            }

            if (!found)
            {
                AudioInitialized = false;
                return;
            }

            if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS)
            {
                // Error.
            }

            ma_device_info *pPlaybackInfos;
            ma_uint32 playbackCount;
            ma_device_info *pCaptureInfos;
            ma_uint32 captureCount;
            if (ma_context_get_devices(&context,
                                       &pPlaybackInfos,
                                       &playbackCount,
                                       &pCaptureInfos,
                                       &captureCount) != MA_SUCCESS)
            {
                // Error.
            }

            int deviceIndex = 0;
            found = false;
            for (ma_uint32 iDevice = 0; iDevice < playbackCount; iDevice += 1)
            {
                std::string name = pPlaybackInfos[iDevice].name;

                ma_device_info info;
                ma_result r = ma_context_get_device_info__wasapi(
                    &context,
                    ma_device_type_playback,
                    &pPlaybackInfos[iDevice].id,
                    &info);

                if (wcscmp(info.id.wasapi, ID) == 0)
                {
                    wcout << "Selected audio device: " << info.id.wasapi
                          << " Index:" << deviceIndex << endl;
                    found = true;
                    break;
                }
                else
                {
                    deviceIndex++;
                }
            }

            if (!found)
            {
                AudioInitialized = false;
                return;
            }

            if (InitEngine)
            {
                ma_channel channel;

                ma_standard_channel_map channelInMap;

                ma_channel_converter_config channelConverterConfig =
                    ma_channel_converter_config_init(
                        ma_format_f32,
                        2,
                        NULL,
                        4,
                        NULL,
                        ma_channel_mix_mode_default);
                ma_channel_converter channelConverter;
                channelConverter.channelsIn = 2;
                channelConverter.channelsOut = 4;
                ma_channel_conversion_path cpath;

                ma_result channelConverterResult =
                    ma_channel_converter_init(&channelConverterConfig,
                                              NULL,
                                              &channelConverter);

                if (channelConverterResult != MA_SUCCESS)
                {
                    cout << "Creating channel converter failed!" << endl;
                }

                ma_device_config deviceconfig =
                    ma_device_config_init(ma_device_type_playback);
                deviceconfig.playback.pDeviceID =
                    &pPlaybackInfos[deviceIndex].id;
                deviceconfig.playback.format = ma_format_f32;
                deviceconfig.playback.channels = 4;
                deviceconfig.playback.channelMixMode =
                    ma_channel_mix_mode_default;
                deviceconfig.sampleRate = 48000;
                deviceconfig.playback.shareMode = ma_share_mode_shared;

                if (ma_device_init(NULL, &deviceconfig, &device) != MA_SUCCESS)
                {
                    cout << "Failed to initialize audio device!" << endl;
                }
                else
                {
                    cout << "Audio device intialized" << endl;
                }

                ma_engine_config config = ma_engine_config_init();
                //config.pContext = &context;
                config.pPlaybackDeviceID = &pPlaybackInfos[deviceIndex].id;
                config.channels = 0;
                config.sampleRate = 48000;
                config.monoExpansionMode = ma_mono_expansion_mode_duplicate;

                result = ma_engine_init(&config, &engine);
                if (result != MA_SUCCESS)
                {
                    return;
                }
                else
                {
                    cout << "Sound engine intialized" << endl;
                }

                ma_engine_set_volume(&engine, 100);
                ma_device_set_master_volume(&device, 1);
                AudioInitialized = true;
            }
        }
    }

    bool PlayHaptics(const char *WavFileLocation)
    {
        if (connectionType == Feature::USB)
        {
            ma_result soundplay =
                ma_engine_play_sound(&engine, WavFileLocation, NULL);

            if (soundplay == MA_SUCCESS)
            {
                return true;
            }
            else
            {
                cout << (ma_result)soundplay << endl;
            }
        }

        return false;
    }

    DualsenseUtils::Reconnect_Result Reconnect(bool SamePort = true)
    {
        if (!Connected)
        {
            bt_initialized = false;
            if (SamePort && path != "")
            {
                handle = hid_open_path(path.c_str());
            }
            else
            {
                handle = hid_open(0x054c, 0x0ce6, NULL);
            }

            if (!handle)
            {
                hid_close(handle);
                Connected = false;
                return DualsenseUtils::Reconnect_Result::Fail;
            }
            else
            {
                Connected = true;
                if (AudioInitialized)
                {
                    ma_context_uninit(&context);
                    ma_device_uninit(&device);
                    ma_engine_uninit(&engine);
                    AudioInitialized = false;
                }

                hid_device_info *info = hid_get_device_info(handle);

                if (info->bus_type == 1) // USB
                {
                    connectionType = Feature::USB;
                    offset = 0;
                }
                else if (info->bus_type == 2) // Bluetooth
                {
                    connectionType = Feature::BT;
                    offset = 1;
                }

                if (connectionType == Feature::USB)
                {
                    wstring parent;
                    string stringPath(path);
                    string convertedPath = convertDeviceId(stringPath);
                    wstring finalS = ctow(convertedPath.c_str());

                    int statusf =
                        FindParentDeviceInstanceId(replaceUSBwithHID(finalS),
                                                   L".*",
                                                   parent);
                    wcout << "Device parent: " << parent << endl;
                    lastKnownParent = parent;
                    wcout << lastKnownParent << endl;
                }
                else
                {
                    cout << "Bluetooth connection detected." << endl;
                }

                return DualsenseUtils::Reconnect_Result::Reconnected;
            }
        }
        else
        {
            return DualsenseUtils::Reconnect_Result::No_Change;
        }
    }

    void Reinitialize()
    {
        Dualsense(path);
    }

    ~Dualsense()
    {
        SetLeftTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0);
        SetRightTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0);

        if (Connected)
        {
            if (connectionType == Feature::USB)
                CurSettings.Features = Feature::FeatureType::LetGo;
            else
            {
                // Return to blue lightbar if bluetooth
                SetPlayerLED(0);
                SetLightbar(0, 0, 255);
                SetMicrophoneLED(false, false);
            }

            UseRumbleNotHaptics(false);
            Write();
        }

        if (AudioInitialized)
        {
            ma_context_uninit(&context);
            ma_device_uninit(&device);
            ma_engine_uninit(&engine);
        }
    };

    void SetLightbar(uint8_t R, uint8_t G, uint8_t B)
    {
        CurSettings.Red = R;
        CurSettings.Green = G;
        CurSettings.Blue = B;
    }

    void SetMicrophoneLED(bool LED, bool Pulse = false)
    {
        if (LED && Pulse)
        {
            CurSettings.micLED = Feature::MicrophoneLED::Pulse;
        }
        else if (LED)
        {
            CurSettings.micLED = Feature::MicrophoneLED::MLED_On;
        }
        else
        {
            CurSettings.micLED = Feature::MicrophoneLED::MLED_Off;
        }
    }

    void SetMicrophoneVolume(int Volume) {
        CurSettings.MicrophoneVolume = Volume;
    }

    void SetPlayerLED(uint8_t Player)
    {
        switch (Player)
        {
        case 0:
            CurSettings.PlayerLED_Bitmask = 0x00;
            break;
        case 1:
            CurSettings.PlayerLED_Bitmask = 0x04;
            break;
        case 2:
            CurSettings.PlayerLED_Bitmask = 0x02;
            break;
        case 3:
            CurSettings.PlayerLED_Bitmask = 0x01 | 0x04;
            break;
        case 4:
            CurSettings.PlayerLED_Bitmask = 0x02 | 0x10;
            break;
        case 5:
            CurSettings.PlayerLED_Bitmask = 0x02 | 0x04 | 0x10;
            break;
        default:
            CurSettings.PlayerLED_Bitmask = 0x00;
            break;
        }
    }

    void SetRightTrigger(Trigger::TriggerMode triggerMode,
                         uint8_t Force1,
                         uint8_t Force2,
                         uint8_t Force3,
                         uint8_t Force4,
                         uint8_t Force5,
                         uint8_t Force6,
                         uint8_t Force7)
    {
        CurSettings.RightTriggerMode = triggerMode;
        CurSettings.RightTriggerForces[0] = Force1;
        CurSettings.RightTriggerForces[1] = Force2;
        CurSettings.RightTriggerForces[2] = Force3;
        CurSettings.RightTriggerForces[3] = Force4;
        CurSettings.RightTriggerForces[4] = Force5;
        CurSettings.RightTriggerForces[5] = Force6;
        CurSettings.RightTriggerForces[6] = Force7;
    }

    void SetLeftTrigger(Trigger::TriggerMode triggerMode,
                        uint8_t Force1,
                        uint8_t Force2,
                        uint8_t Force3,
                        uint8_t Force4,
                        uint8_t Force5,
                        uint8_t Force6,
                        uint8_t Force7)
    {
        CurSettings.LeftTriggerMode = triggerMode;
        CurSettings.LeftTriggerForces[0] = Force1;
        CurSettings.LeftTriggerForces[1] = Force2;
        CurSettings.LeftTriggerForces[2] = Force3;
        CurSettings.LeftTriggerForces[3] = Force4;
        CurSettings.LeftTriggerForces[4] = Force5;
        CurSettings.LeftTriggerForces[5] = Force6;
        CurSettings.LeftTriggerForces[6] = Force7;
    }

    void UseRumbleNotHaptics(bool flag)
    {
        if (flag)
        {
            CurSettings.VibrationType = Feature::StandardRumble;
        }
        else
        {
            CurSettings.VibrationType = Feature::Haptic_Feedback;
        }
    }

    void SetRumble(uint8_t LeftMotor, uint8_t RightMotor)
    {
        if (LeftMotor < 0 || LeftMotor > 255)
        {
            return;
        }

        if (RightMotor < 0 || RightMotor > 255)
        {
            return;
        }

        CurSettings._LeftMotor = LeftMotor;
        CurSettings._RightMotor = RightMotor;
    }
};
