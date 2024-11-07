#include <iostream>
#include <string>
#include <fstream>
#include <ctime>
#include <windows.h>
#include <chrono>
#include <shlobj.h>

namespace MyUtils {
    std::string GetImagesFolderPath();
    std::string GetLocalFolderPath();
    std::string currentDateTime();
    std::string currentDateTimeWMS();
    void SaveBitmapFromClipboard(const char* filename);
    void TakeScreenShot();
}

