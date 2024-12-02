const int VERSION = 18;

//#define _CRTDBG_MAP_ALLOC
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>

#include "MyUtils.h"
#include "DualSense.h"
#include "ControllerEmulation.h"
#include "IMMNotificationClient.h"
#include "cycle.hpp"
#include "miniaudio.h"
#include "Config.h"
#include "UDP.h"
#include "Settings.h"
#include "Strings.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
//#include <cstdlib>
//#include <crtdbg.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) &&                                 \
    !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")

#endif

static void glfw_error_callback(int error, const char *description);

deque<Dualsense> DualSense;
std::atomic<bool> stop_thread{false}; // Flag to signal the thread to stop

NotificationClient *client;
IMMDeviceEnumerator *deviceEnumerator = nullptr;
IMMDevice *device = nullptr;
IAudioMeterInformation *meterInfo = nullptr;

void StartHidHideRequest(std::string ID, std::string arg) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; // Use this flag to control window visibility
    si.wShowWindow = SW_HIDE;          // Set to SW_HIDE to prevent the window from showing

    ZeroMemory(&pi, sizeof(pi));

    std::string arg1 = "\"" + ID + "\"";
    std::string arg2 = " \"" + arg + "\" ";
    std::string command = "utilities\\hidhide_service_request.exe " + arg1 + arg2;

    if (CreateProcess(NULL,              // No module name (use command line)
                      (LPSTR)command.c_str(), // Command line
                      NULL,               // Process handle not inheritable
                      NULL,               // Thread handle not inheritable
                      FALSE,              // Set handle inheritance to FALSE
                      0,                  // No creation flags
                      NULL,               // Use parent's environment block
                      NULL,               // Use parent's starting directory 
                      &si,                // Pointer to STARTUPINFO structure
                      &pi)                // Pointer to PROCESS_INFORMATION structure
       ) {
        // Close process and thread handles. 
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

void RunAsyncHidHideRequest(std::string ID, std::string arg) {
    std::thread asyncThread(StartHidHideRequest, ID, arg);
    asyncThread.detach();  // Detach the thread to run independently
}

void InitializeAudioEndpoint()
{
    CoInitialize(nullptr);

    CoCreateInstance(__uuidof(MMDeviceEnumerator),
                     nullptr,
                     CLSCTX_ALL,
                     __uuidof(IMMDeviceEnumerator),
                     (void **)&deviceEnumerator);
    deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);

    client = new NotificationClient();
    deviceEnumerator->RegisterEndpointNotificationCallback(client);

    device->Activate(__uuidof(IAudioMeterInformation),
                     CLSCTX_ALL,
                     nullptr,
                     (void **)&meterInfo);

    CoUninitialize();
}

void DeinitializeAudioEndpoint()
{
    if (meterInfo)
    {
        meterInfo->Release();
        meterInfo = nullptr;
    }

    if (device)
    {
        device->Release();
        device = nullptr;
    }

    if (client)
    {
        deviceEnumerator->UnregisterEndpointNotificationCallback(client);
        delete client;
        client = nullptr;
    }

    if (deviceEnumerator)
    {
        deviceEnumerator->Release();
        deviceEnumerator = nullptr;
    }

    CoUninitialize();
}

string WStringToString(const wstring& wstr)
{
	string str;
	size_t size;
	str.resize(wstr.length());
	wcstombs_s(&size, &str[0], str.size() + 1, wstr.c_str(), wstr.size());
	return str;
}

void MoveCursor(int x, int y)
{
    POINT p;
    if (GetCursorPos(&p)) {
        SetCursorPos(p.x + x, p.y + y);
    }
}

float GetCurrentAudioPeak()
{
    if (client->DeviceChanged)
    {
        if (meterInfo)
            meterInfo->Release();
        if (device)
            device->Release();
        if (deviceEnumerator)
            deviceEnumerator->Release();
        client->DeviceChanged = false;
        InitializeAudioEndpoint();
    }

    float peakValue = 0.0f;
    if (meterInfo->GetPeakValue(&peakValue) == S_OK)
    {
        return peakValue;
    }
    else
    {
        return 0;
    }
}

void readControllerState(Dualsense &controller)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    while (!stop_thread)
    {
        controller.Read(); // Perform the read operation

        if(controller.Connected == false)
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}


void Tooltip(const char* text) {
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

void writeEmuController(Dualsense &controller, Settings &settings)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    ViGEm v;
    v.InitializeVigembus();
    ButtonState editedButtonState;
    constexpr auto POLL_INTERVAL = std::chrono::microseconds(500); // 0.5ms polling

    while (!stop_thread)
    {
        if (controller.Connected)
        {
            auto start = std::chrono::high_resolution_clock::now();

            editedButtonState = controller.State;

            int deltaLX = editedButtonState.LX - 128;
            int deltaLY = editedButtonState.LY - 128;

            float magnitudeL = sqrt(deltaLX * deltaLX + deltaLY * deltaLY);

            if (magnitudeL <= settings.LeftAnalogDeadzone) {
                editedButtonState.LX = 128;
                editedButtonState.LY = 128;
            } else {
                float scale = (magnitudeL - settings.LeftAnalogDeadzone) / (127.0f - settings.LeftAnalogDeadzone);

                if (scale > 1.0f) scale = 1.0f;

                editedButtonState.LX = 128 + static_cast<int>(deltaLX * scale);
                editedButtonState.LY = 128 + static_cast<int>(deltaLY * scale);
            }

            int deltaRX = editedButtonState.RX - 128;
            int deltaRY = editedButtonState.RY - 128;

            float magnitudeR = sqrt(deltaRX * deltaRX + deltaRY * deltaRY);

            if (magnitudeR <= settings.RightAnalogDeadzone) {
                editedButtonState.RX = 128;
                editedButtonState.RY = 128;
            } else {
                float scale = (magnitudeR - settings.RightAnalogDeadzone) / (127.0f - settings.RightAnalogDeadzone);

                if (scale > 1.0f) scale = 1.0f;

                editedButtonState.RX = 128 + static_cast<int>(deltaRX * scale);
                editedButtonState.RY = 128 + static_cast<int>(deltaRY * scale);
            }

            if (settings.TriggersAsButtons) {
                if (editedButtonState.L2 >= 1) {
                    editedButtonState.L2 = 255;
                    editedButtonState.L2Btn = true;
                }

                if (editedButtonState.R2 >= 1) {
                    editedButtonState.R2 = 255;
                    editedButtonState.R2Btn = true;
                }
            }

            if (settings.TouchpadAsSelectStart) {
                if (controller.State.touchBtn && controller.State.trackPadTouch0.X <= 1000) {
                    editedButtonState.share = true;
                }

                if (controller.State.touchBtn && controller.State.trackPadTouch0.X >= 1001) {
                    editedButtonState.options = true;
                }
            }

            if (settings.emuStatus != None && !v.isWorking())
            {
                MessageBox(0, "ViGEm connection failed! Are you sure you installed ViGEmBus driver?" ,"Error", 0);
                settings.emuStatus = None;
                continue;
            }

            if (settings.emuStatus == X360)
            {
                v.StartX360();
                v.UpdateX360(editedButtonState);
                settings.lrumble = v.rotors.lrotor;
                settings.rrumble = v.rotors.rrotor;
            }
            else if (settings.emuStatus == DS4)
            {
                if (settings.DualShock4V2)
                    v.SetDS4V2();
                else
                    v.SetDS4V1();

                v.StartDS4();
                v.UpdateDS4(editedButtonState);
                settings.lrumble = v.rotors.lrotor;
                settings.rrumble = v.rotors.rrotor;
                if(!settings.CurrentlyUsingUDP){
                    if (v.Red > 0 || v.Green > 0 || v.Blue > 0) {
                        controller.SetLightbar(v.Red, v.Green, v.Blue);                      
                    }
                }            
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = end - start;
            if (elapsed < POLL_INTERVAL) {
                std::this_thread::sleep_for(POLL_INTERVAL - elapsed);
            }
        }

        if(settings.emuStatus == None || !controller.Connected)
        {
            v.Remove360();
            v.RemoveDS4();
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
    input.mi.dwFlags = flag;  // Left button down
    input.mi.time = 0;
    input.mi.dwExtraInfo = 0;
    SendInput(1, &input, sizeof(INPUT));
}

void writeControllerState(Dualsense &controller, Settings &settings)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    cout << "Write thread started: " << controller.GetPath() << " | "
         << settings.ControllerInput.ID << endl;

    // DISCO MODE VARIABLES
    int colorState = 0; // 0 = red to yellow, 1 = yellow to green, 2 = green to cyan, etc.
    int colorStateBattery = 0;
    int colorStateX360EMU = 0;
    uint8_t led[3];
    int step = 5; 
    led[0] = 255;
    led[1] = 0;
    led[2] = 0;
    int x360emuanimation = 0;
    bool lastMic = false;
    bool lastMic2 = false;
    bool lastMic3 = false;
    bool mic = false;
    float touchpadLastX = 0;
    float touchpadLastY = 0;
    float touchpad1LastY = 0;
    bool wasTouching = false;
    bool wasTouching1 = false;
    bool wasTouchpadClicked = false;
    bool X360animationplayed = false;
    bool X360readyfornew = false;
    int steps[] = {1, -1, 1, -1,1, -1, 1, -1,1, -1, 1, -1};
    int limits[] = {255, 0, 255, 0,255, 0, 255, 0,255, 0, 255, 0};
    int direction = 1; // 1 for increasing, -1 for decreasing
    DualsenseUtils::InputFeatures lastFeatures;

    while (!stop_thread)
    {
        if (true)
        {
            if (settings.emuStatus == X360 && !X360animationplayed) {
                if (colorStateX360EMU < 20) {
                    x360emuanimation += direction * step;

                    // Reverse direction when reaching the upper or lower limits
                    if (x360emuanimation >= 255) {
                        x360emuanimation = 255; // Clamp to the max value
                        direction = -1;          // Change to decreasing
                        colorStateX360EMU++;
                    } else if (x360emuanimation <= 0) {
                        x360emuanimation = 0;    // Clamp to the min value
                        direction = 1;           // Change to increasing
                        colorStateX360EMU++;
                    }

                    controller.SetLightbar(0, x360emuanimation, 0);
                } else {
                    X360animationplayed = true;
                }          
            }
            else if (settings.emuStatus == X360 && X360readyfornew) {
                X360animationplayed = false;
            }

            if(settings.emuStatus == None && X360animationplayed) {
                X360readyfornew = true;             
                x360emuanimation = 0;
                colorStateX360EMU = 0;
            }
            else if (settings.emuStatus == None && !X360animationplayed) {
                X360animationplayed = true;
            }

            if (controller.State.micBtn && controller.State.DpadUp && settings.SwapTriggersShortcut && !lastMic2) {
                settings.SwapTriggersRumbleToAT = settings.SwapTriggersRumbleToAT ? settings.SwapTriggersRumbleToAT = false : settings.SwapTriggersRumbleToAT = true;
                lastMic2 = true;
            }
            else if(!controller.State.micBtn && controller.State.DpadUp && settings.SwapTriggersShortcut && lastMic2){
                lastMic2 = false;
            }

            if (settings.emuStatus != X360 && settings.X360Shortcut && controller.State.micBtn && controller.State.DpadLeft) {
                X360animationplayed = false;
                X360readyfornew = true;
                RunAsyncHidHideRequest(controller.GetPath(), "hide");
                settings.emuStatus = X360;
            }

            if (settings.emuStatus != DS4 && settings.DS4Shortcut && controller.State.micBtn && controller.State.DpadRight) {
                RunAsyncHidHideRequest(controller.GetPath(), "hide");
                settings.emuStatus = DS4;
                controller.SetLightbar(0, 0, 64);
            }

            if (settings.emuStatus != None && settings.StopEmuShortcut && controller.State.micBtn && controller.State.DpadDown) {
                RunAsyncHidHideRequest(controller.GetPath(), "show");
                settings.emuStatus = None;
            }

            if (settings.TouchpadToMouseShortcut && controller.State.micBtn && controller.State.touchBtn && !lastMic3) {
                settings.TouchpadToMouse = settings.TouchpadToMouse ? settings.TouchpadToMouse = false : settings.TouchpadToMouse = true;
                lastMic3 = true;
            }
            else if (settings.TouchpadToMouseShortcut && !controller.State.micBtn && controller.State.touchBtn && lastMic3) {
                lastMic3 = false;
            }

            if (settings.TouchpadToMouse && controller.State.trackPadTouch0.IsActive) {
                if (controller.State.touchBtn && !wasTouchpadClicked && controller.State.trackPadTouch0.X <= 1000) {
                    MouseClick(MOUSEEVENTF_LEFTDOWN);
                    wasTouchpadClicked = true;
                }
                else if (!controller.State.touchBtn && wasTouchpadClicked && controller.State.trackPadTouch0.X <= 1000) {
                    MouseClick(MOUSEEVENTF_LEFTUP);
                    wasTouchpadClicked = false;
                }

                if (controller.State.touchBtn && !wasTouchpadClicked && controller.State.trackPadTouch0.X >= 1001) {
                    MouseClick(MOUSEEVENTF_RIGHTDOWN);
                    wasTouchpadClicked = true;
                }
                else if (!controller.State.touchBtn && wasTouchpadClicked && controller.State.trackPadTouch0.X >= 1001) {
                    MouseClick(MOUSEEVENTF_RIGHTUP);
                    wasTouchpadClicked = false;
                }

                // If it wasn't previously touching, start tracking movement
                if (!wasTouching) {
                    touchpadLastX = controller.State.trackPadTouch0.X;
                    touchpadLastY = controller.State.trackPadTouch0.Y;
                }

                if (!wasTouching1) {
                    touchpad1LastY = controller.State.trackPadTouch1.Y;
                }

                float normalizedX = controller.State.trackPadTouch0.X;
                float normalizedY = controller.State.trackPadTouch0.Y;

                float deltaX = normalizedX - touchpadLastX;
                float deltaY = normalizedY - touchpadLastY;
                float delta1Y = touchpad1LastY - controller.State.trackPadTouch1.Y;

                if (controller.State.trackPadTouch1.IsActive) {
                    wasTouching1 = true;
                    if (fabs(delta1Y) > 5.0f) {
                        MouseClick(MOUSEEVENTF_WHEEL, static_cast<int>(delta1Y * 2));
                    }

                    touchpad1LastY = controller.State.trackPadTouch1.Y;
                }
                else {
                    wasTouching1 = false;
                }

                if (fabs(deltaX) > settings.swipeThreshold || fabs(deltaY) > settings.swipeThreshold) {

                    MoveCursor(deltaX * settings.swipeThreshold, deltaY * settings.swipeThreshold);
                }

                // Update the last touchpad position
                touchpadLastX = normalizedX;
                touchpadLastY = normalizedY;
                
                // Mark that the touchpad is active
                wasTouching = true;
            } else {
                // Reset the touch state when the touchpad is no longer active
                wasTouching = false;
            }
            
            if(!settings.CurrentlyUsingUDP && settings.emuStatus != DS4 && X360animationplayed)
            {
                controller.SetLightbar(settings.ControllerInput.Red,settings.ControllerInput.Green,settings.ControllerInput.Blue);
            }

            if (!settings.RumbleToAT && !settings.HapticsToTriggers && !settings.CurrentlyUsingUDP) {
               controller.SetRightTrigger(
                    settings.ControllerInput.RightTriggerMode,
                    settings.ControllerInput.RightTriggerForces[0],
                    settings.ControllerInput.RightTriggerForces[1],
                    settings.ControllerInput.RightTriggerForces[2],
                    settings.ControllerInput.RightTriggerForces[3],
                    settings.ControllerInput.RightTriggerForces[4],
                    settings.ControllerInput.RightTriggerForces[5],
                    settings.ControllerInput.RightTriggerForces[6],
                    settings.ControllerInput.RightTriggerForces[7],
                    settings.ControllerInput.RightTriggerForces[8],
                    settings.ControllerInput.RightTriggerForces[9],
                    settings.ControllerInput.RightTriggerForces[10]);

                controller.SetLeftTrigger(settings.ControllerInput.LeftTriggerMode,
                    settings.ControllerInput.LeftTriggerForces[0],
                    settings.ControllerInput.LeftTriggerForces[1],
                    settings.ControllerInput.LeftTriggerForces[2],
                    settings.ControllerInput.LeftTriggerForces[3],
                    settings.ControllerInput.LeftTriggerForces[4],
                    settings.ControllerInput.LeftTriggerForces[5],
                    settings.ControllerInput.LeftTriggerForces[6],
                    settings.ControllerInput.LeftTriggerForces[7],
                    settings.ControllerInput.LeftTriggerForces[8],
                    settings.ControllerInput.LeftTriggerForces[9],
                    settings.ControllerInput.LeftTriggerForces[10]);
            }

            if (settings.RumbleToAT && !settings.CurrentlyUsingUDP) {
                int leftForces[3] = { 0 };
                int rightForces[3] = { 0 };

                if (settings.RumbleToAT_RigidMode) {
                    // stiffness
                    leftForces[1] = settings.SwapTriggersRumbleToAT ? settings.rrumble : settings.lrumble;
                    rightForces[1] = settings.SwapTriggersRumbleToAT ? settings.lrumble : settings.rrumble;

                    // set threshold
                    leftForces[0] = settings.SwapTriggersRumbleToAT ? 255 - settings.rrumble: 255 - settings.lrumble; 
                    rightForces[0] = settings.SwapTriggersRumbleToAT ? 255 - settings.lrumble : 255 - settings.rrumble;

                    controller.SetLeftTrigger(Trigger::Rigid, leftForces[0], leftForces[1], 0,0,0,0,0,0,0,0,0);
                    controller.SetRightTrigger(Trigger::Rigid,rightForces[0], rightForces[1], 0,0,0,0,0,0,0,0,0);
                }
                else {
                    // set frequency
                    if (!settings.SwapTriggersRumbleToAT && settings.lrumble < 25 || settings.SwapTriggersRumbleToAT && settings.rrumble < 25)
                        leftForces[0] = settings.SwapTriggersRumbleToAT ? min(settings.MaxRightRumbleToATFrequency, settings.rrumble) : min(settings.MaxLeftRumbleToATFrequency, settings.lrumble);
                    else
                        leftForces[0] = min(settings.MaxLeftRumbleToATFrequency, 25);

                    if(!settings.SwapTriggersRumbleToAT && settings.rrumble < 25 || settings.SwapTriggersRumbleToAT && settings.lrumble < 25)
                        rightForces[0] = settings.SwapTriggersRumbleToAT ? min(settings.MaxLeftRumbleToATFrequency, settings.lrumble) : min(settings.MaxRightRumbleToATFrequency, settings.rrumble);
                    else
                        rightForces[0] = min(settings.MaxRightRumbleToATFrequency, 25);

                    // set power
                    leftForces[1] = settings.SwapTriggersRumbleToAT ? min(settings.MaxRightRumbleToATIntensity, settings.rrumble) : min(settings.MaxLeftRumbleToATIntensity,settings.lrumble); 
                    rightForces[1] = settings.SwapTriggersRumbleToAT ? min(settings.MaxLeftRumbleToATIntensity,settings.lrumble) : min(settings.MaxRightRumbleToATIntensity,settings.rrumble);

                    // set static threshold
                    leftForces[2] = 20;
                    rightForces[2] = 20;

                    controller.SetLeftTrigger(Trigger::Pulse_B, leftForces[0], leftForces[1], leftForces[2],0,0,0,0,0,0,0,0);
                    controller.SetRightTrigger(Trigger::Pulse_B,rightForces[0], rightForces[1], rightForces[2],0,0,0,0,0,0,0,0);
                }            
            }

            if (settings.HapticsToTriggers && !settings.CurrentlyUsingUDP) {
                if (settings.RumbleToAT && settings.lrumble > 0 || settings.RumbleToAT && settings.rrumble > 0) {
                    goto skiphapticstotriggers;
                }

                Peaks peaks = controller.GetAudioPeaks();
                int ScaledLeftActuator = MyUtils::scaleFloatToInt(peaks.LeftActuator);
                int ScaledRightActuator = MyUtils::scaleFloatToInt(peaks.RightActuator);
                int leftForces[3] = { 0 };
                int rightForces[3] = { 0 };

                if (settings.RumbleToAT_RigidMode) {
                    // stiffness
                    leftForces[1] = settings.SwapTriggersRumbleToAT ? ScaledRightActuator : ScaledLeftActuator;
                    rightForces[1] = settings.SwapTriggersRumbleToAT ? ScaledLeftActuator : ScaledRightActuator;

                    // set threshold
                    leftForces[0] = settings.SwapTriggersRumbleToAT ? 255 - ScaledRightActuator: 255 - ScaledLeftActuator; 
                    rightForces[0] = settings.SwapTriggersRumbleToAT ? 255 - ScaledLeftActuator : 255 - ScaledRightActuator;

                    controller.SetLeftTrigger(Trigger::Rigid, leftForces[0], leftForces[1], 0,0,0,0,0,0,0,0,0);
                    controller.SetRightTrigger(Trigger::Rigid,rightForces[0], rightForces[1], 0,0,0,0,0,0,0,0,0);
                }
                else {
                    //cout << ScaledLeftActuator << " | " << ScaledRightActuator << endl;

                    // set frequency
                    if (!settings.SwapTriggersRumbleToAT && ScaledLeftActuator < 25 || settings.SwapTriggersRumbleToAT && ScaledRightActuator < 25)
                        leftForces[0] = settings.SwapTriggersRumbleToAT ? min(settings.MaxRightRumbleToATFrequency, ScaledRightActuator) : min(settings.MaxLeftRumbleToATFrequency, ScaledLeftActuator);
                    else
                        leftForces[0] = min(settings.MaxLeftRumbleToATFrequency, 25);

                    if(!settings.SwapTriggersRumbleToAT && ScaledRightActuator < 25 || settings.SwapTriggersRumbleToAT && ScaledLeftActuator < 25)
                        rightForces[0] = settings.SwapTriggersRumbleToAT ? min(settings.MaxLeftRumbleToATFrequency, ScaledLeftActuator) : min(settings.MaxRightRumbleToATFrequency, ScaledRightActuator);
                    else
                        rightForces[0] = min(settings.MaxRightRumbleToATFrequency, 25);

                    // set power
                    leftForces[1] = settings.SwapTriggersRumbleToAT ? min(settings.MaxRightRumbleToATFrequency, ScaledRightActuator) : min(settings.MaxLeftRumbleToATFrequency,ScaledLeftActuator); 
                    rightForces[1] = settings.SwapTriggersRumbleToAT ? min(settings.MaxLeftRumbleToATFrequency,ScaledLeftActuator) : min(settings.MaxRightRumbleToATFrequency,ScaledRightActuator);

                    // set static threshold
                    leftForces[2] = 20;
                    rightForces[2] = 20;

                    controller.SetLeftTrigger(Trigger::Pulse_B, leftForces[0], leftForces[1], leftForces[2],0,0,0,0,0,0,0,0);
                    controller.SetRightTrigger(Trigger::Pulse_B,rightForces[0], rightForces[1], rightForces[2],0,0,0,0,0,0,0,0);
                }  
            }
            skiphapticstotriggers:

            if (settings.TriggersAsButtons && !settings.CurrentlyUsingUDP && settings.emuStatus != None) {
                controller.SetLeftTrigger(Trigger::Rigid,40,255,255,255,255,255,255,255,255,255,255);
                controller.SetRightTrigger(Trigger::Rigid,40,255,255,255,255,255,255,255,255,255,255);
            }

            if (settings.AudioToLED && !settings.CurrentlyUsingUDP && settings.emuStatus != DS4 && X360animationplayed)
            {
                int peak = static_cast<int>(GetCurrentAudioPeak() * 500);
                controller.SetLightbar(peak,peak,peak);
            }

            if (settings.DiscoMode && !settings.CurrentlyUsingUDP && settings.emuStatus != DS4 && X360animationplayed)
            {               
                controller.SetLightbar(led[0], led[1], led[2]);

                switch (colorState)
                    {
                        case 0:
                            led[1] += step;
                            if (led[1] >= 255) colorState = 1;
                            break;

                        case 1:
                            led[0] -= step;
                            if (led[0] <= 0) colorState = 2;
                            break;

                        case 2:
                            led[2] += step;
                            if (led[2] >= 255) colorState = 3;
                            break;

                        case 3:
                            led[1] -= step;
                            if (led[1] <= 0) colorState = 4;
                            break;

                        case 4:
                            led[0] += step;
                            if (led[0] >= 255) colorState = 5;
                            break;

                        case 5:
                            led[2] -= step;
                            if (led[2] <= 0) colorState = 0;
                            break;
                    }
            }

            if (settings.BatteryLightbar && !settings.CurrentlyUsingUDP && settings.emuStatus != DS4 && X360animationplayed) {
                int batteryLevel = controller.State.battery.Level;

                if(batteryLevel > 20)
                {
                    batteryLevel = std::max<int>(0, std::min<int>(100, batteryLevel));

                    int red = (100 - batteryLevel) * 255 / 100;
                    int green = batteryLevel * 255 / 100;
                    int blue = 0;
                    controller.SetLightbar(red, green, blue);
                }
                else {
                    switch (colorStateBattery) {
                    case 0:
                        led[0] += step;
                        if (led[0] == 255) colorStateBattery = 1;
                        break;
                    case 1:
                        led[0] -= step;
                        if (led[0] == 0) colorStateBattery = 0;
                        break;
                    }

                    controller.SetLightbar(led[0], 0, 0);
                }

            }

            if (settings.MicScreenshot) {
                if (controller.State.micBtn && !lastMic && !controller.State.DpadDown && !controller.State.DpadLeft && !controller.State.DpadRight && !controller.State.DpadUp && !controller.State.touchBtn) {
                    controller.PlayHaptics("sounds/screenshot.wav");
                    MyUtils::TakeScreenShot();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    filesystem::create_directories(MyUtils::GetImagesFolderPath() + "\\DualSenseY\\");
                    string filename = MyUtils::GetImagesFolderPath() + "\\DualSenseY\\" + MyUtils::currentDateTimeWMS() + ".bmp";
                    MyUtils::SaveBitmapFromClipboard(filename.c_str());
                    lastMic = true;
                }
            }

            if (settings.MicFunc) {
                if (!controller.State.micBtn && lastMic) {
                    if (!mic) {
                        controller.SetMicrophoneLED(true, false);
                        controller.SetMicrophoneVolume(0);
                        mic = true;
                    }               
                    else{
                        controller.SetMicrophoneLED(false, false);
                        controller.SetMicrophoneVolume(80);

                        mic = false;
                    }
                    lastMic = false;
                }
                else if (controller.State.micBtn && !lastMic) {
                    lastMic = true;
                }
            }

            if (!controller.State.micBtn && lastMic) {
                lastMic = false;
            }
      
            if (settings.lrumble > 0 || settings.rrumble > 0)
            {
                controller.UseRumbleNotHaptics(true);
            }
            else {
                controller.UseRumbleNotHaptics(false);
            }

            controller.SetSpeakerVolume(settings.ControllerInput.SpeakerVolume);
            controller.UseHeadsetNotSpeaker(settings.UseHeadset);
            controller.SetRumble(settings.lrumble, settings.rrumble);

            if(controller.Connected)
            {
                if (controller.Write() == false) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
    }
}

BOOL IsRunAsAdministrator()
{
    BOOL fIsRunAsAdmin = FALSE;
    DWORD dwError = ERROR_SUCCESS;
    PSID pAdministratorsGroup = NULL;

    // Allocate and initialize a SID of the administrators group.
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(
        &NtAuthority, 
        2, 
        SECURITY_BUILTIN_DOMAIN_RID, 
        DOMAIN_ALIAS_RID_ADMINS, 
        0, 0, 0, 0, 0, 0, 
        &pAdministratorsGroup))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    // Determine whether the SID of administrators group is enabled in 
    // the primary access token of the process.
    if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

Cleanup:
    // Centralized cleanup for all allocated resources.
    if (pAdministratorsGroup)
    {
        FreeSid(pAdministratorsGroup);
        pAdministratorsGroup = NULL;
    }

    // Throw the error if something failed in the function.
    if (ERROR_SUCCESS != dwError)
    {
        throw dwError;
    }

    return fIsRunAsAdmin;
}

void ElevateNow()
{
    BOOL bAlreadyRunningAsAdministrator = FALSE;
    try
    {
        bAlreadyRunningAsAdministrator = IsRunAsAdministrator();
    }
    catch(...)
    {
        std::cout << "Failed to determine if application was running with admin rights" << std::endl;
        DWORD dwErrorCode = GetLastError();
        TCHAR szMessage[256];
        _stprintf_s(szMessage, ARRAYSIZE(szMessage), _T("Error code returned was 0x%08lx"), dwErrorCode);
        std::cout << szMessage << std::endl;
    }

    if (!bAlreadyRunningAsAdministrator)
    {
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath)))
        {
            // Launch itself as admin
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;

            if (!ShellExecuteExW(&sei))
            {
                DWORD dwError = GetLastError();
                if (dwError == ERROR_CANCELLED)
                {
                    std::cout << "End user did not allow elevation" << std::endl;
                }
            }
            else
            {
                _exit(1);  // Quit itself
            }
        }
    }
}

float CalculateScaleFactor(int screenWidth, int screenHeight) {
    // Choose a baseline resolution, e.g., 1920x1080
    const int baseWidth = 1920;
    const int baseHeight = 1080;

    // Calculate the scale factor based on the vertical or horizontal scale difference
    float widthScale = static_cast<float>(screenWidth) / baseWidth;
    float heightScale = static_cast<float>(screenHeight) / baseHeight;

    // Use an average or the larger scale factor, depending on preference
    return (widthScale + heightScale) / 1.2f;
}

void setTaskbarIcon(GLFWwindow* window) {
    int width, height, nrChannels;
    unsigned char* img = stbi_load("utilities/icon.png", &width, &height, &nrChannels, 4);
    
    if (img) {
        GLFWimage icon;
        icon.width = width;
        icon.height = height;
        icon.pixels = img;

        // Set the window icon
        glfwSetWindowIcon(window, 1, &icon);

        // Free the image memory after setting the icon
        stbi_image_free(img);
    } else {
        // Handle error loading image
        printf("Failed to load icon\n");
    }
}

static bool IsMinimized = false;
void window_iconify_callback(GLFWwindow* window, int iconified) {
    if (iconified) {
        IsMinimized = true;
    } else {
        IsMinimized = false;
    }
}

void mTray(Tray::Tray &tray, Config::AppConfig &AppConfig) {
    tray.addEntry(Tray::Button("Show window", [&] {
        AppConfig.ShowWindow = true;
    }));

    tray.run();
}

int main()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    timeBeginPeriod(1);
    //glaiel::crashlogs::set_crashlog_folder("crashlogs\\");
    //glaiel::crashlogs::begin_monitoring();
    //_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc(28737);

    Config::AppConfig appConfig = Config::ReadAppConfigFromFile();

    if(appConfig.ShowConsole == false)
        FreeConsole();
 
    bool UpdateAvailable = false;
    try {
        std::string response = cpr::Get(cpr::Url("https://raw.githubusercontent.com/WujekFoliarz/DualSenseY-v2/refs/heads/master/version")).text;
        int version = stoi(response);

        if (version > VERSION) {
            cout << "Update available!" << endl;
            UpdateAvailable = true;
        }
        else {
            cout << "Application is up-to-date!" << endl;
        }
    }
    catch (...) {
        cout << "Version check failed" << endl;
    }
     
    if (appConfig.ElevateOnStartup) {
        ElevateNow();
    }
    else {
        cout << "Not elevating" << endl;
    }

    Tray::Tray tray("test", "utilities\\icon.ico");
    std::thread trayThread(mTray, std::ref(tray), std::ref(appConfig));
    trayThread.detach();

    bool WasElevated = IsRunAsAdministrator();

    // Write default english to file
    Strings enStrings;
    nlohmann::json enJson = enStrings.to_json();
    filesystem::create_directories(MyUtils::GetExecutableFolderPath() + "\\localizations\\");
    std::ofstream EnglishFILE(MyUtils::GetExecutableFolderPath() + "\\localizations\\en.json");  
    EnglishFILE << enJson.dump(4);
    EnglishFILE.close();
    enJson.clear();
    
    Strings strings = ReadStringsFromFile(appConfig.Language); // Load language file

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    int InstancesCount = 0;
    if (Process32First(snapshot, &entry) == TRUE)
    {
        while (Process32Next(snapshot, &entry) == TRUE)
        {
            if (strcmp(entry.szExeFile, "DualSenseY.exe") == 0)
            {
                InstancesCount++;
            }
        }
    }

    if (InstancesCount > 1) {
        MessageBox(0, "DualSenseY is already running!", "Error", 0);
        return 1;
    }

    CloseHandle(snapshot);

    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
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
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    string title = "DualSenseY - Version " + std::to_string(VERSION);
    // Create window with graphics context
    auto *window = glfwCreateWindow(static_cast<std::int32_t>(1200),
                                    static_cast<std::int32_t>(900),
                                    title.c_str(),
                                    nullptr,
                                    nullptr);
    if (window == nullptr)
    {
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    

    auto &style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(1.0, 1.0, 1.0, 1.0);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(1.0, 1.0, 1.0, 1.0);
    deque<Settings> ControllerSettings;
    vector<thread> readThreads;  // Store the threads for each controller
    vector<thread> writeThreads; // Audio to LED threads
    vector<thread> emuThreads;
    std::string CurrentController = "";
    bool firstController = true;
    bool firstTimeUDP = true;
    DualsenseUtils::InputFeatures preUDP;
    vector<std::string> ControllerID = DualsenseUtils::EnumerateControllerIDs();

    int ControllerCount = DualsenseUtils::GetControllerCount();
    std::chrono::high_resolution_clock::time_point LastControllerCheck = std::chrono::high_resolution_clock::now();

    InitializeAudioEndpoint();

    UDP udpServer;
    udpServer.StartFakeDSXProcess();
    
    ImGuiIO &io = ImGui::GetIO();

     static const ImWchar polishGlyphRange[] = {
                0x0020, 0x00FF, // Basic Latin + Latin Supplement
                0x0100, 0x017F, // Latin Extended-A
                0x0180, 0x024F, // Latin Extended-B (if you need even more coverage)
                0 };
      if (appConfig.Language == "ja")    
        ImFont* font_title = io.Fonts->AddFontFromFileTTF(std::string(MyUtils::GetExecutableFolderPath() + "\\fonts\\NotoSansJP-Bold.ttf").c_str(), 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
      else {
        ImFont* font_title = io.Fonts->AddFontFromFileTTF(std::string(MyUtils::GetExecutableFolderPath() + "\\fonts\\Roboto-Bold.ttf").c_str(), 15.0f, NULL, polishGlyphRange);
      }

    std::vector<const char*> languageItems;
    for (const auto& lang : languages) {
        languageItems.push_back(lang.c_str());
    }
    int selectedLanguageIndex = 0;

    // set language index
    for (int i = 0; i < languages.size(); i++) {
        if (languages[i] == appConfig.Language) {
            selectedLanguageIndex = i;
            break;
        }
    }
    
    setTaskbarIcon(window);
    glfwSetWindowIconifyCallback(window, window_iconify_callback);
    bool WindowHiddenOnStartup = false;
  

    if (appConfig.HideWindowOnStartup) {
        IsMinimized = true;
        WindowHiddenOnStartup = true;
        appConfig.ShowWindow = false;
    }

    while (!glfwWindowShouldClose(window))
    {

        if (IsMinimized && appConfig.HideToTray || IsMinimized && !appConfig.HideToTray && appConfig.HideWindowOnStartup && WindowHiddenOnStartup) {
            glfwHideWindow(window);
            WindowHiddenOnStartup = false;       
        }

        if (appConfig.ShowWindow) {
            glfwFocusWindow(window);
            glfwRestoreWindow(window);
            appConfig.ShowWindow = false;
            IsMinimized = false;
        }

        start_cycle();
        ImGui::NewFrame();

        ImGuiStyle *style = &ImGui::GetStyle();

        // Get the viewport size (size of the application window)
        
        ImVec2 window_size = io.DisplaySize;

        // Set window flags to remove the ability to resize, move, or close the window
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing;

        // Begin the "Main" window and set its size and position
        ImGui::SetNextWindowSize(window_size);
        ImGui::SetNextWindowPos(
            ImVec2(0, 0)); // Position at the top-left corner

        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        int screenWidth = mode->width;
        int screenHeight = mode->height;

        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        io.FontGlobalScale = CalculateScaleFactor(screenHeight, screenHeight);  // Adjust the scale factor as needed
        
        if (ImGui::Begin("Main", nullptr, window_flags))
        {
            // Limit these functions for better CPU usage 
            std::chrono::high_resolution_clock::time_point Now = std::chrono::high_resolution_clock::now();
            if ((Now - LastControllerCheck) > 3s) {
                LastControllerCheck = std::chrono::high_resolution_clock::now();
                ControllerID.clear();
                ControllerID = DualsenseUtils::EnumerateControllerIDs();
                ControllerCount = DualsenseUtils::GetControllerCount();
            }
            
            if (ImGui::BeginCombo("##", CurrentController.c_str()))
            {
                for (int n = 0; n < ControllerID.size(); n++)
                {
                    if (ControllerID[n] != "") {
                        bool is_selected = (CurrentController == ControllerID[n]);
                        if (ImGui::Selectable(ControllerID[n].c_str(), is_selected))
                        {
                            CurrentController = ControllerID[n].c_str();
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
                std::string udpStatusString = std::string(strings.UDPStatus + ": " + strings.Inactive);
            ImGui::Text(udpStatusString.c_str());
            ImGui::SameLine();
            static float color[3] = {255, 0, 0};
            ImGui::ColorEdit3("##",
                              color,
                              ImGuiColorEditFlags_NoAlpha |
                                  ImGuiColorEditFlags_NoPicker |
                                  ImGuiColorEditFlags_NoOptions |
                                  ImGuiColorEditFlags_NoInputs |
                                  ImGuiColorEditFlags_NoTooltip |
                                  ImGuiColorEditFlags_NoLabel |
                                  ImGuiColorEditFlags_NoSidePreview |
                                  ImGuiColorEditFlags_NoDragDrop |
                                  ImGuiColorEditFlags_NoBorder);
            }
            else
            {
                ImGui::SameLine();
            ImGui::Text(std::string(strings.UDPStatus + ": " + strings.Active).c_str());
            ImGui::SameLine();
            static float color[3] = {0, 255, 0};
            ImGui::ColorEdit3("##",
                              color,
                              ImGuiColorEditFlags_NoAlpha |
                                  ImGuiColorEditFlags_NoPicker |
                                  ImGuiColorEditFlags_NoOptions |
                                  ImGuiColorEditFlags_NoInputs |
                                  ImGuiColorEditFlags_NoTooltip |
                                  ImGuiColorEditFlags_NoLabel |
                                  ImGuiColorEditFlags_NoSidePreview |
                                  ImGuiColorEditFlags_NoDragDrop |
                                  ImGuiColorEditFlags_NoBorder);
            }
                      
            int nloop = 0;
            for (std::string &id : ControllerID)
            {
                bool IsPresent = false;

                for (Dualsense &ds : DualSense)
                {
                    if (id == ds.GetPath())
                    {
                        IsPresent = true;
                        break;
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
                            DualSense.emplace_back(x);

                            // Read default config if present
                            std::string configPath = "";
                            MyUtils::GetConfigPathForController(configPath, id);
                            Settings s;
                            if (configPath != "") {
                                s = Config::ReadFromFile(configPath);
                                if (s.emuStatus != None) {
                                    RunAsyncHidHideRequest(id, "hide");
                                }
                            }

                            ControllerSettings.emplace_back(s);
                            ControllerSettings.back().ControllerInput.ID = id;

                            if (firstController)
                            {
                                ControllerSettings.back().UseUDP = true;
                                firstController = false;
                                CurrentController = id.c_str();
                            }

                            readThreads.emplace_back(readControllerState,
                                                     std::ref(DualSense.back()));

                            writeThreads.emplace_back(writeControllerState,
                                                      std::ref(DualSense.back()),
                                                      std::ref(ControllerSettings.back()));

                            emuThreads.emplace_back(writeEmuController,
                                                    std::ref(DualSense.back()),
                                                    std::ref(ControllerSettings.back()));
                        }

                }
            }

            int ActiveDualsenseCount = DualSense.size();

            for (int i = 0; i < ActiveDualsenseCount; i++)
            {
                DualSense[i].Reconnect();
                DualSense[i].InitializeAudioEngine();
                DualSense[i].SetPlayerLED(i + 1);

                if (udpServer.isActive)
                {
                    for (Settings &s : ControllerSettings)
                    {                     
                        if (s.UseUDP == true)
                        {
                            if (firstTimeUDP) {
                                preUDP = s.ControllerInput;
                                firstTimeUDP = false;
                            }
                            s.CurrentlyUsingUDP = true;
                            udpServer.Battery = DualSense[i].State.battery.Level;               
                            DualSense[i].SetLightbar(udpServer.thisSettings.ControllerInput.Red, udpServer.thisSettings.ControllerInput.Green, udpServer.thisSettings.ControllerInput.Blue);
                            DualSense[i].SetLeftTrigger(udpServer.thisSettings.ControllerInput.LeftTriggerMode,
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

                            DualSense[i].SetRightTrigger(udpServer.thisSettings.ControllerInput.RightTriggerMode,
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

                            if (udpServer.thisSettings.CurrentHapticFile != "") {
                                
                                DualSense[i].PlayHaptics(udpServer.thisSettings.CurrentHapticFile.c_str());
                                udpServer.thisSettings.CurrentHapticFile = "";
                            }
                            
                            std::chrono::high_resolution_clock::time_point Now = std::chrono::high_resolution_clock::now();
                            if ((Now - udpServer.LastTime) > 8s) {
                                udpServer.isActive = false;
                                s.CurrentlyUsingUDP = false;
                                firstTimeUDP = true;
                                s.ControllerInput = preUDP;
                            }
                        }                      
                    }
                }

                if (DualSense[i].GetPath() == CurrentController && DualSense[i].Connected)
                {                  
                    for (Settings &s : ControllerSettings)
                    {
                        if (s.ControllerInput.ID == CurrentController)
                        {
                            DualsenseUtils::HapticFeedbackStatus HF_status = DualSense[i].GetHapticFeedbackStatus();
                            const char* bt_or_usb = DualSense[i].GetConnectionType() == Feature::USB ? "USB" : "BT";
                            ImGui::Text("%s %d | %s: %s | %s: %d%%", strings.ControllerNumberText.c_str(),i+1, strings.USBorBTconnectionType.c_str(), bt_or_usb, strings.BatteryLevel.c_str(), DualSense[i].State.battery.Level);

                            if (UpdateAvailable) {
                                ImGui::SameLine();
                                if (ImGui::Button(strings.InstallLatestUpdate.c_str())) {
                                     if(filesystem::exists("Updater.exe")) {
                                        system("start Updater.exe");
                                        _exit(0);
                                     }
                                }
                            }

                            ImGui::Separator();

                            if (!udpServer.isActive || !s.UseUDP)
                            {
                                if(s.emuStatus != DS4)
                                {
                                    if (ImGui::CollapsingHeader(strings.LedSection.c_str())) {
                                        ImGui::Separator();
                                   
                                        if (!s.DiscoMode && !s.BatteryLightbar) {
                                            ImGui::Checkbox(strings.AudioToLED.c_str(), &s.AudioToLED);
                                            Tooltip(strings.Tooltip_AudioToLED.c_str());                                         
                                        }

                                        if (!s.AudioToLED && !s.BatteryLightbar) {
                                            ImGui::Checkbox(strings.DiscoMode.c_str(), &s.DiscoMode);                                          
                                            Tooltip(strings.Tooltip_DiscoMode.c_str());
                                        }

                                        if (!s.DiscoMode && !s.AudioToLED) {
                                            ImGui::Checkbox(strings.BatteryLightbar.c_str(), &s.BatteryLightbar);
                                            Tooltip(strings.Tooltip_BatteryLightbar.c_str());
                                        }

                                        if (!s.AudioToLED && !s.DiscoMode && !s.BatteryLightbar) {
                                            ImGui::SliderInt(strings.LED_Red.c_str(),
                                                &s.ControllerInput.Red,
                                                0,
                                                255);
                                            ImGui::SliderInt(strings.LED_Green.c_str(),
                                                &s.ControllerInput.Green,
                                                0,
                                                255);
                                            ImGui::SliderInt(strings.LED_Blue.c_str(),
                                                &s.ControllerInput.Blue,
                                                0,
                                                255);
                                        }

                                        ImGui::Separator();
                                    }
                                }
                                else {
                                    ImGui::Text(strings.LED_DS4_Unavailable.c_str());
                                }

                            if (ImGui::CollapsingHeader(strings.AdaptiveTriggers.c_str()))
                            {

                                ImGui::Checkbox(strings.RumbleToAT.c_str(), &s.RumbleToAT);
                                Tooltip(strings.Tooltip_RumbleToAT.c_str());

                                ImGui::Checkbox(strings.HapticsToTriggers.c_str(), &s.HapticsToTriggers);
                                Tooltip(strings.Tooltip_HapticsToTriggers.c_str());

                                if (s.RumbleToAT || s.HapticsToTriggers) {
                                    ImGui::Checkbox(strings.SwapTriggersRumbleToAT.c_str(), &s.SwapTriggersRumbleToAT);
                                    Tooltip(strings.Tooltip_SwapTriggersRumbleToAT.c_str());

                                    ImGui::Checkbox(strings.RumbleToAT_RigidMode.c_str(), &s.RumbleToAT_RigidMode);
                                    Tooltip(strings.Tooltip_RumbleToAT_RigidMode.c_str());

                                    if (!s.RumbleToAT_RigidMode) {
                                        ImGui::SliderInt(strings.MaxLeftIntensity.c_str(), &s.MaxLeftRumbleToATIntensity, 0, 255);
                                        Tooltip(strings.Tooltip_MaxIntensity.c_str());

                                        ImGui::SliderInt(strings.MaxLeftFrequency.c_str(), &s.MaxLeftRumbleToATFrequency, 0, 25);
                                        Tooltip(strings.Tooltip_MaxFrequency.c_str());

                                        ImGui::Separator();

                                        ImGui::SliderInt(strings.MaxRightIntensity.c_str(), &s.MaxRightRumbleToATIntensity, 0, 255);
                                        Tooltip(strings.Tooltip_MaxIntensity.c_str());

                                        ImGui::SliderInt(strings.MaxRightFrequency.c_str(), &s.MaxRightRumbleToATFrequency, 0, 25);
                                        Tooltip(strings.Tooltip_MaxFrequency.c_str());
                                    }                                 
                                }

                                ImGui::Separator();

                                if(!s.RumbleToAT && !s.HapticsToTriggers)
                                {
                                    if (ImGui::BeginCombo(strings.LeftTriggerMode.c_str(),
                                        s.lmodestr.c_str())) {
                                        if (ImGui::Selectable("Off", false)) {
                                            s.lmodestr = "Off";
                                            s.ControllerInput.LeftTriggerMode =
                                                Trigger::Off;
                                        }
                                        if (ImGui::Selectable("Rigid", false)) {
                                            s.lmodestr = "Rigid";
                                            s.ControllerInput.LeftTriggerMode =
                                                Trigger::Rigid;
                                        }
                                        if (ImGui::Selectable("Pulse", false)) {
                                            s.lmodestr = "Pulse";
                                            s.ControllerInput.LeftTriggerMode =
                                                Trigger::Pulse;
                                        }
                                        if (ImGui::Selectable("Rigid_A", false)) {
                                            s.lmodestr = "Rigid_A";
                                            s.ControllerInput.LeftTriggerMode =
                                                Trigger::Rigid_A;
                                        }
                                        if (ImGui::Selectable("Rigid_B", false)) {
                                            s.lmodestr = "Rigid_B";
                                            s.ControllerInput.LeftTriggerMode =
                                                Trigger::Rigid_B;
                                        }
                                        if (ImGui::Selectable("Rigid_AB", false)) {
                                            s.lmodestr = "Rigid_AB";
                                            s.ControllerInput.LeftTriggerMode =
                                                Trigger::Rigid_AB;
                                        }
                                        if (ImGui::Selectable("Pulse_A", false)) {
                                            s.lmodestr = "Pulse_A";
                                            s.ControllerInput.LeftTriggerMode =
                                                Trigger::Pulse_A;
                                        }
                                        if (ImGui::Selectable("Pulse_B", false)) {
                                            s.lmodestr = "Pulse_B";
                                            s.ControllerInput.LeftTriggerMode =
                                                Trigger::Pulse_B;
                                        }
                                        if (ImGui::Selectable("Pulse_AB", false)) {
                                            s.lmodestr = "Pulse_AB";
                                            s.ControllerInput.LeftTriggerMode =
                                                Trigger::Pulse_AB;
                                        }
                                        if (ImGui::Selectable("Calibration", false)) {
                                            s.lmodestr = "Calibration";
                                            s.ControllerInput.LeftTriggerMode =
                                                Trigger::Calibration;
                                        }

                                        ImGui::EndCombo();
                                    }
                                    ImGui::SliderInt("LT 1",
                                        &s.ControllerInput.LeftTriggerForces[0],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT 2",
                                        &s.ControllerInput.LeftTriggerForces[1],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT 3",
                                        &s.ControllerInput.LeftTriggerForces[2],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT 4",
                                        &s.ControllerInput.LeftTriggerForces[3],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT 5",
                                        &s.ControllerInput.LeftTriggerForces[4],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT 6",
                                        &s.ControllerInput.LeftTriggerForces[5],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT 7",
                                        &s.ControllerInput.LeftTriggerForces[6],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT 8",
                                        &s.ControllerInput.LeftTriggerForces[7],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT 9",
                                        &s.ControllerInput.LeftTriggerForces[8],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT 10",
                                        &s.ControllerInput.LeftTriggerForces[9],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT 11",
                                        &s.ControllerInput.LeftTriggerForces[10],
                                        0,
                                        255);

                                    ImGui::Spacing();

                                    if (ImGui::BeginCombo(strings.RightTriggerMode.c_str(),
                                        s.rmodestr.c_str())) {
                                        if (ImGui::Selectable("Off", false)) {
                                            s.rmodestr = "Off";
                                            s.ControllerInput.RightTriggerMode =
                                                Trigger::Off;
                                        }
                                        if (ImGui::Selectable("Rigid", false)) {
                                            s.rmodestr = "Rigid";
                                            s.ControllerInput.RightTriggerMode =
                                                Trigger::Rigid;
                                        }
                                        if (ImGui::Selectable("Pulse", false)) {
                                            s.rmodestr = "Pulse";
                                            s.ControllerInput.RightTriggerMode =
                                                Trigger::Pulse;
                                        }
                                        if (ImGui::Selectable("Rigid_A", false)) {
                                            s.rmodestr = "Rigid_A";
                                            s.ControllerInput.RightTriggerMode =
                                                Trigger::Rigid_A;
                                        }
                                        if (ImGui::Selectable("Rigid_B", false)) {
                                            s.rmodestr = "Rigid_B";
                                            s.ControllerInput.RightTriggerMode =
                                                Trigger::Rigid_B;
                                        }
                                        if (ImGui::Selectable("Rigid_AB", false)) {
                                            s.rmodestr = "Rigid_AB";
                                            s.ControllerInput.RightTriggerMode =
                                                Trigger::Rigid_AB;
                                        }
                                        if (ImGui::Selectable("Pulse_A", false)) {
                                            s.rmodestr = "Pulse_A";
                                            s.ControllerInput.RightTriggerMode =
                                                Trigger::Pulse_A;
                                        }
                                        if (ImGui::Selectable("Pulse_B", false)) {
                                            s.rmodestr = "Pulse_B";
                                            s.ControllerInput.RightTriggerMode =
                                                Trigger::Pulse_B;
                                        }
                                        if (ImGui::Selectable("Pulse_AB", false)) {
                                            s.rmodestr = "Pulse_AB";
                                            s.ControllerInput.RightTriggerMode =
                                                Trigger::Pulse_AB;
                                        }
                                        if (ImGui::Selectable("Calibration", false)) {
                                            s.rmodestr = "Calibration";
                                            s.ControllerInput.RightTriggerMode =
                                                Trigger::Calibration;
                                        }
                                        ImGui::EndCombo();
                                    }
                                    ImGui::SliderInt("RT 1",
                                        &s.ControllerInput.RightTriggerForces[0],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT 2",
                                        &s.ControllerInput.RightTriggerForces[1],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT 3",
                                        &s.ControllerInput.RightTriggerForces[2],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT 4",
                                        &s.ControllerInput.RightTriggerForces[3],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT 5",
                                        &s.ControllerInput.RightTriggerForces[4],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT 6",
                                        &s.ControllerInput.RightTriggerForces[5],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT 7",
                                        &s.ControllerInput.RightTriggerForces[6],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT 8",
                                        &s.ControllerInput.RightTriggerForces[7],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT 9",
                                        &s.ControllerInput.RightTriggerForces[8],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT 10",
                                        &s.ControllerInput.RightTriggerForces[9],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT 11",
                                        &s.ControllerInput.RightTriggerForces[10],
                                        0,
                                        255);
                                    ImGui::Separator();
                                }
                            }
                            }
                            else {
                                ImGui::Text(strings.LEDandATunavailableUDP.c_str());
                            }
                            

                            if (ImGui::CollapsingHeader(strings.HapticFeedback.c_str()))
                            {
                                ImGui::Text(strings.StandardRumble.c_str());
                                Tooltip(strings.Tooltip_HapticFeedback.c_str());
                                ImGui::SliderInt(strings.LeftMotor.c_str(),
                                                 &s.lrumble,
                                                 0,
                                                 255);
                                ImGui::SliderInt(strings.RightMotor.c_str(),
                                                 &s.rrumble,
                                                 0,
                                                 255);

                                ImGui::Separator();

                                if (HF_status == DualsenseUtils::HapticFeedbackStatus::Working)
                                {
                                    if (ImGui::Button(strings.StartAudioToHaptics.c_str()))
                                    {
                                        STARTUPINFO si;
                                        PROCESS_INFORMATION pi;

                                        ZeroMemory(&si, sizeof(si));
                                        si.cb = sizeof(si);
                                        ZeroMemory(&pi, sizeof(pi));

                                        string id = DualSense[i].GetAudioDeviceName();
                                        string arg1 = string("\"") + id + string("\"");
                                        string command = "utilities\\AudioPassthrough.exe " + arg1;

                                        cout << "Starting audio to haptics" << endl << "Arg1: " << arg1 << endl;

                                        if (CreateProcess(NULL,   // No module name (use command line)
                                            (LPSTR)command.c_str(),        // Command line
                                            NULL,            // Process handle not inheritable
                                            NULL,            // Thread handle not inheritable
                                            FALSE,          // Set handle inheritance to FALSE
                                            0,              // No creation flags
                                            NULL,           // Use parent's environment block
                                            NULL,           // Use parent's starting directory 
                                            &si,            // Pointer to STARTUPINFO structure
                                            &pi)            // Pointer to PROCESS_INFORMATION structure
                                        )
                                        {
                                            // Close process and thread handles. 
                                            CloseHandle(pi.hProcess);
                                            CloseHandle(pi.hThread);
                                        }
                                    };
                                    Tooltip(strings.Tooltip_StartAudioToHaptics.c_str());

                                    ImGui::SliderInt(strings.SpeakerVolume.c_str(), &s.ControllerInput.SpeakerVolume, 0, 255);
                                    Tooltip(strings.Tooltip_SpeakerVolume.c_str());

                                    ImGui::Checkbox("Use headset", &s.UseHeadset);

                                    ImGui::Separator();
                                    ImGui::Text( WStringToString(DualSense[i].GetAudioDeviceInstanceID()).c_str());
                                    ImGui::Text(DualSense[i].GetAudioDeviceName());
                                }
                                else
                                {
                                    if (HF_status == DualsenseUtils::HapticFeedbackStatus::BluetoothNotUSB) {
                                        ImGui::Text(strings.HapticsUnavailable.c_str());
                                    }
                                    else if (HF_status == DualsenseUtils::HapticFeedbackStatus::AudioDeviceNotFound) {
                                        ImGui::Text(strings.HapticsUnavailableNoAudioDevice.c_str());
                                    }
                                }
                              
                            }

                            if (ImGui::CollapsingHeader(strings.Touchpad.c_str()))
                            {
                                    // Define the touchpad's preview size in the UI
                                    ImVec2 touchpadSize(300 * io.FontGlobalScale, 140* io.FontGlobalScale);  // Adjusted to keep a similar aspect ratio (1920:900)

                                    // Draw touchpad background
                                    ImGui::InvisibleButton("##touchpad_bg", touchpadSize);
                                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                                    ImVec2 touchpadPos = ImGui::GetItemRectMin();
                                    int rcolor = DualSense[i].State.touchBtn ? 100 : 50;
                                    drawList->AddRectFilled(touchpadPos, ImVec2(touchpadPos.x + touchpadSize.x, touchpadPos.y + touchpadSize.y), IM_COL32(rcolor, 50, 50, 255));

                                    // Scale touch points to fit within the UI preview
                                    if (DualSense[i].State.trackPadTouch0.IsActive)
                                    {
                                            float scaledX = (DualSense[i].State.trackPadTouch0.X / 1900.0f) * touchpadSize.x;
                                            float scaledY = (DualSense[i].State.trackPadTouch0.Y / 1100.0f) * touchpadSize.y;
                                            ImVec2 touchPos(touchpadPos.x + scaledX, touchpadPos.y + scaledY);
                                            drawList->AddCircleFilled(touchPos, 6.0f * io.FontGlobalScale, IM_COL32(255, 0, 0, 255));                                    
                                    }

                                    if (DualSense[i].State.trackPadTouch1.IsActive)
                                    {
                                            float scaledX = (DualSense[i].State.trackPadTouch1.X / 1900.0f) * touchpadSize.x;
                                            float scaledY = (DualSense[i].State.trackPadTouch1.Y / 1100.0f) * touchpadSize.y;
                                            ImVec2 touchPos(touchpadPos.x + scaledX, touchpadPos.y + scaledY);
                                            drawList->AddCircleFilled(touchPos, 6.0f * io.FontGlobalScale, IM_COL32(255, 0, 0, 255));  // Red circle for active touch
                                    }
                                    ImGui::SameLine();
                                    ImGui::Text("%s:\n%s: %d\n\n%s 1:\nX: %d\nY: %d\n\n%s 2:\nX: %d\nY: %d",
                                        strings.GeneralData.c_str(),
                                        strings.TouchPacketNum.c_str(),
                                        DualSense[i].State.TouchPacketNum,
                                        strings.Touch.c_str(),                                       
                                        DualSense[i].State.trackPadTouch0.X,
                                        DualSense[i].State.trackPadTouch0.Y,
                                        strings.Touch.c_str(), 
                                        DualSense[i].State.trackPadTouch1.X,
                                        DualSense[i].State.trackPadTouch1.Y);

                                    ImGui::Checkbox(strings.TouchpadToMouse.c_str(), &s.TouchpadToMouse);
                                    Tooltip(strings.Tooltip_TouchpadToMouse.c_str());
                                    ImGui::SliderFloat(strings.Sensitivity.c_str(), &s.swipeThreshold, 0.01f, 3.0f);
                            }

                            if (ImGui::CollapsingHeader(strings.MicButton.c_str())) {
                                ImGui::Checkbox(strings.TakeScreenshot.c_str(), &s.MicScreenshot);
                                Tooltip(strings.Tooltip_TakeScreenshot.c_str());

                                ImGui::Checkbox(strings.RealMicFunctionality.c_str(), &s.MicFunc);
                                Tooltip(strings.Tooltip_RealMicFunctionality.c_str());

                                if (!s.MicFunc) {
                                    DualSense[i].SetMicrophoneLED(false, false);
                                    DualSense[i].SetMicrophoneVolume(80);
                                }

                                ImGui::Checkbox(strings.TouchpadShortcut.c_str(), &s.TouchpadToMouseShortcut);
                                ImGui::Checkbox(strings.SwapTriggersShortcut.c_str(), &s.SwapTriggersShortcut);
                                ImGui::Checkbox(strings.X360Shortcut.c_str(), &s.X360Shortcut);
                                ImGui::Checkbox(strings.DS4Shortcut.c_str(), &s.DS4Shortcut);
                                ImGui::Checkbox(strings.StopEmuShortcut.c_str(), &s.StopEmuShortcut);
                            }

                            if (ImGui::CollapsingHeader(strings.EmulationHeader.c_str()))
                            {
                                if (s.emuStatus == None)
                                {
                                    if (ImGui::Button(strings.X360emu.c_str()))
                                    {
                                        RunAsyncHidHideRequest(DualSense[i].GetPath(), "hide");
                                        s.emuStatus = X360;                                      
                                    }
                                    ImGui::SameLine();
                                    if (ImGui::Button(strings.DS4emu.c_str()))
                                    {
                                        RunAsyncHidHideRequest(DualSense[i].GetPath(), "hide");
                                        DualSense[i].SetLightbar(0, 0, 64);
                                        s.emuStatus = DS4;
                                    }
                                    ImGui::SameLine();
                                    ImGui::Checkbox(strings.DualShock4V2emu.c_str(), &s.DualShock4V2);
                                    Tooltip(strings.Tooltip_Dualshock4V2.c_str());
                                }
                                else
                                {
                                    if (ImGui::Button(strings.STOPemu.c_str()))
                                    {
                                        RunAsyncHidHideRequest(DualSense[i].GetPath(), "show");
                                        s.emuStatus = None;
                                    }
                                }
                                
                                if (!WasElevated)                               
                                {
                                    ImGui::Text(strings.ControllerEmuUserMode.c_str());
                                }
                                else {
                                    ImGui::Text(strings.ControllerEmuAppAsAdmin.c_str());
                                }

                                ImGui::Separator();

                                ImGui::SliderInt(strings.LeftAnalogStickDeadZone.c_str(), &s.LeftAnalogDeadzone, 0, 127);
                                ImGui::SliderInt(strings.RightAnalogStickDeadZone.c_str(), &s.RightAnalogDeadzone, 0, 127);
                                ImGui::Checkbox(strings.TriggersAsButtons.c_str(), &s.TriggersAsButtons);
                                Tooltip(strings.Tooltip_TriggersAsButtons.c_str());
                                ImGui::Checkbox(strings.TouchpadAsSelectStart.c_str(), &s.TouchpadAsSelectStart);
                            }

                            if (ImGui::CollapsingHeader(strings.ConfigHeader.c_str())) {
                                ImGui::SetNextItemWidth(120.0f);
                                if (ImGui::Combo(strings.Language.c_str(), &selectedLanguageIndex, languageItems.data(), languageItems.size())) {
                                    const std::string& selectedLanguage = languages[selectedLanguageIndex];                                    
                                    appConfig.Language = selectedLanguage;
                                    Config::WriteAppConfigToFile(appConfig);
                                }

                                if (ImGui::Button(strings.SaveConfig.c_str())) {
                                    Config::WriteToFile(s);
                                }
                                Tooltip(strings.Tooltip_SaveConfig.c_str());

                                if (ImGui::Button(strings.LoadConfig.c_str())) {
                                    std::string configPath = Config::GetConfigPath();
                                    if (configPath != "") {
                                        s = Config::ReadFromFile(configPath);
                                        s.ControllerInput.ID = DualSense[i].GetPath();
                                    }                                   
                                }
                                Tooltip(strings.Tooltip_LoadConfig.c_str());

                                if (ImGui::Button(strings.SetDefaultConfig.c_str())) {
                                    std::string configPath = Config::GetConfigPath();
                                    if (configPath != "") {
                                        MyUtils::WriteDefaultConfigPath(configPath, DualSense[i].GetPath());
                                    }                                   
                                }
                                Tooltip(strings.Tooltip_SetDefaultConfig.c_str());

                                if (ImGui::Button(strings.RemoveDefaultConfig.c_str())) {
                                   MyUtils::RemoveConfig(DualSense[i].GetPath());
                                }
                                Tooltip(strings.Tooltip_RemoveDefaultConfig.c_str());

                                if (ImGui::Checkbox(strings.RunAsAdmin.c_str(), &appConfig.ElevateOnStartup)) {
                                    Config::WriteAppConfigToFile(appConfig);
                                }
                                Tooltip(strings.Tooltip_RunAsAdmin.c_str());

                                if (ImGui::Checkbox(strings.HideToTray.c_str() , &appConfig.HideToTray)) {
                                    Config::WriteAppConfigToFile(appConfig);
                                }
                                Tooltip(strings.Tooltip_HideToTray.c_str());

                                if (ImGui::Checkbox(strings.HideWindowOnStartup.c_str(), &appConfig.HideWindowOnStartup)) {
                                    Config::WriteAppConfigToFile(appConfig);
                                }
                                Tooltip(strings.Tooltip_HideWindowOnStartup.c_str());

                                if (ImGui::Checkbox(strings.RunWithWindows.c_str(), &appConfig.RunWithWindows)) {
                                    if (appConfig.RunWithWindows) {
                                        MyUtils::AddToStartup();
                                    }
                                    else {
                                        MyUtils::RemoveFromStartup();
                                    }

                                    Config::WriteAppConfigToFile(appConfig);
                                }
                                Tooltip(strings.RunWithWindows.c_str());

                                if (ImGui::Checkbox(strings.ShowConsole.c_str(), &appConfig.ShowConsole)) {
                                    Config::WriteAppConfigToFile(appConfig);
                                }
                            }                         
                        }
                    }
                }
                else
                {
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        ImGui::End();
        ImGui::Render();

        end_cycle(window);       
    }

    stop_thread = true; // Signal all threads to stop

    std::this_thread::sleep_for(std::chrono::microseconds(30));

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

    if(WasElevated)
    {
        for (Dualsense& ds : DualSense) {
            StartHidHideRequest(ds.GetPath(), "show");
            ds.~Dualsense();
        }
    }

    udpServer.Dispose();
    if (trayThread.joinable())
        trayThread.join();
    tray.exit();
    tray.~Tray();
   
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    hid_exit();

    DeinitializeAudioEndpoint();
    //_CrtDumpMemoryLeaks();

    timeEndPeriod(1);
    return 0;
}

void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
