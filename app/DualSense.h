#ifndef DUALSENSE_H
#define DUALSENSE_H

#include <atlcomcli.h>
#include <cfgmgr32.h>
#include <hidapi.h>
#include <mmdeviceapi.h>
#include <setupapi.h>
#include <usbiodef.h>

#include <algorithm>
#include <array>
#include <bit>
#include <codecvt>
#include <cstdint>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <thread>

#include "miniaudio.h"
#pragma comment(lib, "Propsys.lib")

using namespace std;

// Constants for get-parent-device
constexpr int ERR_BAD_ARGUMENTS = 1;
constexpr int ERR_NO_DEVICES_FOUND = 2;
constexpr int ERR_NO_DEVICE_INFO = 3;
constexpr wchar_t GUID_DISK_DRIVE_STRING[] =
L"{21EC2020-3AEA-1069-A2DD-08002B30309D}";

// Function declarations
BOOL GetParentDeviceInstanceId(std::wstring& parentDeviceInstanceId,
							   DEVINST& parentDeviceInstanceIdHandle,
							   DEVINST currentDeviceInstanceId);
BOOL DeviceIdMatchesPattern(const std::wstring& deviceInstanceId,
							const std::wstring& pattern);
int FindParentDeviceInstanceId(
	const std::wstring& searchedDeviceInstanceId,
	const std::wstring& parentDeviceInstanceIdPattern,
	std::wstring& foundDeviceInstanceId);

// Constants for HID communication
#define MAX_READ_LENGTH 64
#define MAX_USB_WRITE_LENGTH 48
#define MAX_BT_WRITE_LENGTH 78

// CRC32 computation function
UINT32 compute(unsigned char* buffer, size_t len);

// Class declarations
class Touchpad {
public:
	int RawTrackingNum = 0;
	bool IsActive = false;
	int ID = 0;
	int X = 0;
	int Y = 0;
};

class Gyro {
public:
	int X = 0;
	int Y = 0;
	int Z = 0;
};

class Accelerometer {
public:
	int X = 0;
	int Y = 0;
	int Z = 0;
	uint32_t SensorTimestamp = 0;
};

enum class BatteryState : uint8_t {
	POWER_SUPPLY_STATUS_DISCHARGING = 0x0,
	POWER_SUPPLY_STATUS_CHARGING = 0x1,
	POWER_SUPPLY_STATUS_FULL = 0x2,
	POWER_SUPPLY_STATUS_NOT_CHARGING = 0xb,
	POWER_SUPPLY_STATUS_ERROR = 0xf,
	POWER_SUPPLY_TEMP_OR_VOLTAGE_OUT_OF_RANGE = 0xa,
	POWER_SUPPLY_STATUS_UNKNOWN = 0x0
};

struct Battery {
	BatteryState State = BatteryState::POWER_SUPPLY_STATUS_UNKNOWN;
	int Level = 0;
};

class ButtonState {
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

	void SetDPadState(int dpad_state);
};

namespace Feature {
	enum MuteLight : uint8_t {
		Off = 0,
		On,
		Breathing,
		DoNothing,
		NoAction4,
		NoAction5,
		NoAction6,
		NoAction7 = 7
	};
	enum class Brightness : uint8_t {
		Bright = 0,
		Mid,
		Dim,
		BNoAction3,
		BNoAction4,
		BNoAction5,
		BNoAction6,
		BNoAction7 = 7
	};
	enum LightFadeAnimation : uint8_t { Nothing = 0, FadeIn, FadeOut };
	enum RumbleMode : uint8_t { StandardRumble = 0xFF, Haptic_Feedback = 0xFC };
	enum FeatureType : int8_t {
		Full = 0x57,
		LetGo = 0x08,
		BT_Init = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x40
	};
	enum AudioOutput : uint8_t { Headset = 0x05, Speaker = 0x31 };
	enum MicrophoneLED : uint8_t { Pulse = 0x2, MLED_On = 0x1, MLED_Off = 0x0 };
	enum MicrophoneStatus : uint8_t { MSTATUS_On = 0, MSTATUS_Off = 0x10 };
	enum PlayerLED : uint8_t {
		PLAYER_OFF = 0x0,
		PLAYER_1 = 0x01,
		PLAYER_2 = 0x02,
		PLAYER_3 = 0x03,
		PLAYER_4 = 0x04,
		PLAYER_5 = 0x05
	};
	enum DeviceType : uint8_t { DualSense = 0, DualSense_Edge = 1 };
	enum ConnectionType : uint8_t { BT = 0x0, USB = 0x1 };
}  // namespace Feature

namespace Trigger {
	enum TriggerMode : uint8_t {
		Off = 0x0,
		Rigid = 0x1,
		Pulse = 0x2,
		Rigid_A = 0x1 | 0x20,
		Rigid_B = 0x1 | 0x04,
		Rigid_AB = 0x1 | 0x20 | 0x04,
		Pulse_A = 0x2 | 0x20,
		Pulse_A2 = 35,
		Pulse_B = 0x2 | 0x04,
		Pulse_B2 = 38,
		Pulse_AB = 39,
		Calibration = 0xFC,
		Feedback = 0x21,
		Weapon = 0x25,
		Vibration = 0x26
	};
}

namespace DualsenseUtils {
	enum HapticFeedbackStatus {
		Working = 0,
		BluetoothNotUSB = 1,
		AudioDeviceNotFound = 2,
		AudioEngineNotInitialized = 3,
		Unknown = 4
	};

#define INPUTFEATURES_JSON_MEMBERS \
  X(VibrationType)                 \
  X(Features)                      \
  X(_RightMotor)                   \
  X(_LeftMotor)                    \
  X(SpeakerVolume)                 \
  X(MicrophoneVolume)              \
  X(HeadsetVolume)                 \
  X(audioOutput)                   \
  X(micLED)                        \
  X(micStatus)                     \
  X(RightTriggerMode)              \
  X(LeftTriggerMode)               \
  X(RightTriggerForces)            \
  X(LeftTriggerForces)             \
  X(_Brightness)                   \
  X(playerLED)                     \
  X(PlayerLED_Bitmask)             \
  X(Red)                           \
  X(Green)                         \
  X(Blue)                          \
  X(RumbleReduction)               \
  X(ImprovedRumbleEmulation)       \
  X(ID)

	class InputFeatures {
	public:
		Feature::RumbleMode VibrationType = Feature::Haptic_Feedback;
		Feature::FeatureType Features = Feature::FeatureType::Full;
		uint8_t _RightMotor = 0;
		uint8_t _LeftMotor = 0;
		int SpeakerVolume = 100;
		int MicrophoneVolume = 80;
		int HeadsetVolume = 0x7C;
		Feature::AudioOutput audioOutput = Feature::Speaker;
		Feature::MicrophoneLED micLED = Feature::MicrophoneLED::MLED_Off;
		Feature::MicrophoneStatus micStatus = Feature::MicrophoneStatus::MSTATUS_On;
		Trigger::TriggerMode RightTriggerMode = Trigger::TriggerMode::Off;
		Trigger::TriggerMode LeftTriggerMode = Trigger::TriggerMode::Off;
		int RightTriggerForces[11] = { 0 };
		int LeftTriggerForces[11] = { 0 };
		Feature::Brightness _Brightness = Feature::Brightness::Bright;
		Feature::PlayerLED playerLED = Feature::PlayerLED::PLAYER_OFF;
		unsigned char PlayerLED_Bitmask = 0x0;
		int Red = 0;
		int Green = 0;
		int Blue = 0;
		string ID = "";
		bool ImprovedRumbleEmulation = true;
		int RumbleReduction = 0x0;

		nlohmann::json to_json() const;
		static InputFeatures from_json(const nlohmann::json& j);
		bool operator==(const InputFeatures& other) const;
	};

	int GetControllerCount();
	std::vector<std::string> EnumerateControllerIDs();
	enum Reconnect_Result { Reconnected = 0, Fail = 1, No_Change = 2 };
}  // namespace DualsenseUtils

std::wstring ctow(const char* src);
std::string convertDeviceId(const std::string& originalId);
std::wstring replaceUSBwithHID(const std::wstring& input);
void trimTrailingWhitespace(std::wstring& str);
void debugString(const std::wstring& str);
bool compareDeviceIDs(const std::wstring& id1, const std::wstring& id2);

class Peaks {
public:
	float channelZero = 0;
	float Speaker = 0;
	float LeftActuator = 0;
	float RightActuator = 0;
};

class Dualsense {
private:
	std::chrono::high_resolution_clock::time_point LastTimeReconnected;
	std::chrono::steady_clock::time_point lastPlayTime;
	std::chrono::milliseconds cooldownDuration{ 300 };
	int offset = 0;
	uint8_t connectionType;
	bool Running = false;
	const wchar_t* DefaultError = L"Success";
	volatile const wchar_t* error = L"";
	int res = 0;
	std::string path = "";
	std::string instanceid = "";
	wstring lastKnownParent;
	hid_device* handle = nullptr;
	bool bt_initialized = false;
	bool AudioInitialized = false;
	bool AudioDeviceNotFound = false;
	bool WasConnectedAtLeastOnce = false;
	bool DisconnectedByError = false;
	bool FirstTimeWrite = true;
	bool isCharging = false;
	wstring AudioDeviceInstanceID;
	const char* AudioDeviceName = nullptr;
	DualsenseUtils::InputFeatures CurSettings;
	DualsenseUtils::InputFeatures LastSettings;
	Peaks peaks;
	ma_context context;
	ma_result result;
	ma_engine engine;
	ma_device device;
	ma_decoder decoder;
	ma_device deviceLoopback;
	std::map<std::string, shared_ptr<ma_sound>> soundMap;
	std::map<std::string, shared_ptr<ma_sound>> CurrentlyPlayedSounds;

	static void data_callback(ma_device* device, void* output, const void* input,
							  ma_uint32 frameCount);
	static void data_callback_playback(ma_device* device, void* output,
									   const void* input, ma_uint32 frameCount);

public:
	bool Connected = false;
	bool TouchpadToHaptics = false;
	float TouchpadToHapticsFrequency = 50.0f;
	ButtonState State;

	Dualsense(std::string Path = "");
	bool LoadSound(const std::string& soundName, const std::string& filePath);
	Peaks GetAudioPeaks();
	wstring GetKnownAudioParentInstanceID();
	wstring GetAudioDeviceInstanceID();
	std::string GetInstanceID();
	const char* GetAudioDeviceName();
	void Read();
	bool Write();
	std::string GetMACAddress(bool colons = true);
	std::string GetPath() const;
	Feature::ConnectionType GetConnectionType();
	void InitializeAudioEngine();
	DualsenseUtils::HapticFeedbackStatus GetHapticFeedbackStatus();
	static void ma_sound_end_proc(void* pUserData, ma_sound* pSound);
	bool SetSoundVolume(const std::string& soundName, float Volume);
	bool SetSoundPitch(const std::string& soundName, float Pitch);
	void StopAllHaptics();
	void StopSoundsThatStartWith(const std::string& String);
	bool StopHaptics(const std::string& soundName);
	bool PlayHaptics(const std::string& soundName,
					 bool DontPlayIfAlreadyPlaying = false,
					 bool LoopUntilStoppedManually = false);
	DualsenseUtils::Reconnect_Result Reconnect(bool SamePort = true);
	void Reinitialize();
	~Dualsense();
	void SetLightbar(uint8_t R, uint8_t G, uint8_t B);
	void SetMicrophoneLED(bool LED, bool Pulse = false);
	void SetMicrophoneVolume(int Volume);
	void SetPlayerLED(uint8_t Player);
	void AddPlayerLED(char Led);
	void SetRightTrigger(Trigger::TriggerMode triggerMode, uint8_t Force1,
						 uint8_t Force2, uint8_t Force3, uint8_t Force4,
						 uint8_t Force5, uint8_t Force6, uint8_t Force7,
						 uint8_t Force8, uint8_t Force9, uint8_t Force10,
						 uint8_t Force11);
	void SetLeftTrigger(Trigger::TriggerMode triggerMode, uint8_t Force1,
						uint8_t Force2, uint8_t Force3, uint8_t Force4,
						uint8_t Force5, uint8_t Force6, uint8_t Force7,
						uint8_t Force8, uint8_t Force9, uint8_t Force10,
						uint8_t Force11);
	void UseRumbleNotHaptics(bool flag);
	void SetSpeakerVolume(int Volume);
	void SetHeadsetVolume(int Volume);
	void SetRumble(uint8_t LeftMotor, uint8_t RightMotor);
	void UseHeadsetNotSpeaker(bool flag);
	void SetLedBrightness(Feature::Brightness brightness);
	void EnableImprovedRumbleEmulation(bool flag);
	void SetRumbleReduction(int value);  // 0x0-0x7
	bool IsCharging() const;
	void SetCharging(bool value);
	unsigned char GetPlayerBitMask() const;
};

#endif  // DUALSENSE_H
