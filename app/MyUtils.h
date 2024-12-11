#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>


#include "stb_image.h"

#include <string>
#include <fstream>
#include <ctime>
#include <windows.h>
#include <chrono>
#include <shlobj.h>
#include <filesystem>
#include <audioclient.h>
#include <ratio>
#include <deque>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <thread>
#include <iostream>
#include <windowsx.h>
#include <shlobj.h>
#include <stdio.h>
#include <ShellAPI.h>
#include <tchar.h>
#include <algorithm>
#include <cpr/cpr.h>
#include <tray.hpp>
#include <nlohmann/json.hpp>
#include <crashlogs.h>
#include <TlHelp32.h>
#include <SetupAPI.h>
#include <timeapi.h>

namespace MyUtils {
    bool LoadTextureFromMemory(const void* data, size_t data_size, GLuint* out_texture, int* out_width, int* out_height);
    bool LoadTextureFromFile(const char* file_name, GLuint* out_texture, int* out_width, int* out_height);
    std::string GetExecutablePath();
    std::string GetExecutableFolderPath();
    void AddToStartup();
    void RemoveFromStartup();
    std::string GetImagesFolderPath();
    std::string GetLocalFolderPath();
    int scaleFloatToInt(float input_float);
    std::string currentDateTime();
    std::string currentDateTimeWMS();
    std::string USBtoHIDinstance(const std::string& input);
    std::wstring ConvertToWideString(const std::string& str);
    bool GetConfigPathForController(std::string& Path, std::string ControllerID);
    bool WriteDefaultConfigPath(std::string Path, std::string ControllerID);
    void RemoveConfig(std::string ControllerID);
    void SaveBitmapFromClipboard(const char* filename);
    void TakeScreenShot();
}

