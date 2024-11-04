#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h"
#include <filesystem>
#include <iostream>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>

#include "DualSense.h"
#include "ControllerEmulation.h"
#include "IMMNotificationClient.h"
#include "cycle.hpp"
#include "miniaudio.h"
#include "Config.h"
#include "UDP.h"
#include "MyUtils.h"
#include "Settings.h"
#include <audioclient.h>
#include <chrono>
#include <ctime>
#include <ratio>
#include <deque>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <mutex>
#include <thread>
#include <windows.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) &&                                 \
    !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")

#endif

static void glfw_error_callback(int error, const char *description);

#include <windows.h>
#include <string>

deque<Dualsense> DualSense;
std::mutex buffer_mutex;
std::atomic<bool> stop_thread{false}; // Flag to signal the thread to stop

NotificationClient *client;
IMMDeviceEnumerator *deviceEnumerator = nullptr;
IMMDevice *device = nullptr;
IAudioMeterInformation *meterInfo = nullptr;

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
}

string WStringToString(const wstring& wstr)
{
	string str;
	size_t size;
	str.resize(wstr.length());
	wcstombs_s(&size, &str[0], str.size() + 1, wstr.c_str(), wstr.size());
	return str;
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
        CoUninitialize();
        InitializeAudioEndpoint();
        client->DeviceChanged = false;
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

bool AudioToLEDfunc(Settings &f)
{
    if (f.AudioToLED)
    {
        int peak = static_cast<int>(GetCurrentAudioPeak() * 500);
        f.ControllerInput.Red = peak;
        f.ControllerInput.Green = peak;
        f.ControllerInput.Blue = peak;
        return true;
    }
    else
    {
        return false;
    }
}

void readControllerState(Dualsense &controller)
{
    while (!stop_thread)
    {
        controller.Read(); // Perform the read operation
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
}

void writeEmuController(Dualsense &controller, Settings &settings)
{
    ViGEm v;
    v.InitializeVigembus();
    EmuStatus curStatus = None;

    while (!stop_thread)
    {
        if (controller.Connected)
        {
            if (settings.emuStatus != None && !v.isWorking())
            {
                MessageBox(0, "ViGEm connection failed! Are you sure you installed ViGEmBus driver?" ,"Error", 0);
                settings.emuStatus = None;
                continue;
            }

            if (curStatus == None && settings.emuStatus == X360)
            {
                v.StartX360();
                curStatus = X360;
            }
            else if (curStatus == None && settings.emuStatus == DS4)
            {
                v.StartDS4();
                curStatus = DS4;
            }
            else if (curStatus != None && settings.emuStatus == None)
            {
                v.RemoveController();
                curStatus = None;
            }

            if (curStatus == X360)
            {
                v.UpdateX360(controller.State);
                settings.lrumble = v.rotors.lrotor;
                settings.rrumble = v.rotors.rrotor;
            }
            else if (curStatus == DS4)
            {
                v.UpdateDS4(controller.State);
                settings.lrumble = v.rotors.lrotor;
                settings.rrumble = v.rotors.rrotor;
                if (v.Red > 0 || v.Green > 0 || v.Blue > 0)
                {
                    settings.ControllerInput.Red = v.Red;
                    settings.ControllerInput.Green = v.Green;
                    settings.ControllerInput.Blue = v.Blue;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        else
        {
            if (settings.emuStatus != None) {
                curStatus = None;
                v.RemoveController();
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void writeControllerState(Dualsense &controller, Settings &settings)
{  
    cout << "Write thread started: " << controller.GetPath() << " | "
         << settings.ControllerInput.ID << endl;

    // DISCO MODE VARIABLES
    int colorState = 0; // 0 = red to yellow, 1 = yellow to green, 2 = green to cyan, etc.
    uint8_t led[3];
    int step = 5; 
    led[0] = 255;
    led[1] = 0;
    led[2] = 0;
    bool lastMic = false;
    bool mic = false;

    while (!stop_thread)
    {
        if (controller.Connected)
        {
            controller.SetLightbar(settings.ControllerInput.Red,
                                   settings.ControllerInput.Green,
                                   settings.ControllerInput.Blue);

            if (settings.RumbleToAT) {
                settings.ControllerInput.LeftTriggerMode = Trigger::Pulse_B;
                settings.ControllerInput.RightTriggerMode = Trigger::Pulse_B;

                // set frequency
                if (settings.lrumble < 25)
                    settings.ControllerInput.LeftTriggerForces[0] = settings.lrumble;
                else
                    settings.ControllerInput.LeftTriggerForces[0] = 25;

                if(settings.rrumble < 25)
                    settings.ControllerInput.RightTriggerForces[0] = settings.rrumble;
                else
                    settings.ControllerInput.RightTriggerForces[0] = 25;

                // set power
                settings.ControllerInput.LeftTriggerForces[1] = settings.lrumble;
                settings.ControllerInput.RightTriggerForces[1] = settings.rrumble;

                // set static threshold
                settings.ControllerInput.LeftTriggerForces[2] = 20;
                settings.ControllerInput.RightTriggerForces[2] = 20;
            }

            controller.SetRightTrigger(
                                      settings.ControllerInput.RightTriggerMode,
                                      settings.ControllerInput.RightTriggerForces[0],
                                      settings.ControllerInput.RightTriggerForces[1],
                                      settings.ControllerInput.RightTriggerForces[2],
                                      settings.ControllerInput.RightTriggerForces[3],
                                      settings.ControllerInput.RightTriggerForces[4],
                                      settings.ControllerInput.RightTriggerForces[5],
                                      settings.ControllerInput.RightTriggerForces[6]);

            controller.SetLeftTrigger(settings.ControllerInput.LeftTriggerMode,
                                      settings.ControllerInput.LeftTriggerForces[0],
                                      settings.ControllerInput.LeftTriggerForces[1],
                                      settings.ControllerInput.LeftTriggerForces[2],
                                      settings.ControllerInput.LeftTriggerForces[3],
                                      settings.ControllerInput.LeftTriggerForces[4],
                                      settings.ControllerInput.LeftTriggerForces[5],
                                      settings.ControllerInput.LeftTriggerForces[6]);

            controller.SetLightbar(settings.ControllerInput.Red, settings.ControllerInput.Green, settings.ControllerInput.Blue);

            if (settings.AudioToLED)
            {
                AudioToLEDfunc(settings);
            }

            if (settings.DiscoMode)
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

            if (settings.MicScreenshot) {
                if (controller.State.micBtn && !lastMic) {
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
                if (controller.State.micBtn && !lastMic) {
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
                    lastMic = true;
                }            
            }

            if (!controller.State.micBtn && lastMic) {
                lastMic = false;
            }


            controller.UseRumbleNotHaptics(false);

            if (settings.lrumble > 0 || settings.rrumble > 0)
            {
                controller.UseRumbleNotHaptics(true);
            }
            if (settings.emuStatus != None)
            {
                controller.UseRumbleNotHaptics(true);
            }

            controller.SetRumble(settings.lrumble, settings.rrumble);
            controller.Write();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(25));
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


int main(int, char **)
{
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    ShowWindow(GetConsoleWindow(), SW_RESTORE);

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

    // Create window with graphics context
    auto *window = glfwCreateWindow(static_cast<std::int32_t>(1200),
                                    static_cast<std::int32_t>(900),
                                    "DualSenseY",
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
    const char *CurrentController = "";
    bool firstController = true;
    bool firstTimeUDP = true;
    DualsenseUtils::InputFeatures preUDP;
    vector<const char*> ControllerID = DualsenseUtils::EnumerateControllerIDs();
    int ControllerCount = DualsenseUtils::GetControllerCount();
    std::chrono::high_resolution_clock::time_point LastControllerCheck = std::chrono::high_resolution_clock::now();

    InitializeAudioEndpoint();

    UDP udpServer;
    udpServer.StartFakeDSXProcess();
    
    ImGuiIO &io = ImGui::GetIO();
    ImFont* font_title = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\Roboto-Bold.ttf", 15.0f, NULL, io.Fonts->GetGlyphRangesDefault()); 

    while (!glfwWindowShouldClose(window))
    {
        start_cycle();
        ImGui::NewFrame();

        ImGuiStyle *style = &ImGui::GetStyle();

        // Get the viewport size (size of the application window)
        
        ImVec2 window_size = io.DisplaySize;

        // Set window flags to remove the ability to resize, move, or close the window
        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

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
            if ((Now - LastControllerCheck) > 1s) {
                LastControllerCheck = std::chrono::high_resolution_clock::now();
                ControllerID = DualsenseUtils::EnumerateControllerIDs();
                ControllerCount = DualsenseUtils::GetControllerCount();
            }
            
            if (ImGui::BeginCombo("##", CurrentController))
            {
                for (int n = 0; n < ControllerID.size(); n++)
                {
                    if (strcmp(ControllerID[n], "") != 0) {
                        bool is_selected = (CurrentController == ControllerID[n]);
                        if (ImGui::Selectable(ControllerID[n], is_selected))
                        {
                            CurrentController = ControllerID[n];
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
            ImGui::Text("UDP Status: Inactive");
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
            ImGui::Text("UDP Status: Active");
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
            for (const char *id : ControllerID)
            {
                bool IsPresent = false;

                for (Dualsense &ds : DualSense)
                {
                    if (strcmp(id, ds.GetPath()) == 0)
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
                        Dualsense x = Dualsense(id);
                        if (x.Connected)
                        {
                            DualSense.emplace_back(x);
                            Settings s;
                            ControllerSettings.emplace_back(s);
                            ControllerSettings.back().ControllerInput.ID = id;
                            if (firstController)
                            {
                                ControllerSettings.back().UseUDP = true;
                                firstController = false;
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
                            Settings udpSettings = udpServer.GetSettings();
                            udpServer.Battery = DualSense[i].State.battery.Level;               
                            s.AudioToLED = false;
                            s.DiscoMode = false;
                            s.RumbleToAT = false;
                            s.ControllerInput.Red = udpSettings.ControllerInput.Red;
                            s.ControllerInput.Green = udpSettings.ControllerInput.Green;
                            s.ControllerInput.Blue = udpSettings.ControllerInput.Blue;
                            s.ControllerInput.LeftTriggerForces[0] = udpSettings.ControllerInput.LeftTriggerForces[0];
                            s.ControllerInput.LeftTriggerForces[1] = udpSettings.ControllerInput.LeftTriggerForces[1];
                            s.ControllerInput.LeftTriggerForces[2] = udpSettings.ControllerInput.LeftTriggerForces[2];
                            s.ControllerInput.LeftTriggerForces[3] = udpSettings.ControllerInput.LeftTriggerForces[3];
                            s.ControllerInput.LeftTriggerForces[4] = udpSettings.ControllerInput.LeftTriggerForces[4];
                            s.ControllerInput.LeftTriggerForces[5] = udpSettings.ControllerInput.LeftTriggerForces[5];
                            s.ControllerInput.LeftTriggerForces[6] = udpSettings.ControllerInput.LeftTriggerForces[6];
                            s.ControllerInput.LeftTriggerMode = udpSettings.ControllerInput.LeftTriggerMode;

                            s.ControllerInput.RightTriggerForces[0] = udpSettings.ControllerInput.RightTriggerForces[0];
                            s.ControllerInput.RightTriggerForces[1] = udpSettings.ControllerInput.RightTriggerForces[1];
                            s.ControllerInput.RightTriggerForces[2] = udpSettings.ControllerInput.RightTriggerForces[2];
                            s.ControllerInput.RightTriggerForces[3] = udpSettings.ControllerInput.RightTriggerForces[3];
                            s.ControllerInput.RightTriggerForces[4] = udpSettings.ControllerInput.RightTriggerForces[4];
                            s.ControllerInput.RightTriggerForces[5] = udpSettings.ControllerInput.RightTriggerForces[5];
                            s.ControllerInput.RightTriggerForces[6] = udpSettings.ControllerInput.RightTriggerForces[6];
                            s.ControllerInput.RightTriggerMode = udpSettings.ControllerInput.RightTriggerMode;

                            std::chrono::high_resolution_clock::time_point Now = std::chrono::high_resolution_clock::now();
                            if ((Now - udpServer.LastTime) > 8s) {
                                udpServer.isActive = false;
                                firstTimeUDP = true;
                                s.ControllerInput = preUDP;
                            }
                        }                      
                    }
                }

                if (strcmp(DualSense[i].GetPath(), CurrentController) == 0 && DualSense[i].Connected)
                {                  
                    for (Settings &s : ControllerSettings)
                    {
                        if (strcmp(s.ControllerInput.ID.c_str(), CurrentController) == 0)
                        {
                            const char* bt_or_usb = DualSense[i].GetConnectionType() == Feature::USB ? "USB" : "BT";
                            ImGui::Text("Controller No. %d | Connection type: %s | Battery level: %d%%", i+1, bt_or_usb ,DualSense[i].State.battery.Level);
                            ImGui::Separator();

                            if (!udpServer.isActive || !s.UseUDP)
                            {
                                if(s.emuStatus != DS4)
                                {
                                    if (ImGui::CollapsingHeader("LED")) {
                                        ImGui::Separator();

                                        if (!s.DiscoMode) {
                                            if (ImGui::Checkbox("Audio to LED", &s.AudioToLED)) {
                                                if (!s.AudioToLED) {
                                                    s.ControllerInput.Red = 0;
                                                    s.ControllerInput.Green = 0;
                                                    s.ControllerInput.Blue = 0;
                                                }
                                            }
                                        }

                                        if (!s.AudioToLED) {
                                            ImGui::Checkbox("Disco Mode", &s.DiscoMode);
                                        }


                                        if (!s.AudioToLED && !s.DiscoMode) {
                                            ImGui::SliderInt("Red",
                                                &s.ControllerInput.Red,
                                                0,
                                                255);
                                            ImGui::SliderInt("Green",
                                                &s.ControllerInput.Green,
                                                0,
                                                255);
                                            ImGui::SliderInt("Blue",
                                                &s.ControllerInput.Blue,
                                                0,
                                                255);
                                        }

                                        ImGui::Separator();
                                    }
                                }
                                else {
                                    ImGui::Text("LED settings are unavailable while emulating DualShock 4");
                                }

                            if (ImGui::CollapsingHeader("Adaptive Triggers"))
                            {
                                ImGui::Checkbox("Rumble to Adaptive Triggers", &s.RumbleToAT);

                                ImGui::Separator();

                                if(!s.RumbleToAT)
                                {
                                    if (ImGui::BeginCombo("Left Trigger Mode",
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
                                    ImGui::SliderInt("LT Force 1",
                                        &s.ControllerInput.LeftTriggerForces[0],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT Force 2",
                                        &s.ControllerInput.LeftTriggerForces[1],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT Force 3",
                                        &s.ControllerInput.LeftTriggerForces[2],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT Force 4",
                                        &s.ControllerInput.LeftTriggerForces[3],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT Force 5",
                                        &s.ControllerInput.LeftTriggerForces[4],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT Force 6",
                                        &s.ControllerInput.LeftTriggerForces[5],
                                        0,
                                        255);
                                    ImGui::SliderInt("LT Force 7",
                                        &s.ControllerInput.LeftTriggerForces[6],
                                        0,
                                        255);

                                    ImGui::Spacing();

                                    if (ImGui::BeginCombo("Right Trigger Mode",
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
                                    ImGui::SliderInt("RT Force 1",
                                        &s.ControllerInput.RightTriggerForces[0],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT Force 2",
                                        &s.ControllerInput.RightTriggerForces[1],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT Force 3",
                                        &s.ControllerInput.RightTriggerForces[2],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT Force 4",
                                        &s.ControllerInput.RightTriggerForces[3],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT Force 5",
                                        &s.ControllerInput.RightTriggerForces[4],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT Force 6",
                                        &s.ControllerInput.RightTriggerForces[5],
                                        0,
                                        255);
                                    ImGui::SliderInt("RT Force 7",
                                        &s.ControllerInput.RightTriggerForces[6],
                                        0,
                                        255);
                                    ImGui::Separator();
                                }
                            }
                            }
                            else {
                                ImGui::Text("LED and Adaptive Trigger settings are unavailable while UDP is active");
                            }
                            

                            if (ImGui::CollapsingHeader("Haptic Feedback"))
                            {
                                ImGui::Text("Standard rumble");
                                ImGui::SameLine();
                                ImGui::TextDisabled("(?)");
                                if (ImGui::BeginItemTooltip())
                                {
                                    ImGui::PushTextWrapPos(
                                        ImGui::GetFontSize() * 35.0f);
                                    ImGui::TextUnformatted(
                                        "Controls standard rumble values of "
                                        "your dualsense.\nThis is what your "
                                        "controller does to emulate DualShock "
                                        "4 rumble motors.");
                                    ImGui::PopTextWrapPos();
                                    ImGui::EndTooltip();
                                }
                                ImGui::SliderInt("Left \"Motor\"",
                                                 &s.lrumble,
                                                 0,
                                                 255);
                                ImGui::SliderInt("Right \"Motor\"",
                                                 &s.rrumble,
                                                 0,
                                                 255);

                                if (DualSense[i].GetConnectionType() == Feature::USB)
                                {
                                    if (ImGui::Button("Start [Audio To Haptics]"))
                                    {
                                        STARTUPINFO si;
                                        PROCESS_INFORMATION pi;

                                        ZeroMemory(&si, sizeof(si));
                                        si.cb = sizeof(si);
                                        ZeroMemory(&pi, sizeof(pi));

                                        string id = WStringToString(DualSense[i].GetKnownAudioParentInstanceID());
                                        string arg1 = string("\"") + id + string("\"");
                                        string command = "AudioPassthrough.exe " + arg1;

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

                                    ImGui::SameLine();
                                    ImGui::TextDisabled("(?)");
                                    if (ImGui::BeginItemTooltip())
                                    {
                                        ImGui::PushTextWrapPos(
                                        ImGui::GetFontSize() * 35.0f);
                                        ImGui::TextUnformatted("Creates haptic feedback from your system audio.");
                                        ImGui::PopTextWrapPos();
                                        ImGui::EndTooltip();
                                    }
                                }
                                else
                                {
                                    ImGui::Text("Haptic Feedback features are unavailable in Bluetooth mode.");
                                }
                              
                            }

                            if (ImGui::CollapsingHeader("Touchpad"))
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
                                    ImGui::Text("General data:\nTouch packet number: %d\n\nTouch 1:\nX: %d\nY: %d\n\nTouch 2:\nX: %d\nY: %d",
                                        DualSense[i].State.TouchPacketNum,
                                        DualSense[i].State.trackPadTouch0.X,
                                        DualSense[i].State.trackPadTouch0.Y,
                                        DualSense[i].State.trackPadTouch1.X,
                                        DualSense[i].State.trackPadTouch1.Y);
                                    
                            }

                            if (ImGui::CollapsingHeader("Microphone button")) {
                                ImGui::Checkbox("Take screenshot", &s.MicScreenshot);
                                ImGui::Checkbox("Real functionality", &s.MicFunc);
                                if (!s.MicFunc) {
                                    DualSense[i].SetMicrophoneLED(false, false);
                                    DualSense[i].SetMicrophoneVolume(80);
                                }
                                ImGui::SameLine();
                                ImGui::TextDisabled("(?)");
                                if (ImGui::BeginItemTooltip())
                                {
                                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                                    ImGui::TextUnformatted("Mimics microphone button functionality from the PS5, only works on this controller's microphone.");
                                    ImGui::PopTextWrapPos();
                                    ImGui::EndTooltip();
                                }
                            }

                            if (ImGui::CollapsingHeader("Controller emulation (DS4/X360)"))
                            {
                                if (s.emuStatus == None)
                                {
                                    if (ImGui::Button("Start X360 emulation"))
                                    {
                                        s.emuStatus = X360;
                                    }
                                    ImGui::SameLine();
                                    if (ImGui::Button("Start DS4 emulation"))
                                    {
                                        s.AudioToLED = false;
                                        s.DiscoMode = false;
                                        s.ControllerInput.Red = 0;
                                        s.ControllerInput.Green = 0;
                                        s.ControllerInput.Blue = 255;
                                        s.emuStatus = DS4;
                                    }
                                }
                                else
                                {
                                    if (ImGui::Button("Stop Emulation"))
                                    {
                                        s.emuStatus = None;
                                    }
                                }


                                if (ImGui::Button("Hide real controller"))
                                {
                                    string arg1 = string("\"") + DualSense[i].GetPath() + string("\"");
                                    string arg2 = " \"hide\"";
                                    string finalarg = "hidhide_service_request.exe " + arg1 + arg2;
                                    cout << finalarg << endl;
                                    int retCode = system(finalarg.c_str());
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("Restore real controller"))
                                {
                                    string arg1 = string("\"") + DualSense[i].GetPath() + string("\"");
                                    string arg2 = " \"show\"";
                                    string finalarg = "start hidhide_service_request.exe " + arg1 + arg2;
                                    cout << finalarg << endl;
                                    int retCode = system(finalarg.c_str());
                                }
                                ImGui::Text("IMPORTANT: UAC prompt should appear in your taskbar when you hide/show\n           please run it to unfreeze the application");
                            }

                            if (ImGui::CollapsingHeader("Config")) {
                                if (ImGui::Button("Save current configuration")) {
                                    Config::WriteToFile(s);
                                }

                                if (ImGui::Button("Load configuration")) {
                                    s = Config::ReadFromFile();
                                    s.ControllerInput.ID = DualSense[i].GetPath();
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

    udpServer.Dispose();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    hid_exit();
    return 0;
}

void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
