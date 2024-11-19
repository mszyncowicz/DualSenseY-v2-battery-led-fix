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

namespace MyUtils {
    std::string GetExecutablePath();
    std::string GetExecutableFolderPath();
    void AddToStartup();
    void RemoveFromStartup();
    std::string GetImagesFolderPath();
    std::string GetLocalFolderPath();
    int scaleFloatToInt(float input_float);
    std::string currentDateTime();
    std::string currentDateTimeWMS();
    bool GetConfigPathForController(std::string& Path, std::string ControllerID);
    bool WriteDefaultConfigPath(std::string Path, std::string ControllerID);
    void RemoveConfig(std::string ControllerID);
    void SaveBitmapFromClipboard(const char* filename);
    void TakeScreenShot();
}

