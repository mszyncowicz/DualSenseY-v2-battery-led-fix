﻿const int VERSION = 0;

extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#define MINIAUDIO_IMPLEMENTATION
#include "Config.h"
#include "ControllerEmulation.h"
#include "DualSense.h"
#include "MyUtils.h"
#include "Settings.h"
#include "Strings.h"
#include "UDP.h"
#include "cycle.hpp"
#include <unordered_map>
#define STB_IMAGE_IMPLEMENTATION
#include <urlmon.h>

#include <algorithm>

#include "stb_image.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#include <windows.h>
#include <shellapi.h>
// Use the winrt namespaces
using namespace winrt;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::UI::Notifications;
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
// #define _CRTDBG_MAP_ALLOC
// #include <cstdlib>
// #include <crtdbg.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && \
	!defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")

#endif

static void glfw_error_callback(int error, const char *description);

deque<Dualsense> DualSense;
deque<Settings> ControllerSettings;

std::atomic<bool> stop_thread{false}; // Flag to signal the thread to stop

struct ControllerData
{
	Dualsense *dualsenseUSB = nullptr;
	Dualsense *dualsenseBT = nullptr;

	Settings *settings;

	std::thread *readThreadUSB = nullptr;
	std::thread *writeThreadUSB = nullptr;
	std::thread *emuThreadUSB = nullptr;

	std::thread *readThreadBT = nullptr;
	std::thread *writeThreadBT = nullptr;
	std::thread *emuThreadBT = nullptr;

	ControllerData() {}

	ControllerData(Settings *s)
		: settings(s) {}

	Dualsense *GetActiveConnection()
	{
		if (dualsenseUSB != nullptr && dualsenseUSB->Connected)
		{
			return dualsenseUSB;
		}
		else if (dualsenseBT != nullptr && dualsenseBT->Connected)
		{
			return dualsenseBT;
		}
		else if (dualsenseUSB != nullptr)
		{
			return dualsenseUSB;
		}
		else
		{
			return dualsenseBT;
		}
	}
};

std::unordered_map<std::string, ControllerData> controllerMap;

void killController(ControllerData &data, std::string guid)
{
	if (data.dualsenseUSB != nullptr)
	{
		data.dualsenseUSB->Remove();
	}
	if (data.dualsenseBT != nullptr)
	{
		data.dualsenseBT->Remove();
	}

	DualSense.erase(
		std::remove_if(DualSense.begin(), DualSense.end(),
					   [](const Dualsense &d)
					   {
						   return d.IsRemoved();
					   }),
		DualSense.end());

	auto it = ControllerSettings.begin();
	while (&(*it) != data.settings)
	{
		++it;
	}

	ControllerSettings.erase(it);
	if (data.readThreadUSB != nullptr)
	{
		if (data.readThreadUSB->joinable())
		{
			data.readThreadUSB->join();
		}
	}
	if (data.writeThreadUSB != nullptr)
	{
		if (data.writeThreadUSB->joinable())
		{
			data.writeThreadUSB->join();
		}
	}
	if (data.emuThreadUSB != nullptr)
	{
		if (data.emuThreadUSB->joinable())
		{
			data.emuThreadUSB->join();
		}
	}
	if (data.readThreadUSB != nullptr)
	{
		if (data.readThreadUSB->joinable())
		{
			data.readThreadUSB->join();
		}
	}
	if (data.writeThreadBT != nullptr)
	{
		if (data.writeThreadBT->joinable())
		{
			data.writeThreadBT->join();
		}
	}
	if (data.emuThreadBT != nullptr)
	{
		if (data.emuThreadBT->joinable())
		{
			data.emuThreadBT->join();
		}
	}
	controllerMap.erase(guid);
}

Config::AppConfig appConfig;

void showToast(int controllerNumber, int batteryPercent)
{
	if (appConfig.ShowNotifications)
	{
		winrt::init_apartment();
		std::wstring sound;

		if (batteryPercent > 5)
		{
			sound = L"Reminder";
		}
		else
		{
			sound = L"Looping.Alarm2";
		}

		std::wstring xml = std::format(
			LR"(
        <toast scenario="alarm">
            <visual>
                <binding template='ToastGeneric'>
                    <text>Low battery on controller #{}!</text>
                    <text>The battery is below {}%</text>
                </binding>
            </visual>
            <audio src="ms-winsoundevent:Notification.{}"/>
        </toast>)",
			controllerNumber, batteryPercent, sound);

		XmlDocument toastXml;
		toastXml.LoadXml(xml);

		ToastNotification toast(toastXml);

		const wchar_t *appID = L"MakeSense";
		SetCurrentProcessExplicitAppUserModelID(appID);

		ToastNotificationManager::CreateToastNotifier(appID).Show(toast);
	}
}

void readControllerState(Dualsense &controller, Settings &settings)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	while (!stop_thread && !controller.IsRemoved())
	{
		controller.Read();

		if (controller.Connected == false)
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
}

void Tooltip(const char *text)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip())
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(text);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

struct TriggerSettings
{
	Trigger::TriggerMode Mode;
	bool right = false;
	uint8_t Forces[11];
};

std::string ExtractGuidFromPath(std::string path)
{
	size_t opening_brace = path.find_last_of('{');
	size_t closing_brace = path.find_last_of('}');

	// Basic validation to ensure we found both braces in the correct order
	if (opening_brace != std::string::npos &&
		closing_brace != std::string::npos &&
		opening_brace < closing_brace)
	{
		return path.substr(opening_brace, closing_brace - opening_brace + 1);
	}

	// Return an empty string if the format is not as expected
	return "";
}

void setTriggerSettings(TriggerSettings &trigger, const char *modeStr,
						const int *feedbackArray, size_t feedbackSize)
{
	if (strcmp(modeStr, "Off") == 0)
	{
		trigger.Mode = Trigger::Rigid_B;
		for (int i = 0; i < 11; i++)
		{
			trigger.Forces[i] = 0;
		}
	}
	else if (strcmp(modeStr, "Feedback") == 0)
	{
		uint8_t position = feedbackArray[0];
		uint8_t strength = feedbackArray[1];

		if (position > 9 || strength > 8)
		{
			return;
		}
		if (strength > 0)
		{
			uint8_t b = (strength - 1) & 7;
			uint32_t num = 0;
			uint16_t num2 = 0;

			for (int i = position; i < 10; i++)
			{
				num |= static_cast<uint32_t>(b) << (3 * i);
				num2 |= static_cast<uint16_t>(1 << i);
			}

			trigger.Mode = Trigger::Feedback;
			trigger.Forces[0] = static_cast<uint8_t>(num2 & 0xFF);
			trigger.Forces[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
			trigger.Forces[2] = static_cast<uint8_t>(num & 0xFF);
			trigger.Forces[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
			trigger.Forces[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
			trigger.Forces[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
			for (int i = 6; i < 11; i++)
			{
				trigger.Forces[i] = 0;
			}
		}
		else
		{
			trigger.Mode = Trigger::Rigid_B;
			for (int i = 0; i < 11; i++)
			{
				trigger.Forces[i] = 0;
			}
		}
	}
	else if (strcmp(modeStr, "Weapon") == 0)
	{
		uint8_t startPosition = feedbackArray[0];
		uint8_t endPosition = feedbackArray[1];
		uint8_t strength = feedbackArray[2];

		if (startPosition > 7 || startPosition < 2 || endPosition > 8 ||
			endPosition <= startPosition || strength > 8)
		{
			return;
		}
		if (strength > 0)
		{
			uint16_t num =
				static_cast<uint16_t>(1 << startPosition | 1 << endPosition);

			trigger.Mode = Trigger::Weapon;
			trigger.Forces[0] = static_cast<uint8_t>(num & 0xFF);
			trigger.Forces[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
			trigger.Forces[2] = strength - 1;
			for (int i = 3; i < 11; i++)
			{
				trigger.Forces[i] = 0;
			}
		}
		else
		{
			trigger.Mode = Trigger::Rigid_B;
			for (int i = 0; i < 11; i++)
			{
				trigger.Forces[i] = 0;
			}
		}
	}
	else if (strcmp(modeStr, "Vibration") == 0)
	{
		uint8_t position = feedbackArray[0];
		uint8_t amplitude = feedbackArray[1];
		uint8_t frequency = feedbackArray[2];

		if (position > 9 || amplitude > 8)
		{
			return;
		}
		if (amplitude > 0 && frequency > 0)
		{
			uint8_t b = (uint8_t)((amplitude - 1) & 0x07);
			uint32_t amplitudeZones = 0;
			uint16_t activeZones = 0;

			for (int i = position; i < 10; i++)
			{
				amplitudeZones |= static_cast<uint32_t>(b << (3 * i));
				activeZones |= static_cast<uint16_t>(1 << i);
			}

			trigger.Mode = Trigger::Vibration;
			trigger.Forces[0] = static_cast<uint8_t>((activeZones >> 0) & 0xFF);
			trigger.Forces[1] = static_cast<uint8_t>((activeZones >> 8) & 0xFF);
			trigger.Forces[2] = static_cast<uint8_t>((amplitudeZones >> 0) & 0xFFF);
			trigger.Forces[3] = static_cast<uint8_t>((amplitudeZones >> 8) & 0xFF);
			trigger.Forces[4] = static_cast<uint8_t>((amplitudeZones >> 16) & 0xFF);
			trigger.Forces[5] = static_cast<uint8_t>((amplitudeZones >> 24) & 0xFF);
			trigger.Forces[6] = 0;
			trigger.Forces[7] = 0;
			trigger.Forces[8] = frequency;
			trigger.Forces[9] = 0;
		}
		else
		{
			trigger.Mode = Trigger::Rigid_B;
			for (int i = 0; i < 11; i++)
			{
				trigger.Forces[i] = 0;
			}
		}
	}
	else if (strcmp(modeStr, "Slope Feedback") == 0)
	{
		uint8_t startPosition = feedbackArray[0];
		uint8_t endPosition = feedbackArray[1];
		uint8_t startStrength = feedbackArray[2];
		uint8_t endStrength = feedbackArray[3];

		if (startPosition > 8 || startPosition < 0 || endPosition > 9 ||
			endPosition <= startPosition || startStrength > 8 ||
			startStrength < 1 || endStrength > 8 || endStrength < 1)
		{
			return;
		}

		uint8_t array[10] = {0};
		float slope = static_cast<float>(endStrength - startStrength) /
					  static_cast<float>(endPosition - startPosition);

		for (int i = startPosition; i < 10; i++)
		{
			if (i <= endPosition)
			{
				float strengthFloat = static_cast<float>(startStrength) +
									  slope * static_cast<float>(i - startPosition);
				array[i] = static_cast<uint8_t>(std::round(strengthFloat));
			}
			else
			{
				array[i] = endStrength;
			}
		}

		bool anyStrength = false;
		for (int i = 0; i < 10; i++)
		{
			if (array[i] > 0)
				anyStrength = true;
		}

		if (anyStrength)
		{
			uint32_t num = 0;
			uint16_t num2 = 0;
			for (int i = 0; i < 10; i++)
			{
				if (array[i] > 0)
				{
					uint8_t b = (array[i] - 1) & 7;
					num |= static_cast<uint32_t>(b) << (3 * i);
					num2 |= static_cast<uint16_t>(1 << i);
				}
			}

			trigger.Mode = Trigger::Feedback;
			trigger.Forces[0] = static_cast<uint8_t>(num2 & 0xFF);
			trigger.Forces[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
			trigger.Forces[2] = static_cast<uint8_t>(num & 0xFF);
			trigger.Forces[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
			trigger.Forces[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
			trigger.Forces[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
			for (int i = 6; i < 11; i++)
			{
				trigger.Forces[i] = 0;
			}
		}
		else
		{
			trigger.Mode = Trigger::Rigid_B;
			for (int i = 0; i < 11; i++)
			{
				trigger.Forces[i] = 0;
			}
		}
	}
	else if (strcmp(modeStr, "Multiple Position Feedback") == 0)
	{
		uint8_t strength[10];
		for (int i = 0; i < 10; i++)
		{
			strength[i] = feedbackArray[i];
		}

		bool anyStrength = false;
		for (int i = 0; i < 10; i++)
		{
			if (strength[i] > 0)
				anyStrength = true;
		}

		if (anyStrength)
		{
			uint32_t num = 0;
			uint16_t num2 = 0;
			for (int i = 0; i < 10; i++)
			{
				if (strength[i] > 0)
				{
					uint8_t b = (strength[i] - 1) & 7;
					num |= static_cast<uint32_t>(b) << (3 * i);
					num2 |= static_cast<uint16_t>(1 << i);
				}
			}

			trigger.Mode = Trigger::Feedback;
			trigger.Forces[0] = static_cast<uint8_t>(num2 & 0xFF);
			trigger.Forces[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
			trigger.Forces[2] = static_cast<uint8_t>(num & 0xFF);
			trigger.Forces[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
			trigger.Forces[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
			trigger.Forces[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
			for (int i = 6; i < 11; i++)
			{
				trigger.Forces[i] = 0;
			}
		}
		else
		{
			trigger.Mode = Trigger::Rigid_B;
			for (int i = 0; i < 11; i++)
			{
				trigger.Forces[i] = 0;
			}
		}
	}
	else if (strcmp(modeStr, "Multiple Position Vibration") == 0)
	{
		uint8_t frequency = feedbackArray[0];
		uint8_t amplitude[10];
		for (int i = 0; i < 10; i++)
		{
			amplitude[i] = feedbackArray[i + 1];
		}

		if (frequency > 0)
		{
			bool anyAmplitude = false;
			for (int i = 0; i < 10; i++)
			{
				if (amplitude[i] > 0)
					anyAmplitude = true;
			}

			if (anyAmplitude)
			{
				uint32_t num = 0;
				uint16_t num2 = 0;
				for (int i = 0; i < 10; i++)
				{
					if (amplitude[i] > 0)
					{
						uint8_t b = (amplitude[i] - 1) & 7;
						num |= static_cast<uint32_t>(b) << (3 * i);
						num2 |= static_cast<uint16_t>(1 << i);
					}
				}

				trigger.Mode = Trigger::Vibration;
				trigger.Forces[0] = static_cast<uint8_t>(num2 & 0xFF);
				trigger.Forces[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
				trigger.Forces[2] = static_cast<uint8_t>(num & 0xFF);
				trigger.Forces[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
				trigger.Forces[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
				trigger.Forces[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
				trigger.Forces[6] = 0;
				trigger.Forces[7] = 0;
				trigger.Forces[8] = frequency;
				trigger.Forces[9] = 0;
				trigger.Forces[10] = 0;
			}
			else
			{
				trigger.Mode = Trigger::Rigid_B;
				for (int i = 0; i < 11; i++)
				{
					trigger.Forces[i] = 0;
				}
			}
		}
		else
		{
			trigger.Mode = Trigger::Rigid_B;
			for (int i = 0; i < 11; i++)
			{
				trigger.Forces[i] = 0;
			}
		}
	}
}

void writeEmuController(Dualsense &controller, Settings &settings)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	ViGEm v;
	v.InitializeVigembus();
	ButtonState editedButtonState;
	constexpr auto POLL_INTERVAL =
		std::chrono::microseconds(500);						   // 0.5ms polling
	constexpr auto DISCONNECT_DELAY = std::chrono::seconds(5); // 5 seconds delay

	auto lastConnectedTime = std::chrono::high_resolution_clock::now();
	bool wasVirtualControllerStarted = false;
	EmuStatus lastEmu = None;

	while (!stop_thread && !controller.IsRemoved())
	{
		if (controller.Connected)
		{
			lastConnectedTime = std::chrono::high_resolution_clock::now();

			if (!wasVirtualControllerStarted)
			{
				if (settings.emuStatus == X360)
				{
					v.StartX360();
					wasVirtualControllerStarted = true;
					lastEmu = X360;
				}
				else if (settings.emuStatus == DS4)
				{
					if (settings.DualShock4V2)
						v.SetDS4V2();
					else
						v.SetDS4V1();

					v.StartDS4();
					lastEmu = DS4;
					wasVirtualControllerStarted = true;
				}
			}
			else if (wasVirtualControllerStarted && settings.emuStatus != lastEmu)
			{
				v.Remove360();
				v.RemoveDS4();
				lastEmu == None;
				wasVirtualControllerStarted = false;
			}

			auto start = std::chrono::high_resolution_clock::now();

			editedButtonState = controller.State;

			if (settings.LeftAnalogDeadzone > 0)
			{
				int deltaLX = editedButtonState.LX - 128;
				int deltaLY = editedButtonState.LY - 128;

				float magnitudeL = sqrt(deltaLX * deltaLX + deltaLY * deltaLY);

				if (magnitudeL <= settings.LeftAnalogDeadzone)
				{
					editedButtonState.LX = 128;
					editedButtonState.LY = 128;
				}
				else
				{
					float scale = (magnitudeL - settings.LeftAnalogDeadzone) /
								  (127.0f - settings.LeftAnalogDeadzone);

					if (scale > 1.0f)
						scale = 1.0f;

					editedButtonState.LX = 128 + static_cast<int>(deltaLX * scale);
					editedButtonState.LY = 128 + static_cast<int>(deltaLY * scale);
				}
			}

			if (settings.RightAnalogDeadzone > 0)
			{
				int deltaRX = editedButtonState.RX - 128;
				int deltaRY = editedButtonState.RY - 128;

				float magnitudeR = sqrt(deltaRX * deltaRX + deltaRY * deltaRY);

				if (magnitudeR <= settings.RightAnalogDeadzone)
				{
					editedButtonState.RX = 128;
					editedButtonState.RY = 128;
				}
				else
				{
					float scale = (magnitudeR - settings.RightAnalogDeadzone) /
								  (127.0f - settings.RightAnalogDeadzone);

					if (scale > 1.0f)
						scale = 1.0f;

					editedButtonState.RX = 128 + static_cast<int>(deltaRX * scale);
					editedButtonState.RY = 128 + static_cast<int>(deltaRY * scale);
				}
			}

			if (settings.TriggersAsButtons)
			{
				if (editedButtonState.L2 >= 1)
				{
					editedButtonState.L2 = 255;
					editedButtonState.L2Btn = true;
				}

				if (editedButtonState.R2 >= 1)
				{
					editedButtonState.R2 = 255;
					editedButtonState.R2Btn = true;
				}
			}

			if (settings.TouchpadAsSelect && !settings.TouchpadAsStart)
			{
				if (controller.State.touchBtn)
				{
					editedButtonState.share = true;
				}
			}
			else if (settings.TouchpadAsStart && !settings.TouchpadAsSelect)
			{
				if (controller.State.touchBtn)
				{
					editedButtonState.options = true;
				}
			}
			else if (settings.TouchpadAsSelect && settings.TouchpadAsStart)
			{
				if (controller.State.trackPadTouch0.IsActive &&
					!controller.State.trackPadTouch1.IsActive)
				{
					if (controller.State.touchBtn &&
						controller.State.trackPadTouch0.X <= 1000)
					{
						editedButtonState.share = true;
					}

					if (controller.State.touchBtn &&
						controller.State.trackPadTouch0.X >= 1001)
					{
						editedButtonState.options = true;
					}
				}
				else if (controller.State.trackPadTouch1.IsActive &&
						 !controller.State.trackPadTouch0.IsActive)
				{
					if (controller.State.touchBtn &&
						controller.State.trackPadTouch1.X <= 1000)
					{
						editedButtonState.share = true;
					}

					if (controller.State.touchBtn &&
						controller.State.trackPadTouch1.X >= 1001)
					{
						editedButtonState.options = true;
					}
				}
			}

			if (settings.TouchpadToRXRY && controller.State.trackPadTouch0.IsActive)
			{
				editedButtonState.RX =
					ConvertRange(controller.State.trackPadTouch0.X, 0, 1919, 0, 255);
				editedButtonState.RY =
					ConvertRange(controller.State.trackPadTouch0.Y, 0, 1079, 0, 255);
			}

			if (settings.GyroToRightAnalogStick &&
				(controller.State.L2 >= settings.L2Threshold))
			{
				if (fabs(controller.State.RX - 128) <= 80 &&
					fabs(controller.State.RY - 128) <= 80)
				{
					float accelY = controller.State.accelerometer.X;
					float accelX = controller.State.accelerometer.Y;

					const float maxAccelValue = 10000.0f;

					float normalizedAccelX = accelX / maxAccelValue;
					float normalizedAccelY = accelY / maxAccelValue;

					if (fabs(normalizedAccelX) <
						settings.GyroToRightAnalogStickDeadzone)
					{
						normalizedAccelX = 0.0f;
					}
					else
					{
						normalizedAccelX =
							(normalizedAccelX > 0
								 ? normalizedAccelX -
									   settings.GyroToRightAnalogStickDeadzone
								 : normalizedAccelX +
									   settings.GyroToRightAnalogStickDeadzone) /
							(1.0f - settings.GyroToRightAnalogStickDeadzone);
					}

					if (fabs(normalizedAccelY) <
						settings.GyroToRightAnalogStickDeadzone)
					{
						normalizedAccelY = 0.0f;
					}
					else
					{
						normalizedAccelY =
							(normalizedAccelY > 0
								 ? normalizedAccelY -
									   settings.GyroToRightAnalogStickDeadzone
								 : normalizedAccelY +
									   settings.GyroToRightAnalogStickDeadzone) /
							(1.0f - settings.GyroToRightAnalogStickDeadzone);
					}

					const float sensitivity =
						settings.GyroToRightAnalogStickSensitivity * 5;

					float adjustedX = -normalizedAccelX * sensitivity;
					float adjustedY = -normalizedAccelY * sensitivity;

					int accelAnalogStickX;
					if (adjustedX > 0)
					{
						accelAnalogStickX =
							128 + max(static_cast<int>(
										  settings.GyroToRightAnalogStickMinimumStickValue),
									  static_cast<int>(adjustedX * 127.0f));
					}
					else if (adjustedX < 0)
					{
						accelAnalogStickX =
							128 - max(static_cast<int>(
										  settings.GyroToRightAnalogStickMinimumStickValue),
									  static_cast<int>(-adjustedX * 128.0f));
					}
					else
					{
						accelAnalogStickX = 128; // Center if no movement
					}

					int accelAnalogStickY;
					if (adjustedY > 0)
					{
						accelAnalogStickY =
							128 + max(static_cast<int>(
										  settings.GyroToRightAnalogStickMinimumStickValue),
									  static_cast<int>(adjustedY * 127.0f));
					}
					else if (adjustedY < 0)
					{
						accelAnalogStickY =
							128 - max(static_cast<int>(
										  settings.GyroToRightAnalogStickMinimumStickValue),
									  static_cast<int>(-adjustedY * 128.0f));
					}
					else
					{
						accelAnalogStickY = 128; // Center if no movement
					}

					editedButtonState.RX =
						controller.State.RX + (accelAnalogStickX - 127);
					editedButtonState.RY =
						controller.State.RY + (accelAnalogStickY - 127);
				}
			}

			if (controller.State.L2 < settings.L2Deadzone)
			{
				editedButtonState.L2 = 0;
				editedButtonState.L2Btn = false;
			}
			if (controller.State.R2 < settings.R2Deadzone)
			{
				editedButtonState.R2 = 0;
				editedButtonState.R2Btn = false;
			}

			if (settings.CurrentlyUsingUDP)
			{
				if (settings.L2UDPDeadzone > 0 &&
					(controller.State.L2 < settings.L2UDPDeadzone))
				{
					editedButtonState.L2 = 0;
					editedButtonState.L2Btn = false;
				}

				if (settings.R2UDPDeadzone > 0 &&
					(controller.State.R2 < settings.R2UDPDeadzone))
				{
					editedButtonState.R2 = 0;
					editedButtonState.R2Btn = false;
				}
			}

			if (settings.LeftStickToMouse || settings.RightStickToMouse)
			{
				int AnalogX = settings.LeftStickToMouse ? controller.State.LX : 0;
				AnalogX = settings.RightStickToMouse ? controller.State.RX : AnalogX;

				int AnalogY = settings.LeftStickToMouse ? controller.State.LY : 0;
				AnalogY = settings.RightStickToMouse ? controller.State.RY : AnalogY;

				float x = (AnalogX - 128) * settings.LeftStickToMouseSensitivity;
				float y =
					(AnalogY - 128) * (settings.LeftStickToMouseSensitivity + 0.001f);

				MyUtils::MoveCursor(x, y);
			}

			if (settings.emuStatus != None && !v.isWorking())
			{
				MessageBox(0,
						   "ViGEm connection failed! Are you sure you "
						   "installed ViGEmBus driver?",
						   "Error", 0);
				settings.emuStatus = None;
				continue;
			}

			if (settings.emuStatus == X360)
			{
				v.UpdateX360(editedButtonState);
				settings.lrumble = min(settings.MaxLeftMotor, v.rotors.lrotor);
				settings.rrumble = min(settings.MaxRightMotor, v.rotors.rrotor);
			}
			else if (settings.emuStatus == DS4)
			{
				v.UpdateDS4(editedButtonState);
				settings.lrumble = min(settings.MaxLeftMotor, v.rotors.lrotor);
				settings.rrumble = min(settings.MaxRightMotor, v.rotors.rrotor);

				if (!settings.CurrentlyUsingUDP)
				{
					if (v.Red > 0 || v.Green > 0 || v.Blue > 0)
					{
						controller.SetLightbar(v.Red, v.Green, v.Blue);
					}
				}
			}

			auto end = std::chrono::high_resolution_clock::now();
			auto elapsed = end - start;
			if (elapsed < POLL_INTERVAL)
			{
				std::this_thread::sleep_for(POLL_INTERVAL - elapsed);
			}
		}
		else
		{
			auto now = std::chrono::high_resolution_clock::now();
			if (wasVirtualControllerStarted &&
				now - lastConnectedTime > DISCONNECT_DELAY)
			{
				v.Remove360();
				v.RemoveDS4();
				lastEmu == None;
				wasVirtualControllerStarted = false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}

	v.Free();
}

void MouseClick(DWORD flag, DWORD mouseData = 0)
{
	// Simulate mouse left button down
	INPUT input;
	input.type = INPUT_MOUSE;
	input.mi.mouseData = mouseData;
	input.mi.dwFlags = flag; // Left button down
	input.mi.time = 0;
	input.mi.dwExtraInfo = 0;
	SendInput(1, &input, sizeof(INPUT));
}

void writeControllerState(Dualsense &controller, Settings &settings,
						  UDP &udpServer)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	cout << "Write thread started: " << controller.GetPath() << " | "
		 << settings.ControllerInput.ID << endl;

	// DISCO MODE VARIABLES
	int colorState =
		0; // 0 = red to yellow, 1 = yellow to green, 2 = green to cyan, etc.
	int colorStateBattery = 0;
	int colorStateX360EMU = 0;
	int led[3];
	int step = 5;
	led[0] = 255;
	led[1] = 0;
	led[2] = 0;
	int x360emuanimation = 0;
	bool lastMic = false;
	bool MicShortcutTriggered = false;
	bool DisconnectShortcutTriggered = false;
	bool mic = false;
	float touchpadLastX = 0;
	float touchpadLastY = 0;
	float touchpad1LastY = 0;
	bool wasR2Clicked = false;
	bool wasTouching = false;
	bool wasTouching1 = false;
	bool wasTouchpadClicked = false;
	bool X360animationplayed = false;
	bool X360readyfornew = false;
	int steps[] = {1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1};
	int limits[] = {255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0};
	int direction = 1; // 1 for increasing, -1 for decreasing
	DualsenseUtils::InputFeatures lastFeatures;
	bool firstTimeUDP = true;
	DualsenseUtils::InputFeatures preUDP;
	auto lastTimeFailedToWrite = std::chrono::high_resolution_clock::now();
	bool showBatteryLevel1 = true;
	bool showBatteryLevel2 = true;
	while (!stop_thread && !controller.IsRemoved())
	{
		if (true)
		{
			if (!lastMic && MicShortcutTriggered)
			{
				MicShortcutTriggered = false;
			}
			if (controller.IsCharging())
			{
				showBatteryLevel1 = true;
				showBatteryLevel2 = true;
			}

			if (settings.emuStatus == X360 && !X360animationplayed)
			{
				if (colorStateX360EMU < 20)
				{
					x360emuanimation += direction * step;

					// Reverse direction when reaching the upper or lower limits
					if (x360emuanimation >= 255)
					{
						x360emuanimation = 255; // Clamp to the max value
						direction = -1;			// Change to decreasing
						colorStateX360EMU++;
					}
					else if (x360emuanimation <= 0)
					{
						x360emuanimation = 0; // Clamp to the min value
						direction = 1;		  // Change to increasing
						colorStateX360EMU++;
					}

					controller.SetLightbar(0, x360emuanimation, 0);
				}
				else
				{
					X360animationplayed = true;
				}
			}
			else if (settings.emuStatus == X360 && X360readyfornew)
			{
				X360animationplayed = false;
			}

			if (settings.emuStatus == None && X360animationplayed)
			{
				X360readyfornew = true;
				x360emuanimation = 0;
				colorStateX360EMU = 0;
			}
			else if (settings.emuStatus == DS4 && !X360animationplayed)
			{
				X360animationplayed = true;
			}
			else if (settings.emuStatus == None && !X360animationplayed)
			{
				X360animationplayed = true;
			}

			if ((controller.State.micBtn && controller.State.touchBtn &&
				 settings.DisconnectControllerShortcut))
			{
				auto now = std::chrono::high_resolution_clock::now();
				if ((now - lastTimeFailedToWrite) > std::chrono::seconds(5))
				{
					MyUtils::DisableBluetoothDevice(controller.GetMACAddress(false));
					lastTimeFailedToWrite = now;
				}
			}

			if (controller.State.micBtn && controller.State.DpadUp &&
				settings.SwapTriggersShortcut && !MicShortcutTriggered)
			{
				settings.SwapTriggersRumbleToAT =
					settings.SwapTriggersRumbleToAT
						? settings.SwapTriggersRumbleToAT = false
						: settings.SwapTriggersRumbleToAT = true;
				MicShortcutTriggered = true;
			}

			if (controller.State.micBtn && controller.State.L1 &&
				settings.GyroToMouseShortcut && !MicShortcutTriggered)
			{
				settings.GyroToMouse = settings.GyroToMouse
										   ? settings.GyroToMouse = false
										   : settings.GyroToMouse = true;
				MicShortcutTriggered = true;
			}

			if (controller.State.micBtn && controller.State.L2 &&
				settings.GyroToRightAnalogStickShortcut && !MicShortcutTriggered)
			{
				settings.GyroToRightAnalogStick =
					settings.GyroToRightAnalogStick
						? settings.GyroToRightAnalogStick = false
						: settings.GyroToRightAnalogStick = true;
				MicShortcutTriggered = true;
			}

			if (controller.State.micBtn && controller.State.triangle &&
				settings.StopWritingShortcut && !MicShortcutTriggered)
			{
				settings.StopWriting = settings.StopWriting
										   ? settings.StopWriting = false
										   : settings.StopWriting = true;
				MicShortcutTriggered = true;

				if (settings.StopWriting)
					controller.PlayHaptics("halted");
				else
					controller.PlayHaptics("resumed");
			}

			if (settings.emuStatus != X360 && settings.X360Shortcut &&
				controller.State.micBtn && controller.State.DpadLeft)
			{
				X360animationplayed = false;
				X360readyfornew = true;
				MyUtils::RunAsyncHidHideRequest(controller.GetPath(), "hide");
				settings.emuStatus = X360;
			}

			if (settings.emuStatus != DS4 && settings.DS4Shortcut &&
				controller.State.micBtn && controller.State.DpadRight)
			{
				MyUtils::RunAsyncHidHideRequest(controller.GetPath(), "hide");
				settings.emuStatus = DS4;
				controller.SetLightbar(0, 0, 64);
			}

			if (settings.emuStatus != None && settings.StopEmuShortcut &&
				controller.State.micBtn && controller.State.DpadDown)
			{
				MyUtils::RunAsyncHidHideRequest(controller.GetPath(), "show");
				settings.emuStatus = None;
			}

			if (settings.TouchpadToMouseShortcut && controller.State.micBtn &&
				controller.State.touchBtn && !MicShortcutTriggered)
			{
				settings.TouchpadToMouse = settings.TouchpadToMouse
											   ? settings.TouchpadToMouse = false
											   : settings.TouchpadToMouse = true;
				MicShortcutTriggered = true;
			}

			if (settings.R2ToMouseClick && controller.State.R2Btn)
			{
				if (!wasR2Clicked)
				{
					MouseClick(MOUSEEVENTF_LEFTDOWN);
					wasR2Clicked = true;
				}
			}
			else if (settings.R2ToMouseClick && !controller.State.R2Btn)
			{
				if (wasR2Clicked)
				{
					MouseClick(MOUSEEVENTF_LEFTUP);
					wasR2Clicked = false;
				}
			}

			if (settings.TouchpadToMouse &&
				controller.State.trackPadTouch0.IsActive)
			{
				if (controller.State.touchBtn && !wasTouchpadClicked &&
					controller.State.trackPadTouch0.X <= 1000)
				{
					MouseClick(MOUSEEVENTF_LEFTDOWN);
					wasTouchpadClicked = true;
				}
				else if (!controller.State.touchBtn && wasTouchpadClicked &&
						 controller.State.trackPadTouch0.X <= 1000)
				{
					MouseClick(MOUSEEVENTF_LEFTUP);
					wasTouchpadClicked = false;
				}

				if (controller.State.touchBtn && !wasTouchpadClicked &&
					controller.State.trackPadTouch0.X >= 1001)
				{
					MouseClick(MOUSEEVENTF_RIGHTDOWN);
					wasTouchpadClicked = true;
				}
				else if (!controller.State.touchBtn && wasTouchpadClicked &&
						 controller.State.trackPadTouch0.X >= 1001)
				{
					MouseClick(MOUSEEVENTF_RIGHTUP);
					wasTouchpadClicked = false;
				}

				// If it wasn't previously touching, start tracking movement
				if (!wasTouching)
				{
					touchpadLastX = controller.State.trackPadTouch0.X;
					touchpadLastY = controller.State.trackPadTouch0.Y;
				}

				if (!wasTouching1)
				{
					touchpad1LastY = controller.State.trackPadTouch1.Y;
				}

				float normalizedX = controller.State.trackPadTouch0.X;
				float normalizedY = controller.State.trackPadTouch0.Y;

				float deltaX = normalizedX - touchpadLastX;
				float deltaY = normalizedY - touchpadLastY;
				float delta1Y = touchpad1LastY - controller.State.trackPadTouch1.Y;

				if (controller.State.trackPadTouch1.IsActive)
				{
					wasTouching1 = true;
					if (fabs(delta1Y) > 5.0f)
					{
						MouseClick(MOUSEEVENTF_WHEEL, static_cast<int>(delta1Y * 2));
					}

					touchpad1LastY = controller.State.trackPadTouch1.Y;
				}
				else
				{
					wasTouching1 = false;
				}

				if (fabs(deltaX) > settings.swipeThreshold ||
					fabs(deltaY) > settings.swipeThreshold)
				{
					MyUtils::MoveCursor(deltaX * settings.swipeThreshold,
										deltaY * settings.swipeThreshold);
				}

				// Update the last touchpad position
				touchpadLastX = normalizedX;
				touchpadLastY = normalizedY;

				// Mark that the touchpad is active
				wasTouching = true;
			}
			else
			{
				// Reset the touch state when the touchpad is no longer active
				wasTouching = false;
			}

			if (settings.GyroToMouse)
			{
				if (controller.State.L2 >= settings.L2Threshold)
				{
					int cursorDeltaX =
						static_cast<int>(-controller.State.accelerometer.Y *
										 settings.GyroToMouseSensitivity);
					int cursorDeltaY =
						static_cast<int>(-controller.State.accelerometer.X *
										 settings.GyroToMouseSensitivity);

					MyUtils::MoveCursor(cursorDeltaX, cursorDeltaY);
				}
			}

			if (!settings.DisableLightbar && !settings.CurrentlyUsingUDP &&
				(settings.emuStatus != DS4 || settings.OverrideDS4Lightbar) &&
				X360animationplayed)
			{
				controller.SetLightbar(settings.ControllerInput.Red,
									   settings.ControllerInput.Green,
									   settings.ControllerInput.Blue);
			}

			if (!settings.RumbleToAT && !settings.HapticsToTriggers &&
				!settings.CurrentlyUsingUDP && settings.triggerFormat == "DSX")
			{
				controller.SetRightTrigger(
					settings.ControllerInput.RightTriggerMode, settings.R2DSXForces[0],
					settings.R2DSXForces[1], settings.R2DSXForces[2],
					settings.R2DSXForces[3], settings.R2DSXForces[4],
					settings.R2DSXForces[5], settings.R2DSXForces[6],
					settings.R2DSXForces[7], settings.R2DSXForces[8],
					settings.R2DSXForces[9], settings.R2DSXForces[10]);

				controller.SetLeftTrigger(
					settings.ControllerInput.LeftTriggerMode, settings.L2DSXForces[0],
					settings.L2DSXForces[1], settings.L2DSXForces[2],
					settings.L2DSXForces[3], settings.L2DSXForces[4],
					settings.L2DSXForces[5], settings.L2DSXForces[6],
					settings.L2DSXForces[7], settings.L2DSXForces[8],
					settings.L2DSXForces[9], settings.L2DSXForces[10]);
			}
			else if (!settings.RumbleToAT && !settings.HapticsToTriggers &&
					 !settings.CurrentlyUsingUDP &&
					 settings.triggerFormat == "Sony")
			{
#pragma region Apply Sony Trigger
				TriggerSettings l2;

				if (settings.lmodestrSony == "Off")
				{
					l2.Mode = Trigger::Rigid_B;
					for (int i = 0; i < 11; i++)
					{
						l2.Forces[i] = 0;
					}
				}
				else if (settings.lmodestrSony == "Feedback")
				{
					setTriggerSettings(l2, settings.lmodestrSony.c_str(),
									   settings.L2Feedback, sizeof(settings.L2Feedback));
				}
				else if (settings.lmodestrSony == "Weapon")
				{
					setTriggerSettings(l2, settings.lmodestrSony.c_str(),
									   settings.L2WeaponFeedback,
									   sizeof(settings.L2WeaponFeedback));
				}
				else if (settings.lmodestrSony == "Vibration")
				{
					setTriggerSettings(l2, settings.lmodestrSony.c_str(),
									   settings.L2VibrationFeedback,
									   sizeof(settings.L2VibrationFeedback));
				}
				else if (settings.lmodestrSony == "Slope Feedback")
				{
					setTriggerSettings(l2, settings.lmodestrSony.c_str(),
									   settings.L2SlopeFeedback,
									   sizeof(settings.L2SlopeFeedback));
				}
				else if (settings.lmodestrSony == "Multiple Position Feedback")
				{
					setTriggerSettings(l2, settings.lmodestrSony.c_str(),
									   settings.L2MultiplePositionFeedback,
									   sizeof(settings.L2MultiplePositionFeedback));
				}
				else if (settings.lmodestrSony == "Multiple Position Vibration")
				{
					setTriggerSettings(l2, settings.lmodestrSony.c_str(),
									   settings.L2MultipleVibrationFeedback,
									   sizeof(settings.L2MultipleVibrationFeedback));
				}

				TriggerSettings r2;
				r2.right = true;
				if (settings.rmodestrSony == "Off")
				{
					r2.Mode = Trigger::Rigid_B;
					for (int i = 0; i < 11; i++)
					{
						r2.Forces[i] = 0;
					}
				}
				else if (settings.rmodestrSony == "Feedback")
				{
					setTriggerSettings(r2, settings.rmodestrSony.c_str(),
									   settings.R2Feedback, sizeof(settings.R2Feedback));
				}
				else if (settings.rmodestrSony == "Weapon")
				{
					setTriggerSettings(r2, settings.rmodestrSony.c_str(),
									   settings.R2WeaponFeedback,
									   sizeof(settings.R2WeaponFeedback));
				}
				else if (settings.rmodestrSony == "Vibration")
				{
					setTriggerSettings(r2, settings.rmodestrSony.c_str(),
									   settings.R2VibrationFeedback,
									   sizeof(settings.R2VibrationFeedback));
				}
				else if (settings.rmodestrSony == "Slope Feedback")
				{
					setTriggerSettings(r2, settings.rmodestrSony.c_str(),
									   settings.R2SlopeFeedback,
									   sizeof(settings.R2SlopeFeedback));
				}
				else if (settings.rmodestrSony == "Multiple Position Feedback")
				{
					setTriggerSettings(r2, settings.rmodestrSony.c_str(),
									   settings.R2MultiplePositionFeedback,
									   sizeof(settings.R2MultiplePositionFeedback));
				}
				else if (settings.rmodestrSony == "Multiple Position Vibration")
				{
					setTriggerSettings(r2, settings.rmodestrSony.c_str(),
									   settings.R2MultipleVibrationFeedback,
									   sizeof(settings.R2MultipleVibrationFeedback));
				}

			skipSonyTrigger:
				controller.SetRightTrigger(r2.Mode, r2.Forces[0], r2.Forces[1],
										   r2.Forces[2], r2.Forces[3], r2.Forces[4],
										   r2.Forces[5], r2.Forces[6], r2.Forces[7],
										   r2.Forces[8], r2.Forces[9], r2.Forces[10]);

				controller.SetLeftTrigger(l2.Mode, l2.Forces[0], l2.Forces[1],
										  l2.Forces[2], l2.Forces[3], l2.Forces[4],
										  l2.Forces[5], l2.Forces[6], l2.Forces[7],
										  l2.Forces[8], l2.Forces[9], l2.Forces[10]);
#pragma endregion
			}

			if (settings.RumbleToAT && !settings.CurrentlyUsingUDP)
			{
				int leftForces[3] = {0};
				int rightForces[3] = {0};

				if (settings.RumbleToAT_RigidMode)
				{
					// stiffness
					leftForces[1] =
						settings.SwapTriggersRumbleToAT
							? min(settings.MaxRightTriggerRigid, settings.rrumble)
							: min(settings.MaxLeftTriggerRigid, settings.lrumble);
					rightForces[1] =
						settings.SwapTriggersRumbleToAT
							? min(settings.MaxLeftTriggerRigid, settings.lrumble)
							: min(settings.MaxRightTriggerRigid, settings.rrumble);

					// set threshold
					leftForces[0] = settings.SwapTriggersRumbleToAT
										? 255 - settings.rrumble
										: 255 - settings.lrumble;
					rightForces[0] = settings.SwapTriggersRumbleToAT
										 ? 255 - settings.lrumble
										 : 255 - settings.rrumble;

					controller.SetLeftTrigger(Trigger::Rigid, leftForces[0],
											  leftForces[1], 0, 0, 0, 0, 0, 0, 0, 0, 0);
					controller.SetRightTrigger(Trigger::Rigid, rightForces[0],
											   rightForces[1], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				}
				else
				{
					// set frequency
					if (!settings.SwapTriggersRumbleToAT && settings.lrumble < 25 ||
						settings.SwapTriggersRumbleToAT && settings.rrumble < 25)
						leftForces[0] = settings.SwapTriggersRumbleToAT
											? min(settings.MaxRightRumbleToATFrequency,
												  settings.rrumble)
											: min(settings.MaxLeftRumbleToATFrequency,
												  settings.lrumble);
					else
						leftForces[0] = min(settings.MaxLeftRumbleToATFrequency, 25);

					if (!settings.SwapTriggersRumbleToAT && settings.rrumble < 25 ||
						settings.SwapTriggersRumbleToAT && settings.lrumble < 25)
						rightForces[0] =
							settings.SwapTriggersRumbleToAT
								? min(settings.MaxLeftRumbleToATFrequency, settings.lrumble)
								: min(settings.MaxRightRumbleToATFrequency,
									  settings.rrumble);
					else
						rightForces[0] = min(settings.MaxRightRumbleToATFrequency, 25);

					// set power
					leftForces[1] =
						settings.SwapTriggersRumbleToAT
							? min(settings.MaxRightRumbleToATIntensity, settings.rrumble)
							: min(settings.MaxLeftRumbleToATIntensity, settings.lrumble);
					rightForces[1] =
						settings.SwapTriggersRumbleToAT
							? min(settings.MaxLeftRumbleToATIntensity, settings.lrumble)
							: min(settings.MaxRightRumbleToATIntensity, settings.rrumble);

					// set static threshold
					leftForces[2] = 20;
					rightForces[2] = 20;

					controller.SetLeftTrigger(Trigger::Pulse_B, leftForces[0],
											  leftForces[1], leftForces[2], 0, 0, 0, 0, 0,
											  0, 0, 0);
					controller.SetRightTrigger(Trigger::Pulse_B, rightForces[0],
											   rightForces[1], rightForces[2], 0, 0, 0, 0,
											   0, 0, 0, 0);
				}
			}

			Peaks peaks = controller.GetAudioPeaks();
			if (settings.HapticsToTriggers && !settings.CurrentlyUsingUDP)
			{
				if (settings.RumbleToAT && settings.lrumble > 0 ||
					settings.RumbleToAT && settings.rrumble > 0)
				{
					goto skiphapticstotriggers;
				}

				int ScaledLeftActuator = MyUtils::scaleFloatToInt(
					peaks.LeftActuator, settings.AudioToTriggersMaxFloatValue);
				int ScaledRightActuator = MyUtils::scaleFloatToInt(
					peaks.RightActuator, settings.AudioToTriggersMaxFloatValue);
				int leftForces[3] = {0};
				int rightForces[3] = {0};

				if (settings.RumbleToAT_RigidMode)
				{
					// stiffness
					leftForces[1] = settings.SwapTriggersRumbleToAT ? ScaledRightActuator
																	: ScaledLeftActuator;
					rightForces[1] = settings.SwapTriggersRumbleToAT
										 ? ScaledLeftActuator
										 : ScaledRightActuator;

					// set threshold
					leftForces[0] = settings.SwapTriggersRumbleToAT
										? 255 - ScaledRightActuator
										: 255 - ScaledLeftActuator;
					rightForces[0] = settings.SwapTriggersRumbleToAT
										 ? 255 - ScaledLeftActuator
										 : 255 - ScaledRightActuator;

					controller.SetLeftTrigger(Trigger::Rigid, leftForces[0],
											  leftForces[1], 0, 0, 0, 0, 0, 0, 0, 0, 0);
					controller.SetRightTrigger(Trigger::Rigid, rightForces[0],
											   rightForces[1], 0, 0, 0, 0, 0, 0, 0, 0, 0);
				}
				else
				{
					// cout << ScaledLeftActuator << " | " << ScaledRightActuator << endl;

					// set frequency
					if (!settings.SwapTriggersRumbleToAT && ScaledLeftActuator < 25 ||
						settings.SwapTriggersRumbleToAT && ScaledRightActuator < 25)
						leftForces[0] = settings.SwapTriggersRumbleToAT
											? min(settings.MaxRightRumbleToATFrequency,
												  ScaledRightActuator)
											: min(settings.MaxLeftRumbleToATFrequency,
												  ScaledLeftActuator);
					else
						leftForces[0] = min(settings.MaxLeftRumbleToATFrequency, 25);

					if (!settings.SwapTriggersRumbleToAT && ScaledRightActuator < 25 ||
						settings.SwapTriggersRumbleToAT && ScaledLeftActuator < 25)
						rightForces[0] = settings.SwapTriggersRumbleToAT
											 ? min(settings.MaxLeftRumbleToATFrequency,
												   ScaledLeftActuator)
											 : min(settings.MaxRightRumbleToATFrequency,
												   ScaledRightActuator);
					else
						rightForces[0] = min(settings.MaxRightRumbleToATFrequency, 25);

					// set power
					leftForces[1] = settings.SwapTriggersRumbleToAT
										? min(settings.MaxRightRumbleToATFrequency,
											  ScaledRightActuator)
										: min(settings.MaxLeftRumbleToATFrequency,
											  ScaledLeftActuator);
					rightForces[1] =
						settings.SwapTriggersRumbleToAT
							? min(settings.MaxLeftRumbleToATFrequency, ScaledLeftActuator)
							: min(settings.MaxRightRumbleToATFrequency,
								  ScaledRightActuator);

					// set static threshold
					leftForces[2] = 20;
					rightForces[2] = 20;

					controller.SetLeftTrigger(Trigger::Pulse_B, leftForces[0],
											  leftForces[1], leftForces[2], 0, 0, 0, 0, 0,
											  0, 0, 0);
					controller.SetRightTrigger(Trigger::Pulse_B, rightForces[0],
											   rightForces[1], rightForces[2], 0, 0, 0, 0,
											   0, 0, 0, 0);
				}
			}
		skiphapticstotriggers:

			if (settings.FullyRetractTriggers)
			{
				if (settings.RumbleToAT || settings.RumbleToAT_RigidMode)
				{
					if (settings.HapticsToTriggers &&
						(peaks.LeftActuator > 0 || peaks.RightActuator > 0))
					{
						goto skiprumbleretract;
					}

					if (settings.lrumble == 0)
					{
						if (!settings.SwapTriggersRumbleToAT)
							controller.SetLeftTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0,
													  0, 0, 0, 0);
						else
							controller.SetRightTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0,
													   0, 0, 0, 0);
					}
					if (settings.rrumble == 0)
					{
						if (!settings.SwapTriggersRumbleToAT)
							controller.SetRightTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0,
													   0, 0, 0, 0);
						else
							controller.SetLeftTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0,
													  0, 0, 0, 0);
					}
				}

			skiprumbleretract:
				if (settings.HapticsToTriggers)
				{
					if (settings.lrumble > 0 || settings.rrumble > 0)
						goto skipretract;

					if (peaks.LeftActuator <= 0)
					{
						if (!settings.SwapTriggersRumbleToAT)
							controller.SetLeftTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0,
													  0, 0, 0, 0);
						else
							controller.SetRightTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0,
													   0, 0, 0, 0);
					}
					if (peaks.RightActuator <= 0)
					{
						if (!settings.SwapTriggersRumbleToAT)
							controller.SetRightTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0,
													   0, 0, 0, 0);
						else
							controller.SetLeftTrigger(Trigger::Rigid_B, 0, 0, 0, 0, 0, 0, 0,
													  0, 0, 0, 0);
					}
				}
			}
		skipretract:

			if (settings.TriggersAsButtons && !settings.CurrentlyUsingUDP &&
				settings.emuStatus != None)
			{
				controller.SetLeftTrigger(Trigger::Rigid, 40, 255, 255, 255, 255, 255,
										  255, 255, 255, 255, 255);
				controller.SetRightTrigger(Trigger::Rigid, 40, 255, 255, 255, 255, 255,
										   255, 255, 255, 255, 255);
			}

			if (settings.AudioToLED && !settings.CurrentlyUsingUDP &&
				(settings.emuStatus != DS4 || settings.OverrideDS4Lightbar) &&
				X360animationplayed)
			{
				int peak = static_cast<int>(MyUtils::GetCurrentAudioPeak() * 500);

				if (settings.HapticsToLED)
				{
					float CurrentHighestPeak =
						max(peaks.LeftActuator, peaks.RightActuator);
					peak = MyUtils::scaleFloatToInt(CurrentHighestPeak,
													settings.HapticsAndSpeakerToLedScale);
				}
				else if (settings.SpeakerToLED)
				{
					peak = MyUtils::scaleFloatToInt(peaks.Speaker,
													settings.HapticsAndSpeakerToLedScale);
				}

				int r = 0, g = 0, b = 0;

				if (peak <= settings.QUIET_THRESHOLD)
				{
					float t = static_cast<float>(peak) / settings.QUIET_THRESHOLD;
					r = static_cast<int>(t * settings.QUIET_COLOR[0]);
					g = static_cast<int>(t * settings.QUIET_COLOR[1]);
					b = static_cast<int>(t * settings.QUIET_COLOR[2]);
				}
				else if (peak <= settings.MEDIUM_THRESHOLD)
				{
					float t = static_cast<float>(peak - settings.QUIET_THRESHOLD) /
							  (settings.MEDIUM_THRESHOLD - settings.QUIET_THRESHOLD);
					r = static_cast<int>(
						settings.QUIET_COLOR[0] +
						t * (settings.MEDIUM_COLOR[0] - settings.QUIET_COLOR[0]));
					g = static_cast<int>(
						settings.QUIET_COLOR[1] +
						t * (settings.MEDIUM_COLOR[1] - settings.QUIET_COLOR[1]));
					b = static_cast<int>(
						settings.QUIET_COLOR[2] +
						t * (settings.MEDIUM_COLOR[2] - settings.QUIET_COLOR[2]));
				}
				else
				{
					float t = static_cast<float>(peak - settings.MEDIUM_THRESHOLD) /
							  (255 - settings.MEDIUM_THRESHOLD);
					r = static_cast<int>(
						settings.MEDIUM_COLOR[0] +
						t * (settings.LOUD_COLOR[0] - settings.MEDIUM_COLOR[0]));
					g = static_cast<int>(
						settings.MEDIUM_COLOR[1] +
						t * (settings.LOUD_COLOR[1] - settings.MEDIUM_COLOR[1]));
					b = static_cast<int>(
						settings.MEDIUM_COLOR[2] +
						t * (settings.LOUD_COLOR[2] - settings.MEDIUM_COLOR[2]));
				}

				controller.SetLightbar(r, g, b);
			}

			if (settings.DiscoMode && !settings.CurrentlyUsingUDP &&
				(settings.emuStatus != DS4 || settings.OverrideDS4Lightbar) &&
				X360animationplayed)
			{
				constexpr int DEFAULT_SPEED = 1;
				int speed = settings.DiscoSpeed;
				int step = DEFAULT_SPEED * speed;

				controller.SetLightbar(led[0], led[1], led[2]);

				switch (colorState)
				{
				case 0:
					led[1] += step;
					if (led[1] >= 255)
						colorState = 1;
					break;

				case 1:
					led[0] -= step;
					if (led[0] <= 0)
						colorState = 2;
					break;

				case 2:
					led[2] += step;
					if (led[2] >= 255)
						colorState = 3;
					break;

				case 3:
					led[1] -= step;
					if (led[1] <= 0)
						colorState = 4;
					break;

				case 4:
					led[0] += step;
					if (led[0] >= 255)
						colorState = 5;
					break;

				case 5:
					led[2] -= step;
					if (led[2] <= 0)
						colorState = 0;
					break;
				}

				led[0] = std::clamp(led[0], 0, 255);
				led[1] = std::clamp(led[1], 0, 255);
				led[2] = std::clamp(led[2], 0, 255);
			}

			if (settings.BatteryLightbar && !settings.CurrentlyUsingUDP &&
				(settings.emuStatus != DS4 || settings.OverrideDS4Lightbar) &&
				X360animationplayed)
			{
				int batteryLevel = controller.State.battery.Level;

				if (batteryLevel > 20)
				{
					batteryLevel = std::max<int>(0, std::min<int>(100, batteryLevel));

					int red = (100 - batteryLevel) * 255 / 100;
					int green = batteryLevel * 255 / 100;
					int blue = 0;
					controller.SetLightbar(red, green, blue);
				}
				else
				{
					switch (colorStateBattery)
					{
					case 0:
						led[0] += step;
						if (led[0] == 255)
							colorStateBattery = 1;
						break;
					case 1:
						led[0] -= step;
						if (led[0] == 0)
							colorStateBattery = 0;
						break;
					}

					controller.SetLightbar(led[0], 0, 0);
				}
			}

			if (controller.Connected && settings.BatteryPlayerLed && settings.DisablePlayerLED && !settings.CurrentlyUsingUDP &&
				(settings.emuStatus != DS4 || settings.OverrideDS4Lightbar) &&
				X360animationplayed)
			{

				int batteryLevel = controller.State.battery.Level;

				if (batteryLevel > 0 && batteryLevel <= 100)
				{
					batteryLevel = std::max<int>(0, std::min<int>(100, batteryLevel));

					controller.SetPlayerLED(0);
					if (batteryLevel > 5)
					{
						controller.AddPlayerLED(0x01);
						if (showBatteryLevel1 && batteryLevel < 10 && !controller.IsCharging())
						{
							showToast(settings.player, batteryLevel);
							showBatteryLevel1 = false;
						}
					}
					else if (showBatteryLevel2 && !controller.IsCharging())
					{
						showToast(settings.player, batteryLevel);
						showBatteryLevel2 = false;
					}

					if (batteryLevel > 20)
					{
						controller.AddPlayerLED(0x02);
					}

					if (batteryLevel > 40)
					{
						controller.AddPlayerLED(0x04);
					}

					if (batteryLevel > 60)
					{
						controller.AddPlayerLED(0x08);
					}

					if (batteryLevel > 80)
					{
						controller.AddPlayerLED(0x16);
					}
				}
			}

			controller.SetDisableLightbar(settings.DisableLightbar);

			if (!settings.DisableLightbar && !settings.AudioToLED && !settings.DiscoMode &&
				!settings.BatteryLightbar &&
				(settings.emuStatus != DS4 || settings.OverrideDS4Lightbar) &&
				!settings.CurrentlyUsingUDP && X360animationplayed)
			{
				controller.SetLightbar(
					MyUtils::scaleFloatToInt(settings.colorPickerColor[0], 1),
					MyUtils::scaleFloatToInt(settings.colorPickerColor[1], 1),
					MyUtils::scaleFloatToInt(settings.colorPickerColor[2], 1));
			}

			if (settings.MicScreenshot)
			{
				if (controller.State.micBtn && !MicShortcutTriggered &&
					!controller.State.DpadDown && !controller.State.DpadLeft &&
					!controller.State.DpadRight && !controller.State.DpadUp &&
					!controller.State.touchBtn && !controller.State.triangle &&
					!controller.State.L1 && !controller.State.L2)
				{
					controller.PlayHaptics("screenshot");
					MyUtils::TakeScreenShot();
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					filesystem::create_directories(MyUtils::GetImagesFolderPath() +
												   "\\DualSenseY\\");
					string filename = MyUtils::GetImagesFolderPath() + "\\DualSenseY\\" +
									  MyUtils::currentDateTimeWMS() + ".bmp";
					MyUtils::SaveBitmapFromClipboard(filename.c_str());
					MicShortcutTriggered = true;
				}
			}

			if (settings.MicFunc)
			{
				if (controller.State.micBtn && !MicShortcutTriggered)
				{
					if (!mic)
					{
						controller.SetMicrophoneLED(true, false);
						controller.SetMicrophoneVolume(0);
						mic = true;
					}
					else
					{
						controller.SetMicrophoneLED(false, false);
						controller.SetMicrophoneVolume(80);
						mic = false;
					}
					MicShortcutTriggered = true;
				}
			}

			if (udpServer.isActive && settings.UseUDP == true)
			{
				if (firstTimeUDP)
				{
					preUDP = settings.ControllerInput;
					firstTimeUDP = false;
				}
				udpServer.MacAddress = controller.GetMACAddress();
				settings.CurrentlyUsingUDP = true;
				settings.L2UDPDeadzone = udpServer.thisSettings.L2UDPDeadzone;
				settings.R2UDPDeadzone = udpServer.thisSettings.R2UDPDeadzone;
				udpServer.Battery = controller.State.battery.Level;
				controller.SetLightbar(udpServer.thisSettings.ControllerInput.Red,
									   udpServer.thisSettings.ControllerInput.Green,
									   udpServer.thisSettings.ControllerInput.Blue);
				controller.SetLeftTrigger(
					udpServer.thisSettings.ControllerInput.LeftTriggerMode,
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[0],
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[1],
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[2],
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[3],
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[4],
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[5],
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[6],
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[7],
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[8],
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[9],
					udpServer.thisSettings.ControllerInput.LeftTriggerForces[10]);

				controller.SetRightTrigger(
					udpServer.thisSettings.ControllerInput.RightTriggerMode,
					udpServer.thisSettings.ControllerInput.RightTriggerForces[0],
					udpServer.thisSettings.ControllerInput.RightTriggerForces[1],
					udpServer.thisSettings.ControllerInput.RightTriggerForces[2],
					udpServer.thisSettings.ControllerInput.RightTriggerForces[3],
					udpServer.thisSettings.ControllerInput.RightTriggerForces[4],
					udpServer.thisSettings.ControllerInput.RightTriggerForces[5],
					udpServer.thisSettings.ControllerInput.RightTriggerForces[6],
					udpServer.thisSettings.ControllerInput.RightTriggerForces[7],
					udpServer.thisSettings.ControllerInput.RightTriggerForces[8],
					udpServer.thisSettings.ControllerInput.RightTriggerForces[9],
					udpServer.thisSettings.ControllerInput.RightTriggerForces[10]);

				while (!udpServer.thisSettings.AudioPlayQueue.empty())
				{
					AudioPlay &play = udpServer.thisSettings.AudioPlayQueue.front();

					controller.LoadSound("UDP-" + play.File, play.File);
					controller.PlayHaptics("UDP-" + play.File,
										   play.DontPlayIfAlreadyPlaying, play.Loop);

					cout << "Play: " << play.File << " | "
						 << play.DontPlayIfAlreadyPlaying << " | " << play.Loop << endl;
					udpServer.thisSettings.AudioPlayQueue.erase(
						udpServer.thisSettings.AudioPlayQueue.begin());
				}

				while (!udpServer.thisSettings.AudioEditQueue.empty())
				{
					AudioEdit &edit = udpServer.thisSettings.AudioEditQueue.front();

					switch (edit.EditType)
					{
					case AudioEditType::Pitch:
						controller.SetSoundPitch("UDP-" + edit.File, edit.Value);
						break;
					case AudioEditType::Volume:
						controller.SetSoundVolume("UDP-" + edit.File, edit.Value);
						break;
					case AudioEditType::Stop:
						controller.StopHaptics("UDP-" + edit.File);
						break;
					case AudioEditType::StopAll:
						controller.StopAllHaptics();
						break;
					}

					// cout << "Edit: " << edit.File << " | " << edit.EditType << " | " <<
					// edit.Value << endl;
					udpServer.thisSettings.AudioEditQueue.erase(
						udpServer.thisSettings.AudioEditQueue.begin());
				}

				std::chrono::high_resolution_clock::time_point Now =
					std::chrono::high_resolution_clock::now();
				if ((Now - udpServer.LastTime) > 15s)
				{
					udpServer.isActive = false;
					settings.CurrentlyUsingUDP = false;
					firstTimeUDP = true;
					settings.ControllerInput = preUDP;
					controller.StopSoundsThatStartWith("UDP-");
				}
			}

			if (settings.lrumble > 0 || settings.rrumble > 0)
			{
				controller.UseRumbleNotHaptics(true);
			}
			else
			{
				controller.UseRumbleNotHaptics(false);
			}

			if (settings.DisablePlayerLED && !settings.BatteryPlayerLed)
			{
				controller.SetPlayerLED(0);
			}
			else if (!settings.DisablePlayerLED)
			{
				controller.SetPlayerLED(settings.player);
			}

			controller.SetSpeakerVolume(settings.ControllerInput.SpeakerVolume);
			controller.SetHeadsetVolume(settings.ControllerInput.HeadsetVolume);
			controller.UseHeadsetNotSpeaker(settings.UseHeadset);
			controller.SetRumble(settings.lrumble, settings.rrumble);
			controller.SetLedBrightness(settings.ControllerInput._Brightness);
			controller.EnableImprovedRumbleEmulation(
				settings.ControllerInput.ImprovedRumbleEmulation);
			controller.SetRumbleReduction(settings.ControllerInput.RumbleReduction);

			if (controller.Connected)
			{
				if (!settings.StopWriting)
				{
					controller.Write();
				}
				if (!settings.DisableLightbar && (settings.AudioToLED || settings.DiscoMode))
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				else
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				}
			}
			else
			{
				lastTimeFailedToWrite = std::chrono::high_resolution_clock::now();
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		}

		lastMic = controller.State.micBtn;
	}
}

void setTaskbarIcon(GLFWwindow *window)
{
	int width, height, nrChannels;
	unsigned char *img =
		stbi_load("utilities/icon.png", &width, &height, &nrChannels, 4);

	if (img)
	{
		GLFWimage icon;
		icon.width = width;
		icon.height = height;
		icon.pixels = img;

		// Set the window icon
		glfwSetWindowIcon(window, 1, &icon);

		// Free the image memory after setting the icon
		stbi_image_free(img);
	}
	else
	{
		// Handle error loading image
		printf("Failed to load icon\n");
	}
}

static bool IsMinimized = false;
void window_iconify_callback(GLFWwindow *window, int iconified)
{
	if (iconified)
	{
		IsMinimized = true;
	}
	else
	{
		IsMinimized = false;
	}
}

std::vector<std::wstring> icons = {
	L"icon100",
	L"icon80",
	L"icon60",
	L"icon40",
	L"icon20",
	L"icon"};

HICON *hIcon; // default icon
std::unordered_map<std::wstring, HICON> iconMap;

NOTIFYICONDATAW nid;
std::string CurrentController = "";
HINSTANCE g_hInstance;
HWND mainWindow;
HWND g_trayHwnd;
GLFWwindow *window;
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW 1002

void mTrayUpdate()
{
	while (!stop_thread)
	{
		if (CurrentController != "")
		{
			ControllerData data = controllerMap[CurrentController];
			Dualsense *connection = data.GetActiveConnection();
			if (connection != nullptr)
			{
				int batteryLevel = connection->State.battery.Level;
				int player = data.settings->player;

				wcscpy_s(nid.szTip, ARRAYSIZE(nid.szTip), std::format(L"MakeSense: {}# - {}%", player, batteryLevel).c_str());
				std::wstring iconstring = L"icon";
				if (batteryLevel > 5 && batteryLevel < 20)
				{
					iconstring += L"20";
				}
				else if (batteryLevel < 40)
				{
					iconstring += L"40";
				}
				else if (batteryLevel < 60)
				{
					iconstring += L"60";
				}
				else if (batteryLevel < 80)
				{
					iconstring += L"80";
				}
				else
				{
					iconstring += L"100";
				}

				if (connection->IsCharging())
				{
					iconstring += L"c";
				}
				hIcon = &iconMap[iconstring];
			}
		}
		else
		{
			hIcon = &iconMap[L"icon"];
			wcscpy_s(nid.szTip, sizeof(nid.szTip) / sizeof(wchar_t), L"MakeSense - no controllers connected"); // Tooltip shown on hover
		}
		nid.hIcon = *hIcon; // ← your loaded ico10n
		Shell_NotifyIconW(NIM_MODIFY, &nid);

		std::this_thread::sleep_for(std::chrono::milliseconds(600));
	}
}

void loadAllIcons()
{
	for (const auto &icon : icons)
	{
		std::wstring path = std::format(L"utilities\\{}.ico", icon);
		iconMap[icon] = (HICON)LoadImageW(
			NULL, path.c_str(), // Your relative path
			IMAGE_ICON,
			0, 0, // Use icon's default size
			LR_LOADFROMFILE | LR_DEFAULTSIZE);

		std::wstring iconc = icon + L"c";
		std::wstring path2 = std::format(L"utilities\\{}.ico", iconc);

		iconMap[iconc] = (HICON)LoadImageW(
			NULL, path2.c_str(), // Your relative path
			IMAGE_ICON,
			0, 0, // Use icon's default size
			LR_LOADFROMFILE | LR_DEFAULTSIZE);
	}

	hIcon = &iconMap[L"icon"];
}

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_TRAYICON)
	{
		switch (lParam)
		{
		case WM_RBUTTONUP:
		{
			POINT pt;
			GetCursorPos(&pt);

			HMENU hMenu = CreatePopupMenu();
			AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Show window");
			AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
			AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

			SetForegroundWindow(hwnd); // Needed to make menu disappear correctly
			TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
			DestroyMenu(hMenu);
		}
		break;
		case WM_LBUTTONDBLCLK:
			appConfig.ShowWindow = true;
			break;
		}
	}
	else if (msg == WM_COMMAND)
	{
		switch (LOWORD(wParam))
		{
		case ID_TRAY_EXIT:
			Shell_NotifyIconW(NIM_DELETE, new NOTIFYICONDATAW{.cbSize = sizeof(NOTIFYICONDATAW), .hWnd = hwnd, .uID = 1});
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			DestroyWindow(hwnd);
			PostQuitMessage(0);
			break;
		case ID_TRAY_SHOW:
			// MessageBoxW(nullptr, L"Show Window clicked!", L"Tray", MB_OK);
			appConfig.ShowWindow = true;
			break;
		}
	}
	else if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND CreateTrayWindow()
{
	const wchar_t CLASS_NAME[] = L"MyTrayWindow";

	WNDCLASSW wc = {};
	wc.lpfnWndProc = TrayWndProc;
	wc.hInstance = g_hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClassW(&wc);

	return CreateWindowExW(
		WS_EX_TOOLWINDOW, CLASS_NAME, L"HiddenTrayWindow",
		WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 1, 1,
		nullptr, nullptr, g_hInstance, nullptr);
}

void InitTrayIcon(HWND hwnd)
{
	nid = {};
	nid.cbSize = sizeof(NOTIFYICONDATAW);
	nid.hWnd = hwnd; // Your tray message window
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;																   // Your custom tray icon message
	nid.hIcon = *hIcon;																				   // ← your loaded icon
	wcscpy_s(nid.szTip, sizeof(nid.szTip) / sizeof(wchar_t), L"MakeSense - no controllers connected"); // Tooltip shown on hover

	Shell_NotifyIconW(NIM_ADD, &nid);
}

void RunTrayLoop()
{
	HWND hwnd = CreateTrayWindow();
	g_trayHwnd = hwnd;

	InitTrayIcon(hwnd);
	std::thread trayThread(mTrayUpdate);
	trayThread.detach();
	MSG msg;
	while (!stop_thread)
	{
		if (GetMessage(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	if (trayThread.joinable())
	{
		trayThread.join();
	}
}

int main()
{
	appConfig = Config::ReadAppConfigFromFile();

	if (appConfig.ShowConsole == false)
	{
		FreeConsole();
	}

	bool UpdateAvailable = false;

	if (appConfig.ElevateOnStartup)
	{
		MyUtils::ElevateNow();
	}
	else
	{
		cout << "Not elevating" << endl;
	}
	loadAllIcons();

	bool WasElevated = MyUtils::IsRunAsAdministrator();

	// Write default english to file
	Strings enStrings;
	nlohmann::json enJson = enStrings.to_json();
	filesystem::create_directories(Config::GetExecutableFolderPath() +
								   "\\localizations\\");
	std::ofstream EnglishFILE(Config::GetExecutableFolderPath() +
							  "\\localizations\\en.json");
	EnglishFILE << enJson.dump(4);
	EnglishFILE.close();
	enJson.clear();

	float soundpan = 1;

	Strings strings =
		ReadStringsFromFile(appConfig.Language); // Load language file

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	int InstancesCount = 0;
	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (strcmp(entry.szExeFile, "DSX.exe") == 0)
			{
				InstancesCount++;
			}
		}
	}

	if (InstancesCount > 1)
	{
		MessageBox(0, "MakeSense/DSX is already running!", "Error", 0);
		return 1;
	}

	CloseHandle(snapshot);

	// Setup window
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
	{
		MessageBox(0, "GLFW could not be initialized!", "Error", 0);
		return 1;
	}

	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100
	const char *glsl_version = "#version 100";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
	// GL 3.2 + GLSL 150
	const char *glsl_version = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);		   // Required on Mac
#else
	// GL 3.0 + GLSL 130
	const char *glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

	string title = "MakeSense";
	// Create window with graphics context
	window = glfwCreateWindow(static_cast<std::int32_t>(1200),
							  static_cast<std::int32_t>(900), title.c_str(),
							  nullptr, nullptr);

	mainWindow = glfwGetWin32Window(window);
	if (window == nullptr)
	{
		MessageBox(0, "Window could not be created!", "Error", 0);
		return 1;
	}
	// createTray();
	g_hInstance = GetModuleHandle(nullptr);
	std::thread trayThread(RunTrayLoop);
	trayThread.detach();
	glfwMakeContextCurrent(window);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	glfwSwapInterval(1); // VSYNC

	int imageWidth, imageHeight, channels;
	stbi_uc *backgroundTexture = stbi_load("textures\\background.png",
										   &imageWidth, &imageHeight, &channels,
										   4); // Load as RGBA
	bool BackgroundTextureLoaded = false;

	if (!backgroundTexture)
	{
		// Error handling
		printf("Failed to load texture\n");
	}
	else
	{
		BackgroundTextureLoaded = true;
	}

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, backgroundTexture);

	stbi_image_free(
		backgroundTexture); // Free the image data after loading into GPU

	vector<thread> readThreads;	 // Store the threads for each controller
	vector<thread> writeThreads; // Audio to LED threads
	vector<thread> emuThreads;
	bool firstController = true;
	vector<std::string> ControllerID = DualsenseUtils::EnumerateControllerIDs();

	int ControllerCount = DualsenseUtils::GetControllerCount();
	std::chrono::high_resolution_clock::time_point LastControllerCheck =
		std::chrono::high_resolution_clock::now();

	MyUtils::InitializeAudioEndpoint();

	UDP udpServer;

	ImGuiIO &io = ImGui::GetIO();

	static const ImWchar polishGlyphRange[] = {
		0x0020,
		0x00FF, // Basic Latin + Latin Supplement
		0x0100,
		0x017F, // Latin Extended-A
		0x0180,
		0x024F, // Latin Extended-B (if you need even more coverage)
		0};
	if (appConfig.Language == "ja")
		ImFont *font_title = io.Fonts->AddFontFromFileTTF(
			std::string(Config::GetExecutableFolderPath() +
						"\\fonts\\NotoSansJP-Bold.ttf")
				.c_str(),
			18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	else if (appConfig.Language == "zh")
		ImFont *font_title = io.Fonts->AddFontFromFileTTF(
			std::string(Config::GetExecutableFolderPath() +
						"\\fonts\\NotoSansSC-Bold.otf")
				.c_str(),
			18.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
	else if (appConfig.Language == "ko")
		ImFont *font_title = io.Fonts->AddFontFromFileTTF(
			std::string(Config::GetExecutableFolderPath() +
						"\\fonts\\NotoSansKR-Bold.ttf")
				.c_str(),
			18.0f, NULL, io.Fonts->GetGlyphRangesKorean());
	else if (appConfig.Language == "ru" || appConfig.Language == "uk")
		ImFont *font_title = io.Fonts->AddFontFromFileTTF(
			std::string(Config::GetExecutableFolderPath() +
						"\\fonts\\NotoSansRU-Bold.ttf")
				.c_str(),
			18.0f, NULL, io.Fonts->GetGlyphRangesCyrillic());
	else if (appConfig.Language == "vi")
		ImFont *font_title = io.Fonts->AddFontFromFileTTF(
			std::string(Config::GetExecutableFolderPath() +
						"\\fonts\\Roboto-Bold.ttf")
				.c_str(),
			18.0f, NULL, io.Fonts->GetGlyphRangesVietnamese());
	else
	{
		ImFont *font_title = io.Fonts->AddFontFromFileTTF(
			std::string(Config::GetExecutableFolderPath() +
						"\\fonts\\Roboto-Bold.ttf")
				.c_str(),
			18.0f, NULL, polishGlyphRange);
	}

	std::vector<const char *> languageItems;
	for (const auto &lang : languages)
	{
		languageItems.push_back(lang.c_str());
	}
	int selectedLanguageIndex = 0;

	// set language index
	for (int i = 0; i < languages.size(); i++)
	{
		if (languages[i] == appConfig.Language)
		{
			selectedLanguageIndex = i;
			break;
		}
	}

	setTaskbarIcon(window);
	glfwSetWindowIconifyCallback(window, window_iconify_callback);
	bool WindowHiddenOnStartup = false;

	if (appConfig.HideWindowOnStartup)
	{
		IsMinimized = true;
		WindowHiddenOnStartup = true;
		appConfig.ShowWindow = false;
	}

	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowRounding = 5.3f;
	style.FrameRounding = 2.3f;
	style.ScrollbarRounding = 0;
	if (appConfig.DefaultStyleFilepath != "")
		MyUtils::LoadImGuiColorsFromFilepath(style, appConfig.DefaultStyleFilepath);

	HWND hwnd = glfwGetWin32Window(window);
	BOOL useDarkMode = TRUE;

	glfwSwapInterval(0);

	if (useDarkMode)
	{
		if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
										 &useDarkMode, sizeof(useDarkMode))))
		{
			fprintf(stderr, "Failed to set dark mode for window.\n");
		}

		DWORD color = 0x000000;
		if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color,
										 sizeof(color))))
		{
			fprintf(stderr, "Failed to set caption color.\n");
		}
	}

	const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	int screenWidth = mode->width;
	int screenHeight = mode->height;
	bool ShowStyleEditWindow = false;
	float MainMenuBarHeight = 0;

	static bool show_popup = false;
	while (!glfwWindowShouldClose(window))
	{
		int windowWidth, windowHeight;
		glfwGetWindowSize(window, &windowWidth, &windowHeight);

		if (IsMinimized && appConfig.HideToTray ||
			IsMinimized && !appConfig.HideToTray && appConfig.HideWindowOnStartup &&
				WindowHiddenOnStartup)
		{
			glfwHideWindow(window);
			WindowHiddenOnStartup = false;
		}

		if (appConfig.ShowWindow)
		{
			glfwFocusWindow(window);
			glfwRestoreWindow(window);
			appConfig.ShowWindow = false;
			IsMinimized = false;
		}

		start_cycle();
		ImGui::NewFrame();
		// Get the viewport size (size of the application window)
		ImVec2 window_size = io.DisplaySize;

		// Set window flags to remove the ability to resize, move, or close the
		// window
		ImGuiWindowFlags window_flags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoFocusOnAppearing;

		// Begin the "Main" window and set its size and position
		ImGui::SetNextWindowSize(window_size);
		ImGui::SetNextWindowPos(ImVec2(0, 0)); // Position at the top-left corner

		io.FontGlobalScale = MyUtils::CalculateScaleFactor(
			screenHeight,
			screenHeight); // Adjust the scale factor as needed

		if (BackgroundTextureLoaded)
		{
			if (ImGui::Begin("Background", nullptr, window_flags))
			{
				ImVec2 availSize = ImGui::GetContentRegionAvail();
				ImGui::Image((ImTextureID)(intptr_t)textureID, availSize);
			}
			ImGui::End();
		}

		bool ShowNonControllerConfig = true;

		if (ShowStyleEditWindow)
		{
			if (ImGui::Begin(strings.StyleEditor.c_str(), &ShowStyleEditWindow,
							 ImGuiWindowFlags_NoCollapse))
			{
				if (!ImGui::IsWindowFocused())
				{
					ShowStyleEditWindow = false;
				}

				for (int i = 0; i < ImGuiCol_COUNT; ++i)
				{
					ImVec4 *color = &style.Colors[i];
					const char *colorName = ImGui::GetStyleColorName(i);

					ImGui::Text("%s:", colorName);
					ImGui::SameLine();

					ImGui::PushItemWidth(ImGui::GetWindowWidth() / 10.0f - 10);
					ImGui::SliderFloat(
						std::string(std::string("R##") + colorName).c_str(), &color->x,
						0.0f, 1.0f);
					ImGui::SameLine();
					ImGui::SliderFloat(
						std::string(std::string("G##") + colorName).c_str(), &color->y,
						0.0f, 1.0f);
					ImGui::SameLine();
					ImGui::SliderFloat(
						std::string(std::string("B##") + colorName).c_str(), &color->z,
						0.0f, 1.0f);
					ImGui::SameLine();
					ImGui::SliderFloat(
						std::string(std::string("A##") + colorName).c_str(), &color->w,
						0.0f, 1.0f);
					ImGui::PopItemWidth();
				}
			}
			ImGui::End();
		}

		ImGui::SetNextWindowSize(
			ImVec2(static_cast<float>(windowWidth),
				   static_cast<float>(windowHeight - MainMenuBarHeight)));
		ImGui::SetNextWindowPos(ImVec2(0, MainMenuBarHeight));
		if (ImGui::Begin("Main", nullptr, window_flags))
		{
			// Limit these functions for better CPU usage
			std::chrono::high_resolution_clock::time_point Now =
				std::chrono::high_resolution_clock::now();
			if ((Now - LastControllerCheck) > 3s)
			{
				LastControllerCheck = std::chrono::high_resolution_clock::now();
				ControllerID.clear();
				ControllerID = DualsenseUtils::EnumerateControllerIDs();
				ControllerCount = DualsenseUtils::GetControllerCount();
			}

			int nloop = 0;
			for (std::string &id : ControllerID)
			{
				bool IsPresent = false;
				ControllerData *data = nullptr;

				for (Dualsense &ds : DualSense)
				{
					if (ExtractGuidFromPath(id) != "" && ExtractGuidFromPath(id) == ExtractGuidFromPath(ds.GetPath()))
					{
						if (id != ds.GetPath())
						{
							auto it = controllerMap.find(ExtractGuidFromPath(id));
							if (it != controllerMap.end())
							{
								data = &it->second;
							}
							IsPresent = false;
						}
						else
						{
							IsPresent = true;
							break;
						}
					}
					else
					{
						nloop++;
					}
				}

				if (!IsPresent)
				{
					Dualsense x = Dualsense(id.c_str());
					if (x.Connected)
					{
						DualSense.push_back(x);
						std::string guid = ExtractGuidFromPath(id);
						// Read default config if present
						std::string configPath = "";
						MyUtils::GetConfigPathForController(configPath, guid);
						Settings fallback; // default object
						Settings *s = nullptr;
						if (data != nullptr)
						{
							s = data->settings;
						}
						else
						{
							Settings fallback;
							Settings temp;
							s = &fallback;
							if (configPath != "")
							{
								temp = Config::ReadFromFile(configPath);
								s = &temp;
								if (temp.emuStatus != None)
								{
									MyUtils::RunAsyncHidHideRequest(DualSense.back().GetPath(),
																	"hide");
								}
							}
							s = &ControllerSettings.emplace_back(*s);
						}

						s->ControllerInput.ID = id;

						if (firstController)
						{
							s->UseUDP = true;
							firstController = false;
							CurrentController = guid.c_str();
						}

						readThreads.emplace_back(readControllerState,
												 std::ref(DualSense.back()),
												 std::ref(*s));

						writeThreads.emplace_back(
							writeControllerState, std::ref(DualSense.back()),
							std::ref(*s), std::ref(udpServer));

						emuThreads.emplace_back(writeEmuController,
												std::ref(DualSense.back()),
												std::ref(*s));
						if (data == nullptr)
						{
							controllerMap[guid] = ControllerData(s);
							if (x.GetConnectionType() == Feature::BT)
							{
								controllerMap[guid].dualsenseBT = &DualSense.back();
								controllerMap[guid].readThreadBT = &readThreads.back();
								controllerMap[guid].writeThreadBT = &writeThreads.back();
								controllerMap[guid].emuThreadBT = &emuThreads.back();
							}
							else
							{
								controllerMap[guid].dualsenseUSB = &DualSense.back();
								controllerMap[guid].readThreadUSB = &readThreads.back();
								controllerMap[guid].writeThreadUSB = &writeThreads.back();
								controllerMap[guid].emuThreadUSB = &emuThreads.back();
							}
						}
						else
						{
							if (data->GetActiveConnection()->GetConnectionType() == Feature::BT)
							{
								data->dualsenseUSB = &DualSense.back();
								data->readThreadUSB = &readThreads.back();
								data->writeThreadUSB = &writeThreads.back();
								data->emuThreadUSB = &emuThreads.back();
							}
							else
							{
								data->dualsenseBT = &DualSense.back();
								data->readThreadBT = &readThreads.back();
								data->writeThreadBT = &writeThreads.back();
								data->emuThreadBT = &emuThreads.back();
							}
						}
					}
				}
			}
			if (CurrentController == "" && DualSense.size() > 0)
			{
				Dualsense front = DualSense.front();
				CurrentController = ExtractGuidFromPath(front.GetPath().c_str());
			}
			if (ImGui::BeginCombo("##", CurrentController.c_str()))
			{
				for (const auto &pair : controllerMap)
				{
					if (pair.first != "")
					{
						string path = pair.first;
						bool is_selected = (CurrentController == path.c_str());
						if (ImGui::Selectable(path.c_str(), is_selected))
						{
							CurrentController = path.c_str();
						}
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (!udpServer.isActive)
			{
				ImGui::SameLine();
				std::string udpStatusString =
					std::string(strings.UDPStatus + ": " + strings.Inactive);
				ImGui::Text(udpStatusString.c_str());
				ImGui::SameLine();
				static float color[3] = {255, 0, 0};
				ImGui::ColorEdit3(
					"##", color,
					ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker |
						ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoInputs |
						ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel |
						ImGuiColorEditFlags_NoSidePreview |
						ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder);
			}
			else
			{
				ImGui::SameLine();
				ImGui::Text(
					std::string(strings.UDPStatus + ": " + strings.Active).c_str());
				ImGui::SameLine();
				static float color[3] = {0, 255, 0};
				ImGui::ColorEdit3(
					"##", color,
					ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker |
						ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoInputs |
						ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel |
						ImGuiColorEditFlags_NoSidePreview |
						ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoBorder);
			}
			int ActiveDualsenseCount = DualSense.size();
			ImGui::Separator();

			for (int i = 0; i < ActiveDualsenseCount; i++)
			{
				DualSense[i].Reconnect();
				// DualSense[i].SetPlayerLED(i + 1); change to setplayer number
			}

			for (int i = 0; i < ControllerSettings.size(); i++)
			{
				ControllerSettings[i].player = i + 1;
			}
			auto it = controllerMap.find(CurrentController);
			if (it != controllerMap.end())
			{

				ControllerData &data = it->second;
				bool isNull = data.GetActiveConnection() == nullptr;
				Dualsense &dualsense = *data.GetActiveConnection();
				Settings &s = *data.settings;
				if (!isNull && dualsense.Connected)
				{

					if (ImGui::BeginMainMenuBar())
					{
						if (ImGui::BeginMenu(strings.ConfigHeader.c_str()))
						{
							if (ImGui::Combo(strings.Language.c_str(),
											 &selectedLanguageIndex, languageItems.data(),
											 languageItems.size()))
							{
								const std::string &selectedLanguage =
									languages[selectedLanguageIndex];
								appConfig.Language = selectedLanguage;
								Config::WriteAppConfigToFile(appConfig);
							}

							if (ImGui::Button(strings.SaveConfig.c_str()))
							{
								Config::WriteToFile(s);
							}
							Tooltip(strings.Tooltip_SaveConfig.c_str());

							if (ImGui::Button(strings.LoadConfig.c_str()))
							{
								std::string configPath = Config::GetConfigPath();
								if (configPath != "")
								{
									s = Config::ReadFromFile(configPath);
									if (s.emuStatus == None)
									{
										MyUtils::RunAsyncHidHideRequest(dualsense.GetPath(),
																		"show");
									}
									else
									{
										MyUtils::RunAsyncHidHideRequest(dualsense.GetPath(),
																		"hide");
									}
									s.ControllerInput.ID = dualsense.GetPath();
								}
							}
							Tooltip(strings.Tooltip_LoadConfig.c_str());

							if (ImGui::Button(strings.SetDefaultConfig.c_str()))
							{
								std::string configPath = Config::GetConfigPath();
								if (configPath != "")
								{
									MyUtils::WriteDefaultConfigPath(configPath,
																	CurrentController);
								}
							}
							Tooltip(strings.Tooltip_SetDefaultConfig.c_str());

							if (ImGui::Button(strings.RemoveDefaultConfig.c_str()))
							{
								MyUtils::RemoveConfig(CurrentController);
							}
							Tooltip(strings.Tooltip_RemoveDefaultConfig.c_str());

							if (ImGui::Checkbox(strings.RunAsAdmin.c_str(),
												&appConfig.ElevateOnStartup))
							{
								Config::WriteAppConfigToFile(appConfig);

								if (!MyUtils::IsRunAsAdministrator())
								{
									MyUtils::ElevateNow();
								}
							}
							Tooltip(strings.Tooltip_RunAsAdmin.c_str());

							if (ImGui::Checkbox(strings.HideToTray.c_str(),
												&appConfig.HideToTray))
							{
								Config::WriteAppConfigToFile(appConfig);
							}
							Tooltip(strings.Tooltip_HideToTray.c_str());

							if (ImGui::Checkbox(strings.HideWindowOnStartup.c_str(),
												&appConfig.HideWindowOnStartup))
							{
								Config::WriteAppConfigToFile(appConfig);
							}
							Tooltip(strings.Tooltip_HideWindowOnStartup.c_str());

							if (ImGui::Checkbox(strings.RunWithWindows.c_str(),
												&appConfig.RunWithWindows))
							{
								if (appConfig.RunWithWindows)
								{
									MyUtils::AddToStartup();
								}
								else
								{
									MyUtils::RemoveFromStartup();
								}

								Config::WriteAppConfigToFile(appConfig);
							}
							Tooltip(strings.RunWithWindows.c_str());

							if (ImGui::Checkbox(strings.ShowNotifications.c_str(),
												&appConfig.ShowNotifications))
							{
								if (appConfig.ShowNotifications)
								{
									MyUtils::AddNotifications();
								}
								else
								{
									MyUtils::RemoveNotifications();
								}

								Config::WriteAppConfigToFile(appConfig);
							}

							Tooltip(strings.Tooltip_ShowNotifications.c_str());

							if (ImGui::Checkbox(strings.ShowConsole.c_str(),
												&appConfig.ShowConsole))
							{
								Config::WriteAppConfigToFile(appConfig);
							}

							if (ImGui::Checkbox(
									strings.DisconnectAllBTDevicesOnExit.c_str(),
									&appConfig.DisconnectAllBTControllersOnExit))
							{
								Config::WriteAppConfigToFile(appConfig);
							}

							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu(strings.Style.c_str()))
						{
							if (ImGui::Button(strings.SaveStyleToFile.c_str()))
							{
								MyUtils::SaveImGuiColorsToFile(style);
							}

							if (ImGui::Button(strings.LoadStyleFromFile.c_str()))
							{
								MyUtils::LoadImGuiColorsFromFile(style);
							}

							if (ImGui::Button(strings.SetDefaultStyleFile.c_str()))
							{
								std::string stylePath = Config::GetStylePath();
								if (stylePath != "")
								{
									appConfig.DefaultStyleFilepath = stylePath;
									Config::WriteAppConfigToFile(appConfig);
								}
							}

							if (ImGui::Button(strings.RemoveDefaultStyle.c_str()))
							{
								appConfig.DefaultStyleFilepath = "";
								Config::WriteAppConfigToFile(appConfig);
							}

							if (ImGui::Button(strings.ResetStyle.c_str()))
							{
								style = ImGuiStyle();
							}

							if (ImGui::Button(strings.ShowEditMenu.c_str()))
							{
								ShowStyleEditWindow = true;
							}
							ImGui::EndMenu();
						}
						MainMenuBarHeight = ImGui::GetFrameHeight();
						ImGui::EndMainMenuBar();
					}

					dualsense.SetSoundVolume("screenshot",
											 s.ScreenshotSoundLoudness);
					DualsenseUtils::HapticFeedbackStatus HF_status =
						dualsense.GetHapticFeedbackStatus();
					const char *bt_or_usb =
						dualsense.GetConnectionType() == Feature::USB ? "USB"
																	  : "BT";
					ImGui::Text("%s %d | %s: %s | %s: %d%% | %s: %s",
								strings.ControllerNumberText.c_str(), 1,
								strings.USBorBTconnectionType.c_str(), bt_or_usb,
								strings.BatteryLevel.c_str(),
								dualsense.State.battery.Level,
								strings.Charging.c_str(), dualsense.IsCharging() ? strings.BoolYes.c_str() : strings.BoolNo.c_str());

					if (HF_status ==
						DualsenseUtils::HapticFeedbackStatus::BluetoothNotUSB)
					{
						if (ImGui::Button(strings.DisconnectBT.c_str()))
						{
							MyUtils::DisableBluetoothDevice(
								dualsense.GetMACAddress(false));
						}
					}

					if (s.StopWriting)
					{
						ImGui::TextColored(ImVec4(1, 0, 0, 1),
										   strings.CommunicationPaused.c_str());
						ImGui::SameLine();
						if (ImGui::SmallButton(strings.Resume.c_str()))
						{
							s.StopWriting = false;
							dualsense.PlayHaptics("resumed");
						}
					}

					if (!udpServer.isActive || !s.UseUDP)
					{
						if ((s.emuStatus != DS4 || s.OverrideDS4Lightbar))
						{
							if (ImGui::CollapsingHeader(strings.LedSection.c_str()))
							{
								ImGui::Separator();

								ImGui::Checkbox(strings.DisablePlayerLED.c_str(),
												&s.DisablePlayerLED);

								const char *playerLedBrightnessPreview = "";

								if (s.ControllerInput._Brightness ==
									Feature::Brightness::Bright)
								{
									playerLedBrightnessPreview = "+++";
								}
								else if (s.ControllerInput._Brightness ==
										 Feature::Brightness::Mid)
								{
									playerLedBrightnessPreview = "++";
								}
								else if (s.ControllerInput._Brightness ==
										 Feature::Brightness::Dim)
								{
									playerLedBrightnessPreview = "+";
								}

								ImGui::SetNextItemWidth(100);
								if (ImGui::BeginCombo(strings.PlayerLedBrightness.c_str(),
													  playerLedBrightnessPreview))
								{
									if (ImGui::Selectable("+++"))
									{
										s.ControllerInput._Brightness =
											Feature::Brightness::Bright;
									}

									if (ImGui::Selectable("++"))
									{
										s.ControllerInput._Brightness =
											Feature::Brightness::Mid;
									}

									if (ImGui::Selectable("+"))
									{
										s.ControllerInput._Brightness =
											Feature::Brightness::Dim;
									}

									ImGui::EndCombo();
								}
								ImGui::Checkbox(strings.DisableLightbar.c_str(),
												&s.DisableLightbar);
								Tooltip(strings.Tooltip_DisableLightbar.c_str());

								if (!s.DisableLightbar && !s.DiscoMode && !s.BatteryLightbar)
								{
									ImGui::Checkbox(strings.AudioToLED.c_str(),
													&s.AudioToLED);
									Tooltip(strings.Tooltip_AudioToLED.c_str());
								}

								if (!s.DisableLightbar && !s.AudioToLED && !s.BatteryLightbar)
								{
									ImGui::Checkbox(strings.DiscoMode.c_str(), &s.DiscoMode);
									Tooltip(strings.Tooltip_DiscoMode.c_str());
									ImGui::SameLine();
									ImGui::SetNextItemWidth(100);
									ImGui::SliderInt(
										std::string(strings.Speed + "##Disco").c_str(),
										&s.DiscoSpeed, 1, 10);
								}

								if (s.AudioToLED && !s.DisableLightbar)
								{
									ImGui::Separator();
									if (HF_status ==
										DualsenseUtils::HapticFeedbackStatus::Working)
									{
										if (!s.SpeakerToLED)
										{
											ImGui::Checkbox(
												strings.UseControllerActuatorsInsteadOfSystemAudio
													.c_str(),
												&s.HapticsToLED);
										}
										if (!s.HapticsToLED)
										{
											ImGui::Checkbox(
												strings.UseControllerSpeakerInsteadOfSystemAudio
													.c_str(),
												&s.SpeakerToLED);
										}
										if (s.SpeakerToLED || s.HapticsToLED)
										{
											ImGui::SameLine();
											ImGui::SetNextItemWidth(100);
											ImGui::SliderFloat(strings.Scale.c_str(),
															   &s.HapticsAndSpeakerToLedScale,
															   0.01f, 1.0f);
										}
									}
									else if (DualsenseUtils::HapticFeedbackStatus::
												 BluetoothNotUSB)
									{
										s.SpeakerToLED = false;
										s.HapticsToLED = false;
									}

									ImGui::Text(strings.QuietColor.c_str());
									ImGui::SliderInt(string(strings.LED_Red + "##q").c_str(),
													 &s.QUIET_COLOR[0], 0, 255);
									ImGui::SliderInt(
										string(strings.LED_Green + "##q").c_str(),
										&s.QUIET_COLOR[1], 0, 255);
									ImGui::SliderInt(string(strings.LED_Blue + "##q").c_str(),
													 &s.QUIET_COLOR[2], 0, 255);
									ImGui::Spacing();

									ImGui::Text(strings.MediumColor.c_str());
									ImGui::SliderInt(string(strings.LED_Red + "##m").c_str(),
													 &s.MEDIUM_COLOR[0], 0, 255);
									ImGui::SliderInt(
										string(strings.LED_Green + "##m").c_str(),
										&s.MEDIUM_COLOR[1], 0, 255);
									ImGui::SliderInt(string(strings.LED_Blue + "##m").c_str(),
													 &s.MEDIUM_COLOR[2], 0, 255);
									ImGui::Spacing();

									ImGui::Text(strings.LoudColor.c_str());
									ImGui::SliderInt(string(strings.LED_Red + "##l").c_str(),
													 &s.LOUD_COLOR[0], 0, 255);
									ImGui::SliderInt(
										string(strings.LED_Green + "##l").c_str(),
										&s.LOUD_COLOR[1], 0, 255);
									ImGui::SliderInt(string(strings.LED_Blue + "##l").c_str(),
													 &s.LOUD_COLOR[2], 0, 255);
									ImGui::Spacing();

									ImGui::Separator();
								}

								if (!s.DiscoMode && !s.AudioToLED)
								{
									ImGui::Checkbox(strings.BatteryLightbar.c_str(),
													&s.BatteryLightbar);
									Tooltip(strings.Tooltip_BatteryLightbar.c_str());
								}

								if (s.DisablePlayerLED)
								{
									ImGui::Checkbox(strings.BatteryPlayerLed.c_str(),
													&s.BatteryPlayerLed);
									Tooltip(strings.Tooltip_BatteryPlayerLed.c_str());
								}

								if (!s.DisableLightbar && !s.AudioToLED && !s.DiscoMode && !s.BatteryLightbar)
								{
									ImGui::SetNextItemWidth(350);
									ImGui::ColorPicker3(strings.LightbarColor.c_str(),
														s.colorPickerColor);
								}

								ImGui::Separator();
							}
						}
						else
						{
							ImGui::Text(strings.LED_DS4_Unavailable.c_str());
						}

						if (ImGui::CollapsingHeader(strings.AdaptiveTriggers.c_str()))
						{
							ImGui::Checkbox(strings.RumbleToAT.c_str(), &s.RumbleToAT);
							Tooltip(strings.Tooltip_RumbleToAT.c_str());

							if (HF_status ==
								DualsenseUtils::HapticFeedbackStatus::Working)
							{
								ImGui::Checkbox(strings.HapticsToTriggers.c_str(),
												&s.HapticsToTriggers);
								Tooltip(strings.Tooltip_HapticsToTriggers.c_str());
								if (s.HapticsToTriggers)
								{
									ImGui::SameLine();
									ImGui::SetNextItemWidth(100);
									ImGui::SliderFloat(strings.Scale.c_str(),
													   &s.AudioToTriggersMaxFloatValue, 0.01f,
													   10.0f);
								}
							}

							if (s.RumbleToAT || s.HapticsToTriggers)
							{
								ImGui::Checkbox(strings.FullyRetractWhenNoData.c_str(),
												&s.FullyRetractTriggers);

								ImGui::Checkbox(strings.SwapTriggersRumbleToAT.c_str(),
												&s.SwapTriggersRumbleToAT);
								Tooltip(strings.Tooltip_SwapTriggersRumbleToAT.c_str());

								ImGui::Checkbox(strings.RumbleToAT_RigidMode.c_str(),
												&s.RumbleToAT_RigidMode);
								Tooltip(strings.Tooltip_RumbleToAT_RigidMode.c_str());

								if (!s.RumbleToAT_RigidMode)
								{
									ImGui::SliderInt(strings.MaxLeftIntensity.c_str(),
													 &s.MaxLeftRumbleToATIntensity, 0, 255);
									Tooltip(strings.Tooltip_MaxIntensity.c_str());

									ImGui::SliderInt(strings.MaxLeftFrequency.c_str(),
													 &s.MaxLeftRumbleToATFrequency, 0, 25);
									Tooltip(strings.Tooltip_MaxFrequency.c_str());

									ImGui::Separator();

									ImGui::SliderInt(strings.MaxRightIntensity.c_str(),
													 &s.MaxRightRumbleToATIntensity, 0, 255);
									Tooltip(strings.Tooltip_MaxIntensity.c_str());

									ImGui::SliderInt(strings.MaxRightFrequency.c_str(),
													 &s.MaxRightRumbleToATFrequency, 0, 25);
									Tooltip(strings.Tooltip_MaxFrequency.c_str());
								}
								else
								{
									ImGui::SliderInt(strings.MaxLeftIntensity.c_str(),
													 &s.MaxLeftTriggerRigid, 0, 255);
									Tooltip(strings.Tooltip_MaxIntensity.c_str());

									ImGui::SliderInt(strings.MaxRightIntensity.c_str(),
													 &s.MaxRightTriggerRigid, 0, 255);
									Tooltip(strings.Tooltip_MaxIntensity.c_str());
								}
							}

							ImGui::Separator();

							if (!s.RumbleToAT && !s.HapticsToTriggers)
							{
								ImGui::SetNextItemWidth(150);
								if (ImGui::BeginCombo(strings.TriggerFormat.c_str(),
													  s.triggerFormat.c_str()))
								{
									if (ImGui::Selectable("Sony", false))
									{
										s.triggerFormat = "Sony";
									}
									if (ImGui::Selectable("DSX", false))
									{
										s.triggerFormat = "DSX";
									}

									ImGui::EndCombo();
								}

								ImGui::SetNextItemWidth(150);
								if (ImGui::BeginCombo(strings.SelectedTrigger.c_str(),
													  s.curTrigger))
								{
									if (ImGui::Selectable("L2", false))
									{
										s.curTrigger = "L2";
									}
									if (ImGui::Selectable("R2", false))
									{
										s.curTrigger = "R2";
									}

									ImGui::EndCombo();
								}

								ImGui::SetNextItemWidth(300);

								if (s.triggerFormat == "DSX")
								{
									if (s.curTrigger == "L2")
									{
										if (ImGui::BeginCombo(strings.LeftTriggerMode.c_str(),
															  s.lmodestr.c_str()))
										{
											if (ImGui::Selectable("Off", false))
											{
												s.lmodestr = "Off";
												s.ControllerInput.LeftTriggerMode = Trigger::Off;
											}
											if (ImGui::Selectable("Rigid", false))
											{
												s.lmodestr = "Rigid";
												s.ControllerInput.LeftTriggerMode = Trigger::Rigid;
											}
											if (ImGui::Selectable("Pulse", false))
											{
												s.lmodestr = "Pulse";
												s.ControllerInput.LeftTriggerMode = Trigger::Pulse;
											}
											if (ImGui::Selectable("Rigid_A", false))
											{
												s.lmodestr = "Rigid_A";
												s.ControllerInput.LeftTriggerMode =
													Trigger::Rigid_A;
											}
											if (ImGui::Selectable("Rigid_B", false))
											{
												s.lmodestr = "Rigid_B";
												s.ControllerInput.LeftTriggerMode =
													Trigger::Rigid_B;
											}
											if (ImGui::Selectable("Rigid_AB", false))
											{
												s.lmodestr = "Rigid_AB";
												s.ControllerInput.LeftTriggerMode =
													Trigger::Rigid_AB;
											}
											if (ImGui::Selectable("Pulse_A", false))
											{
												s.lmodestr = "Pulse_A";
												s.ControllerInput.LeftTriggerMode =
													Trigger::Pulse_A;
											}
											if (ImGui::Selectable("Pulse_B", false))
											{
												s.lmodestr = "Pulse_B";
												s.ControllerInput.LeftTriggerMode =
													Trigger::Pulse_B;
											}
											if (ImGui::Selectable("Pulse_AB", false))
											{
												s.lmodestr = "Pulse_AB";
												s.ControllerInput.LeftTriggerMode =
													Trigger::Pulse_AB;
											}
											if (ImGui::Selectable("Calibration", false))
											{
												s.lmodestr = "Calibration";
												s.ControllerInput.LeftTriggerMode =
													Trigger::Calibration;
											}

											ImGui::EndCombo();
										}

										ImGui::SliderInt("LT 1", &s.L2DSXForces[0], 0, 255);
										ImGui::SliderInt("LT 2", &s.L2DSXForces[1], 0, 255);
										ImGui::SliderInt("LT 3", &s.L2DSXForces[2], 0, 255);
										ImGui::SliderInt("LT 4", &s.L2DSXForces[3], 0, 255);
										ImGui::SliderInt("LT 5", &s.L2DSXForces[4], 0, 255);
										ImGui::SliderInt("LT 6", &s.L2DSXForces[5], 0, 255);
										ImGui::SliderInt("LT 7", &s.L2DSXForces[6], 0, 255);
										ImGui::SliderInt("LT 8", &s.L2DSXForces[7], 0, 255);
										ImGui::SliderInt("LT 9", &s.L2DSXForces[8], 0, 255);
										ImGui::SliderInt("LT 10", &s.L2DSXForces[9], 0, 255);
										ImGui::SliderInt("LT 11", &s.L2DSXForces[10], 0, 255);
									}
									else
									{
										if (ImGui::BeginCombo(strings.RightTriggerMode.c_str(),
															  s.rmodestr.c_str()))
										{
											if (ImGui::Selectable("Off", false))
											{
												s.rmodestr = "Off";
												s.ControllerInput.RightTriggerMode = Trigger::Off;
											}
											if (ImGui::Selectable("Rigid", false))
											{
												s.rmodestr = "Rigid";
												s.ControllerInput.RightTriggerMode = Trigger::Rigid;
											}
											if (ImGui::Selectable("Pulse", false))
											{
												s.rmodestr = "Pulse";
												s.ControllerInput.RightTriggerMode = Trigger::Pulse;
											}
											if (ImGui::Selectable("Rigid_A", false))
											{
												s.rmodestr = "Rigid_A";
												s.ControllerInput.RightTriggerMode =
													Trigger::Rigid_A;
											}
											if (ImGui::Selectable("Rigid_B", false))
											{
												s.rmodestr = "Rigid_B";
												s.ControllerInput.RightTriggerMode =
													Trigger::Rigid_B;
											}
											if (ImGui::Selectable("Rigid_AB", false))
											{
												s.rmodestr = "Rigid_AB";
												s.ControllerInput.RightTriggerMode =
													Trigger::Rigid_AB;
											}
											if (ImGui::Selectable("Pulse_A", false))
											{
												s.rmodestr = "Pulse_A";
												s.ControllerInput.RightTriggerMode =
													Trigger::Pulse_A;
											}
											if (ImGui::Selectable("Pulse_B", false))
											{
												s.rmodestr = "Pulse_B";
												s.ControllerInput.RightTriggerMode =
													Trigger::Pulse_B;
											}
											if (ImGui::Selectable("Pulse_AB", false))
											{
												s.rmodestr = "Pulse_AB";
												s.ControllerInput.RightTriggerMode =
													Trigger::Pulse_AB;
											}
											if (ImGui::Selectable("Calibration", false))
											{
												s.rmodestr = "Calibration";
												s.ControllerInput.RightTriggerMode =
													Trigger::Calibration;
											}
											ImGui::EndCombo();
										}

										ImGui::SliderInt("RT 1", &s.R2DSXForces[0], 0, 255);
										ImGui::SliderInt("RT 2", &s.R2DSXForces[1], 0, 255);
										ImGui::SliderInt("RT 3", &s.R2DSXForces[2], 0, 255);
										ImGui::SliderInt("RT 4", &s.R2DSXForces[3], 0, 255);
										ImGui::SliderInt("RT 5", &s.R2DSXForces[4], 0, 255);
										ImGui::SliderInt("RT 6", &s.R2DSXForces[5], 0, 255);
										ImGui::SliderInt("RT 7", &s.R2DSXForces[6], 0, 255);
										ImGui::SliderInt("RT 8", &s.R2DSXForces[7], 0, 255);
										ImGui::SliderInt("RT 9", &s.R2DSXForces[8], 0, 255);
										ImGui::SliderInt("RT 10", &s.R2DSXForces[9], 0, 255);
										ImGui::SliderInt("RT 11", &s.R2DSXForces[10], 0, 255);
									}
								}
								else if (s.triggerFormat == "Sony")
								{
									if (s.curTrigger == "L2")
									{
										if (ImGui::BeginCombo(strings.LeftTriggerMode.c_str(),
															  s.lmodestrSony.c_str()))
										{
											if (ImGui::Selectable("Off", false))
											{
												s.lmodestrSony = "Off";
											}
											if (ImGui::Selectable("Feedback", false))
											{
												s.lmodestrSony = "Feedback";
											}
											if (ImGui::Selectable("Weapon", false))
											{
												s.lmodestrSony = "Weapon";
											}
											if (ImGui::Selectable("Vibration", false))
											{
												s.lmodestrSony = "Vibration";
											}
											if (ImGui::Selectable("Slope Feedback", false))
											{
												s.lmodestrSony = "Slope Feedback";
											}
											if (ImGui::Selectable("Multiple Position "
																  "Feedback",
																  false))
											{
												s.lmodestrSony =
													"Multiple Position "
													"Feedback";
											}
											if (ImGui::Selectable("Multiple Position "
																  "Vibration",
																  false))
											{
												s.lmodestrSony =
													"Multiple Position "
													"Vibration";
											}

											ImGui::EndCombo();
										}

										if (s.lmodestrSony == "Feedback")
										{
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Position.c_str(),
															 &s.L2Feedback[0], 0, 9);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Strength.c_str(),
															 &s.L2Feedback[1], 0, 8);
										}
										else if (s.lmodestrSony == "Weapon")
										{
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.StartPosition.c_str(),
															 &s.L2WeaponFeedback[0], 2, 7);

											if (s.L2WeaponFeedback[1] < s.L2WeaponFeedback[0])
												s.L2WeaponFeedback[1] = s.L2WeaponFeedback[0];

											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.EndPosition.c_str(),
															 &s.L2WeaponFeedback[1],
															 s.L2WeaponFeedback[0], 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Strength.c_str(),
															 &s.L2WeaponFeedback[2], 0, 8);
										}
										else if (s.lmodestrSony == "Vibration")
										{
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Position.c_str(),
															 &s.L2VibrationFeedback[0], 0, 9);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Amplitude.c_str(),
															 &s.L2VibrationFeedback[1], 0, 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Frequency.c_str(),
															 &s.L2VibrationFeedback[2], 0, 255);
										}
										else if (s.lmodestrSony == "Slope Feedback")
										{
											if (s.L2SlopeFeedback[1] < s.L2SlopeFeedback[0])
												s.L2SlopeFeedback[0] = s.L2SlopeFeedback[1];

											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.StartPosition.c_str(),
															 &s.L2SlopeFeedback[0], 0,
															 s.L2SlopeFeedback[1]);

											if (s.L2SlopeFeedback[1] < s.L2SlopeFeedback[0])
												s.L2SlopeFeedback[1] = s.L2SlopeFeedback[0];

											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.EndPosition.c_str(),
															 &s.L2SlopeFeedback[1],
															 s.L2SlopeFeedback[0], 9);

											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.StartStrength.c_str(),
															 &s.L2SlopeFeedback[2], 1, 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.EndStrength.c_str(),
															 &s.L2SlopeFeedback[3], 1, 8);
										}
										else if (s.lmodestrSony ==
												 "Multiple Position "
												 "Feedback")
										{
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(1))
																 .c_str(),
															 &s.L2MultiplePositionFeedback[0], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(2))
																 .c_str(),
															 &s.L2MultiplePositionFeedback[1], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(3))
																 .c_str(),
															 &s.L2MultiplePositionFeedback[2], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(4))
																 .c_str(),
															 &s.L2MultiplePositionFeedback[3], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(5))
																 .c_str(),
															 &s.L2MultiplePositionFeedback[4], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(6))
																 .c_str(),
															 &s.L2MultiplePositionFeedback[5], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(7))
																 .c_str(),
															 &s.L2MultiplePositionFeedback[6], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(8))
																 .c_str(),
															 &s.L2MultiplePositionFeedback[7], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(9))
																 .c_str(),
															 &s.L2MultiplePositionFeedback[8], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(10))
																 .c_str(),
															 &s.L2MultiplePositionFeedback[9], 0,
															 8);
										}
										else if (s.lmodestrSony ==
												 "Multiple Position "
												 "Vibration")
										{
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Frequency.c_str(),
															 &s.L2MultipleVibrationFeedback[0], 0,
															 255);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(1))
																 .c_str(),
															 &s.L2MultipleVibrationFeedback[1], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(2))
																 .c_str(),
															 &s.L2MultipleVibrationFeedback[2], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(3))
																 .c_str(),
															 &s.L2MultipleVibrationFeedback[3], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(4))
																 .c_str(),
															 &s.L2MultipleVibrationFeedback[4], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(5))
																 .c_str(),
															 &s.L2MultipleVibrationFeedback[5], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(6))
																 .c_str(),
															 &s.L2MultipleVibrationFeedback[6], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(7))
																 .c_str(),
															 &s.L2MultipleVibrationFeedback[7], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(8))
																 .c_str(),
															 &s.L2MultipleVibrationFeedback[8], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(9))
																 .c_str(),
															 &s.L2MultipleVibrationFeedback[9], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(10))
																 .c_str(),
															 &s.L2MultipleVibrationFeedback[10],
															 0, 8);
										}
									}
									else if (s.curTrigger == "R2")
									{
										if (ImGui::BeginCombo(strings.RightTriggerMode.c_str(),
															  s.rmodestrSony.c_str()))
										{
											if (ImGui::Selectable("Off", false))
											{
												s.rmodestrSony = "Off";
											}
											if (ImGui::Selectable("Feedback", false))
											{
												s.rmodestrSony = "Feedback";
											}
											if (ImGui::Selectable("Weapon", false))
											{
												s.rmodestrSony = "Weapon";
											}
											if (ImGui::Selectable("Vibration", false))
											{
												s.rmodestrSony = "Vibration";
											}
											if (ImGui::Selectable("Slope Feedback", false))
											{
												s.rmodestrSony = "Slope Feedback";
											}
											if (ImGui::Selectable("Multiple Position "
																  "Feedback",
																  false))
											{
												s.rmodestrSony =
													"Multiple Position "
													"Feedback";
											}
											if (ImGui::Selectable("Multiple Position "
																  "Vibration",
																  false))
											{
												s.rmodestrSony =
													"Multiple Position "
													"Vibration";
											}
											ImGui::EndCombo();
										}

										if (s.rmodestrSony == "Feedback")
										{
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Position.c_str(),
															 &s.R2Feedback[0], 0, 9);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Strength.c_str(),
															 &s.R2Feedback[1], 0, 8);
										}
										else if (s.rmodestrSony == "Weapon")
										{
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.StartPosition.c_str(),
															 &s.R2WeaponFeedback[0], 2, 7);

											if (s.R2WeaponFeedback[1] < s.R2WeaponFeedback[0])
												s.R2WeaponFeedback[1] = s.R2WeaponFeedback[0];

											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.EndPosition.c_str(),
															 &s.R2WeaponFeedback[1],
															 s.R2WeaponFeedback[0], 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Strength.c_str(),
															 &s.R2WeaponFeedback[2], 0, 8);
										}
										else if (s.rmodestrSony == "Vibration")
										{
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Position.c_str(),
															 &s.R2VibrationFeedback[0], 0, 9);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Amplitude.c_str(),
															 &s.R2VibrationFeedback[1], 0, 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Frequency.c_str(),
															 &s.R2VibrationFeedback[2], 0, 255);
										}
										else if (s.rmodestrSony == "Slope Feedback")
										{
											if (s.R2SlopeFeedback[1] < s.R2SlopeFeedback[0])
												s.R2SlopeFeedback[0] = s.R2SlopeFeedback[1];

											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.StartPosition.c_str(),
															 &s.R2SlopeFeedback[0], 0,
															 s.R2SlopeFeedback[1]);

											if (s.R2SlopeFeedback[1] < s.R2SlopeFeedback[0])
												s.R2SlopeFeedback[1] = s.R2SlopeFeedback[0];

											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.EndPosition.c_str(),
															 &s.R2SlopeFeedback[1],
															 s.R2SlopeFeedback[0], 9);

											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.StartStrength.c_str(),
															 &s.R2SlopeFeedback[2], 1, 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.EndStrength.c_str(),
															 &s.R2SlopeFeedback[3], 1, 8);
										}
										else if (s.rmodestrSony ==
												 "Multiple Position "
												 "Feedback")
										{
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(1))
																 .c_str(),
															 &s.R2MultiplePositionFeedback[0], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(2))
																 .c_str(),
															 &s.R2MultiplePositionFeedback[1], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(3))
																 .c_str(),
															 &s.R2MultiplePositionFeedback[2], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(4))
																 .c_str(),
															 &s.R2MultiplePositionFeedback[3], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(5))
																 .c_str(),
															 &s.R2MultiplePositionFeedback[4], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(6))
																 .c_str(),
															 &s.R2MultiplePositionFeedback[5], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(7))
																 .c_str(),
															 &s.R2MultiplePositionFeedback[6], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(8))
																 .c_str(),
															 &s.R2MultiplePositionFeedback[7], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(9))
																 .c_str(),
															 &s.R2MultiplePositionFeedback[8], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Strength + " " +
																		 std::to_string(10))
																 .c_str(),
															 &s.R2MultiplePositionFeedback[9], 0,
															 8);
										}
										else if (s.rmodestrSony ==
												 "Multiple Position "
												 "Vibration")
										{
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(strings.Frequency.c_str(),
															 &s.R2MultipleVibrationFeedback[0], 0,
															 255);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(1))
																 .c_str(),
															 &s.R2MultipleVibrationFeedback[1], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(2))
																 .c_str(),
															 &s.R2MultipleVibrationFeedback[2], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(3))
																 .c_str(),
															 &s.R2MultipleVibrationFeedback[3], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(4))
																 .c_str(),
															 &s.R2MultipleVibrationFeedback[4], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(5))
																 .c_str(),
															 &s.R2MultipleVibrationFeedback[5], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(6))
																 .c_str(),
															 &s.R2MultipleVibrationFeedback[6], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(7))
																 .c_str(),
															 &s.R2MultipleVibrationFeedback[7], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(8))
																 .c_str(),
															 &s.R2MultipleVibrationFeedback[8], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(9))
																 .c_str(),
															 &s.R2MultipleVibrationFeedback[9], 0,
															 8);
											ImGui::SetNextItemWidth(300);
											ImGui::SliderInt(std::string(strings.Amplitude + " " +
																		 std::to_string(10))
																 .c_str(),
															 &s.R2MultipleVibrationFeedback[10],
															 0, 8);
										}
									}
								}

								ImGui::Separator();
								ImGui::Spacing();
							}
						}
					}
					else
					{
						ImGui::Text(strings.LEDandATunavailableUDP.c_str());
					}

					if (ImGui::CollapsingHeader(strings.Motion.c_str()))
					{
						ImGui::BeginDisabled();
						ImGui::SetNextItemWidth(400);
						ImGui::SliderInt(std::string(strings.Gyroscope + " X").c_str(),
										 &dualsense.State.gyro.X, -9000, 9000);
						ImGui::SetNextItemWidth(400);
						ImGui::SliderInt(std::string(strings.Gyroscope + " Y").c_str(),
										 &dualsense.State.gyro.Y, -9000, 9000);
						ImGui::SetNextItemWidth(400);
						ImGui::SliderInt(std::string(strings.Gyroscope + " Z").c_str(),
										 &dualsense.State.gyro.Z, -9000, 9000);

						ImGui::SetNextItemWidth(400);
						ImGui::SliderInt(
							std::string(strings.Accelerometer + " X").c_str(),
							&dualsense.State.accelerometer.X, -9000, 9000);
						ImGui::SetNextItemWidth(400);
						ImGui::SliderInt(
							std::string(strings.Accelerometer + " Y").c_str(),
							&dualsense.State.accelerometer.Y, -9000, 9000);
						ImGui::SetNextItemWidth(400);
						ImGui::SliderInt(
							std::string(strings.Accelerometer + " Z").c_str(),
							&dualsense.State.accelerometer.Z, -9000, 9000);
						ImGui::EndDisabled();

						ImGui::Separator();
						ImGui::Checkbox(strings.GyroToMouse.c_str(), &s.GyroToMouse);
						ImGui::SameLine();
						Tooltip(strings.Tooltip_GyroToMouse.c_str());
						ImGui::SetNextItemWidth(250);
						ImGui::SliderFloat(
							std::string(strings.Sensitivity + "##gyrotomouse").c_str(),
							&s.GyroToMouseSensitivity, 0.001f, 0.030f);
						ImGui::Checkbox(strings.R2ToMouseClick.c_str(),
										&s.R2ToMouseClick);

						ImGui::Separator();
						if (s.emuStatus == None)
							ImGui::Text(strings.ControllerEmulationRequired.c_str());
						ImGui::Checkbox(strings.GyroToRightAnalogStick.c_str(),
										&s.GyroToRightAnalogStick);
						Tooltip(strings.Tooltip_GyroToRightAnalogStick.c_str());
						ImGui::SetNextItemWidth(250);
						ImGui::SliderFloat(std::string(strings.Sensitivity +
													   "##gyrotorightanalogstick")
											   .c_str(),
										   &s.GyroToRightAnalogStickSensitivity, 0.1f,
										   5.0f);
						ImGui::SetNextItemWidth(250);
						ImGui::SliderFloat(strings.GyroDeadzone.c_str(),
										   &s.GyroToRightAnalogStickDeadzone, 0.001,
										   0.300);
						ImGui::SetNextItemWidth(250);
						ImGui::SliderInt(strings.MinimumStickValue.c_str(),
										 &s.GyroToRightAnalogStickMinimumStickValue, 1,
										 127);

						ImGui::Separator();
						if (s.GyroToMouse || s.GyroToRightAnalogStick)
						{
							ImGui::SetNextItemWidth(250);
							ImGui::SliderInt(strings.L2Threshold.c_str(), &s.L2Threshold,
											 1, 255);
						}
					}

					if (HF_status == DualsenseUtils::HapticFeedbackStatus::Working &&
						s.RunAudioToHapticsOnStart && !s.WasAudioToHapticsRan &&
						!s.WasAudioToHapticsCheckboxChecked)
					{
						MyUtils::StartAudioToHaptics(dualsense.GetAudioDeviceName());
						s.WasAudioToHapticsRan = true;
					}
					/*
					if (ImGui::CollapsingHeader(strings.HapticFeedback.c_str()))
					{
						if (s.emuStatus == None)
						{
							ImGui::Text(strings.StandardRumble.c_str());
							Tooltip(strings.Tooltip_HapticFeedback.c_str());
							ImGui::SetNextItemWidth(300);
							ImGui::SliderInt(strings.LeftMotor.c_str(), &s.lrumble, 0,
											 255);
							ImGui::SetNextItemWidth(300);
							ImGui::SliderInt(strings.RightMotor.c_str(), &s.rrumble, 0,
											 255);
						}

						ImGui::SetNextItemWidth(300);
						ImGui::SliderInt(strings.RumbleReduction.c_str(),
										 &s.ControllerInput.RumbleReduction, 0x0, 0x7);

						ImGui::Checkbox(strings.ImprovedRumbleEmulation.c_str(),
										&s.ControllerInput.ImprovedRumbleEmulation);

						ImGui::Separator();

						if (ImGui::Checkbox(strings.RunAudioToHapticsOnStart.c_str(),
											&s.RunAudioToHapticsOnStart))
						{
							s.WasAudioToHapticsCheckboxChecked = true;
						}

						if (HF_status ==
							DualsenseUtils::HapticFeedbackStatus::Working)
						{
							ImGui::Checkbox(strings.TouchpadToHaptics.c_str(),
											&s.TouchpadToHaptics);
							dualsense.TouchpadToHaptics = s.TouchpadToHaptics;
							ImGui::SameLine();
							ImGui::SetNextItemWidth(100);
							ImGui::SliderFloat(strings.Frequency.c_str(),
											   &s.TouchpadToHapticsFrequency, 1.0f,
											   440.0f);
							dualsense.TouchpadToHapticsFrequency =
								s.TouchpadToHapticsFrequency;

							if (ImGui::Button(strings.StartAudioToHaptics.c_str()))
							{
								MyUtils::StartAudioToHaptics(
									dualsense.GetAudioDeviceName());
							};
							Tooltip(strings.Tooltip_StartAudioToHaptics.c_str());

							ImGui::SetNextItemWidth(300);
							ImGui::SliderInt(strings.SpeakerVolume.c_str(),
											 &s.ControllerInput.SpeakerVolume, 0, 0x7C,
											 "%d");
							Tooltip(strings.Tooltip_SpeakerVolume.c_str());
							ImGui::SetNextItemWidth(300);
							ImGui::SliderInt(strings.HeadsetVolume.c_str(),
											 &s.ControllerInput.HeadsetVolume, 0, 0x7C,
											 "%d");

							ImGui::Checkbox(strings.UseHeadset.c_str(), &s.UseHeadset);

							ImGui::Separator();
							ImGui::Text(MyUtils::WStringToString(
											dualsense.GetAudioDeviceInstanceID())
											.c_str());
							ImGui::Text(dualsense.GetAudioDeviceName());
						}
						else
						{
							if (HF_status ==
								DualsenseUtils::HapticFeedbackStatus::BluetoothNotUSB)
							{
								ImGui::Text(strings.HapticsUnavailable.c_str());
							}
							else if (HF_status == DualsenseUtils::HapticFeedbackStatus::
													  AudioDeviceNotFound)
							{
								ImGui::Text(
									strings.HapticsUnavailableNoAudioDevice.c_str());
							}
							else if (HF_status == DualsenseUtils::HapticFeedbackStatus::
													  AudioEngineNotInitialized)
							{
								ImGui::Text(strings.AudioEngineNotInitializedError.c_str());
							}
						}
					} */

					if (ImGui::CollapsingHeader(strings.AnalogSticks.c_str()))
					{
						ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
						float radius = 200.0f;
						ImVec2 center =
							ImVec2(radius + cursor_pos.x, radius + cursor_pos.y);
						ImU32 color = IM_COL32(255, 255, 255, 255);
						ImU32 colorRed = IM_COL32(255, 0, 0, 255);
						ImU32 colorGreen = IM_COL32(0, 255, 0, 255);

						ImGui::GetWindowDrawList()->AddCircle(
							center, radius, dualsense.State.L3 ? colorRed : color);
						ImGui::GetWindowDrawList()->AddCircle(
							center,
							ConvertRange(s.LeftAnalogDeadzone, 0, 127, 0, radius),
							colorGreen);

						ImVec2 leftStickPos =
							ImVec2(center.x + ConvertRange(dualsense.State.LX - 128,
														   -128, 128, -radius, radius),
								   center.y + ConvertRange(dualsense.State.LY - 128,
														   -128, 128, -radius, radius));
						ImGui::GetWindowDrawList()->AddCircleFilled(leftStickPos, 10.0f,
																	colorRed);

						char left_text[32];
						sprintf(left_text, "X: %d | Y: %d", dualsense.State.LX,
								dualsense.State.LY);
						ImVec2 left_text_size = ImGui::CalcTextSize(left_text);
						ImVec2 left_text_pos =
							ImVec2(center.x - left_text_size.x / 2, center.y + radius);
						ImGui::GetWindowDrawList()->AddText(left_text_pos, color,
															left_text);

						ImGui::Dummy(ImVec2(radius * 2, radius * 2));
						ImGui::SameLine();

						cursor_pos = ImGui::GetCursorScreenPos();
						ImVec2 right_center =
							ImVec2(radius + cursor_pos.x, radius + cursor_pos.y);

						ImGui::GetWindowDrawList()->AddCircle(
							right_center, radius,
							dualsense.State.R3 ? colorRed : color);
						ImGui::GetWindowDrawList()->AddCircle(
							right_center,
							ConvertRange(s.RightAnalogDeadzone, 0, 127, 0, radius),
							colorGreen);

						ImVec2 rightStickPos = ImVec2(
							right_center.x + ConvertRange(dualsense.State.RX - 128,
														  -128, 128, -radius, radius),
							right_center.y + ConvertRange(dualsense.State.RY - 128,
														  -128, 128, -radius, radius));
						ImGui::GetWindowDrawList()->AddCircleFilled(rightStickPos,
																	10.0f, colorRed);

						char right_text[32];
						sprintf(right_text, "X: %d | Y: %d", dualsense.State.RX,
								dualsense.State.RY);
						ImVec2 right_text_size = ImGui::CalcTextSize(right_text);
						ImVec2 right_text_pos =
							ImVec2(right_center.x - right_text_size.x / 2,
								   right_center.y + radius);
						ImGui::GetWindowDrawList()->AddText(right_text_pos, color,
															right_text);

						ImGui::Dummy(ImVec2(radius * 2, (radius * 2) + 40));
						ImGui::Separator();

						ImGui::Text(strings.GeneralSettings.c_str());
						if (!s.RightStickToMouse)
							ImGui::Checkbox(
								std::string(strings.LeftStickToMouse + "##L3toMouse")
									.c_str(),
								&s.LeftStickToMouse);
						if (!s.LeftStickToMouse)
							ImGui::Checkbox(
								std::string(strings.RightStickToMouse + "##R3toMouse")
									.c_str(),
								&s.RightStickToMouse);
						ImGui::SetNextItemWidth(300);
						ImGui::SliderFloat(
							std::string(strings.Sensitivity + "##AnalogStickToMouse")
								.c_str(),
							&s.LeftStickToMouseSensitivity, 0.01f, 0.20f);

						ImGui::Text(strings.EmulatedControllerSettings.c_str());
						ImGui::SetNextItemWidth(300);
						ImGui::SliderInt(strings.LeftAnalogStickDeadZone.c_str(),
										 &s.LeftAnalogDeadzone, 0, 127);
						ImGui::SetNextItemWidth(300);
						ImGui::SliderInt(strings.RightAnalogStickDeadZone.c_str(),
										 &s.RightAnalogDeadzone, 0, 127);
						ImGui::Checkbox(strings.TouchpadAsRightStick.c_str(),
										&s.TouchpadToRXRY);
					}

					if (ImGui::CollapsingHeader(strings._Touchpad.c_str()))
					{
						// Define the touchpad's preview size in the UI
						ImVec2 touchpadSize(
							300 * io.FontGlobalScale,
							140 * io.FontGlobalScale); // Adjusted to keep a similar
						// aspect ratio (1920:900)

						// Draw touchpad background
						ImGui::InvisibleButton("##touchpad_bg", touchpadSize);
						ImDrawList *drawList = ImGui::GetWindowDrawList();
						ImVec2 touchpadPos = ImGui::GetItemRectMin();
						int rcolor = dualsense.State.touchBtn ? 100 : 50;
						drawList->AddRectFilled(touchpadPos,
												ImVec2(touchpadPos.x + touchpadSize.x,
													   touchpadPos.y + touchpadSize.y),
												IM_COL32(rcolor, 50, 50, 255));

						// Scale touch points to fit within the UI preview
						if (dualsense.State.trackPadTouch0.IsActive)
						{
							float scaledX =
								(dualsense.State.trackPadTouch0.X / 1900.0f) *
								touchpadSize.x;
							float scaledY =
								(dualsense.State.trackPadTouch0.Y / 1100.0f) *
								touchpadSize.y;
							ImVec2 touchPos(touchpadPos.x + scaledX,
											touchpadPos.y + scaledY);
							drawList->AddCircleFilled(touchPos, 6.0f * io.FontGlobalScale,
													  IM_COL32(255, 0, 0, 255));
						}

						if (dualsense.State.trackPadTouch1.IsActive)
						{
							float scaledX =
								(dualsense.State.trackPadTouch1.X / 1900.0f) *
								touchpadSize.x;
							float scaledY =
								(dualsense.State.trackPadTouch1.Y / 1100.0f) *
								touchpadSize.y;
							ImVec2 touchPos(touchpadPos.x + scaledX,
											touchpadPos.y + scaledY);
							drawList->AddCircleFilled(
								touchPos, 6.0f * io.FontGlobalScale,
								IM_COL32(255, 0, 0,
										 255)); // Red circle for active touch
						}
						ImGui::SameLine();
						ImGui::Text(
							"%s:\n%s: %d\n\n%s 1:\nX: %d\nY: %d\n\n%s "
							"2:\nX: %d\nY: %d",
							strings.GeneralData.c_str(), strings.TouchPacketNum.c_str(),
							dualsense.State.TouchPacketNum, strings.Touch.c_str(),
							dualsense.State.trackPadTouch0.X,
							dualsense.State.trackPadTouch0.Y, strings.Touch.c_str(),
							dualsense.State.trackPadTouch1.X,
							dualsense.State.trackPadTouch1.Y);

						ImGui::Checkbox(strings.TouchpadToMouse.c_str(),
										&s.TouchpadToMouse);
						Tooltip(strings.Tooltip_TouchpadToMouse.c_str());
						ImGui::SetNextItemWidth(300);
						ImGui::SliderFloat(strings.Sensitivity.c_str(),
										   &s.swipeThreshold, 0.01f, 3.0f);
					}

					if (ImGui::CollapsingHeader(strings.MicButton.c_str()))
					{
						ImGui::Checkbox(strings.TakeScreenshot.c_str(),
										&s.MicScreenshot);
						Tooltip(strings.Tooltip_TakeScreenshot.c_str());
						if (HF_status ==
							DualsenseUtils::HapticFeedbackStatus::Working)
						{
							ImGui::SameLine();
							ImGui::SetNextItemWidth(100);
							ImGui::SliderFloat(strings.ScreenshotSoundVolume.c_str(),
											   &s.ScreenshotSoundLoudness, 0, 1);
						}

						ImGui::Checkbox(strings.RealMicFunctionality.c_str(),
										&s.MicFunc);
						Tooltip(strings.Tooltip_RealMicFunctionality.c_str());

						if (!s.MicFunc)
						{
							dualsense.SetMicrophoneLED(false, false);
							dualsense.SetMicrophoneVolume(80);
						}

						ImGui::Checkbox(strings.TouchpadShortcut.c_str(),
										&s.TouchpadToMouseShortcut);
						ImGui::Checkbox(strings.SwapTriggersShortcut.c_str(),
										&s.SwapTriggersShortcut);
						ImGui::Checkbox(strings.X360Shortcut.c_str(), &s.X360Shortcut);
						ImGui::Checkbox(strings.DS4Shortcut.c_str(), &s.DS4Shortcut);
						ImGui::Checkbox(strings.StopEmuShortcut.c_str(),
										&s.StopEmuShortcut);
						ImGui::Checkbox(strings.StopWritingShortcut.c_str(),
										&s.StopWritingShortcut);
						ImGui::Checkbox(strings.GyroToMouseShortcut.c_str(),
										&s.GyroToMouseShortcut);
						ImGui::Checkbox(strings.GyroToRightAnalogStickShortcut.c_str(),
										&s.GyroToRightAnalogStickShortcut);
						ImGui::Checkbox(strings.DisconnectControllerShortcut.c_str(),
										&s.DisconnectControllerShortcut);
					}

					if (ImGui::CollapsingHeader(strings.EmulationHeader.c_str()))
					{
						if (s.emuStatus == None)
						{
							if (ImGui::Button(strings.X360emu.c_str()))
							{
								s.emuStatus = X360;
								MyUtils::RunAsyncHidHideRequest(dualsense.GetPath(),
																"hide");
							}
							ImGui::SameLine();
							if (ImGui::Button(strings.DS4emu.c_str()))
							{
								s.emuStatus = DS4;
								dualsense.SetLightbar(0, 0, 64);
								MyUtils::RunAsyncHidHideRequest(dualsense.GetPath(),
																"hide");
							}
							ImGui::SameLine();
							ImGui::Checkbox(strings.DualShock4V2emu.c_str(),
											&s.DualShock4V2);
							Tooltip(strings.Tooltip_Dualshock4V2.c_str());
						}
						else
						{
							if (ImGui::Button(strings.STOPemu.c_str()))
							{
								MyUtils::RunAsyncHidHideRequest(dualsense.GetPath(),
																"show");
								s.emuStatus = None;
							}
						}

						if (!WasElevated)
						{
							ImGui::Text(strings.ControllerEmuUserMode.c_str());
						}
						else
						{
							ImGui::Text(strings.ControllerEmuAppAsAdmin.c_str());
						}

						ImGui::Separator();

						ImGui::SetNextItemWidth(300);
						ImGui::SliderInt(strings.L2Deadzone.c_str(), &s.L2Deadzone, 1,
										 255);
						ImGui::SetNextItemWidth(300);
						ImGui::SliderInt(strings.R2Deadzone.c_str(), &s.R2Deadzone, 1,
										 255);
						ImGui::Checkbox(strings.TriggersAsButtons.c_str(),
										&s.TriggersAsButtons);
						Tooltip(strings.Tooltip_TriggersAsButtons.c_str());
						ImGui::Checkbox(strings.TouchpadAsSelect.c_str(),
										&s.TouchpadAsSelect);
						ImGui::Checkbox(strings.TouchpadAsStart.c_str(),
										&s.TouchpadAsStart);
						ImGui::Checkbox(strings.OverrideDS4Lightbar.c_str(),
										&s.OverrideDS4Lightbar);
					}

					ShowNonControllerConfig = false;
				}
				else
				{
					killController(data, CurrentController);
					CurrentController = "";
				}
			}
			else
			{
				ShowNonControllerConfig = true;
			}

			if (ShowNonControllerConfig)
			{
				ImGui::BeginMainMenuBar();
				if (ImGui::BeginMenu("Config"))
				{
					ImGui::SetNextItemWidth(120.0f);
					if (ImGui::Combo(strings.Language.c_str(), &selectedLanguageIndex,
									 languageItems.data(), languageItems.size()))
					{
						const std::string &selectedLanguage =
							languages[selectedLanguageIndex];
						appConfig.Language = selectedLanguage;
						Config::WriteAppConfigToFile(appConfig);
					}

					if (ImGui::Checkbox(strings.RunAsAdmin.c_str(),
										&appConfig.ElevateOnStartup))
					{
						Config::WriteAppConfigToFile(appConfig);

						if (!MyUtils::IsRunAsAdministrator())
						{
							MyUtils::ElevateNow();
						}
					}
					Tooltip(strings.Tooltip_RunAsAdmin.c_str());

					if (ImGui::Checkbox(strings.HideToTray.c_str(),
										&appConfig.HideToTray))
					{
						Config::WriteAppConfigToFile(appConfig);
					}
					Tooltip(strings.Tooltip_HideToTray.c_str());

					if (ImGui::Checkbox(strings.HideWindowOnStartup.c_str(),
										&appConfig.HideWindowOnStartup))
					{
						Config::WriteAppConfigToFile(appConfig);
					}
					Tooltip(strings.Tooltip_HideWindowOnStartup.c_str());

					if (ImGui::Checkbox(strings.RunWithWindows.c_str(),
										&appConfig.RunWithWindows))
					{
						if (appConfig.RunWithWindows)
						{
							MyUtils::AddToStartup();
						}
						else
						{
							MyUtils::RemoveFromStartup();
						}

						Config::WriteAppConfigToFile(appConfig);
					}

					Tooltip(strings.RunWithWindows.c_str());
					if (ImGui::Checkbox(strings.ShowNotifications.c_str(),
										&appConfig.ShowNotifications))
					{
						if (appConfig.ShowNotifications)
						{
							MyUtils::AddNotifications();
						}
						else
						{
							MyUtils::RemoveNotifications();
						}

						Config::WriteAppConfigToFile(appConfig);
					}

					Tooltip(strings.Tooltip_ShowNotifications.c_str());
					if (ImGui::Checkbox(strings.ShowConsole.c_str(),
										&appConfig.ShowConsole))
					{
						Config::WriteAppConfigToFile(appConfig);
					}

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu(strings.Style.c_str()))
				{
					if (ImGui::Button(strings.SaveStyleToFile.c_str()))
					{
						MyUtils::SaveImGuiColorsToFile(style);
					}

					if (ImGui::Button(strings.LoadStyleFromFile.c_str()))
					{
						MyUtils::LoadImGuiColorsFromFile(style);
					}

					if (ImGui::Button(strings.SetDefaultStyleFile.c_str()))
					{
						std::string stylePath = Config::GetStylePath();
						if (stylePath != "")
						{
							appConfig.DefaultStyleFilepath = stylePath;
							Config::WriteAppConfigToFile(appConfig);
						}
					}

					if (ImGui::Button(strings.RemoveDefaultStyle.c_str()))
					{
						appConfig.DefaultStyleFilepath = "";
						Config::WriteAppConfigToFile(appConfig);
					}

					if (ImGui::Button(strings.ResetStyle.c_str()))
					{
						style = ImGuiStyle();
					}

					if (ImGui::Button("Show edit menu"))
					{
						ShowStyleEditWindow = true;
					}
					ImGui::EndMenu();
				}
				MainMenuBarHeight = ImGui::GetFrameHeight();
				ImGui::EndMainMenuBar();
			}
			ImGui::End();
		}
		else
		{
			ImGui::End();
		}

		if (IsMinimized)
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(6));

		ImGui::Render();
		end_cycle(window);
	}

	stop_thread = true; // Signal all threads to stop

	std::this_thread::sleep_for(std::chrono::milliseconds(15));

	for (auto &thread : readThreads)
	{
		if (thread.joinable())
		{
			thread.join();
		}
	}

	for (auto &thread : writeThreads)
	{
		if (thread.joinable())
		{
			thread.join();
		}
	}

	for (auto &thread : emuThreads)
	{
		if (thread.joinable())
		{
			thread.join();
		}
	}

	for (Dualsense &ds : DualSense)
	{
		if (WasElevated)
			MyUtils::StartHidHideRequest(ds.GetPath(), "show");

		if (appConfig.DisconnectAllBTControllersOnExit)
		{
			if (ds.GetConnectionType() == Feature::ConnectionType::BT)
			{
				MyUtils::DisableBluetoothDevice(ds.GetMACAddress(false));
			}
		}
	}

	udpServer.Dispose();
	if (trayThread.joinable())
		trayThread.join();
	for (const auto &pair : iconMap)
	{
		DestroyIcon(pair.second);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
	hid_exit();

	MyUtils::DeinitializeAudioEndpoint();
	//_CrtDumpMemoryLeaks();

	return 0;
}

void glfw_error_callback(int error, const char *description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
