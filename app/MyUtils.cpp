#include "MyUtils.h"
#include <errno.h>
#define SYSERROR()  errno

namespace MyUtils {
    std::string GetImagesFolderPath() {
        char path[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_MYPICTURES, NULL, 0, path) == S_OK) {
            return std::string(path);
        }
        return "";
    }

    std::string GetLocalFolderPath() {
        char path[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path) == S_OK) {
            return std::string(path);
        }
        return "";
    }

    std::string currentDateTime()
    {
        std::time_t now = std::time(nullptr);
        std::tm *nowTm = std::localtime(&now);
        char buf[100];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", nowTm);
        return buf;
    }

    std::string sanitizeFileName(const std::string& input) {
        std::string sanitized = input;
        const std::string invalidChars = "\\/:*?\"<>|";

        sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), [&](char c) {
            return invalidChars.find(c) != std::string::npos;
            }), sanitized.end());

        return sanitized;
    }

    bool GetConfigPathForController(std::string &Path, std::string ControllerID) {
        std::ifstream configFile(MyUtils::GetLocalFolderPath() + "\\DualSenseY\\DefaultConfigs\\" + sanitizeFileName(ControllerID));
        if (configFile.good()) {
            getline(configFile, Path);
            configFile.close();
            return true;
        }
        else {
            std::cerr<<"Failed to open file : "<<SYSERROR()<<std::endl;
            return false;
        }
    }

    bool WriteDefaultConfigPath(std::string Path, std::string ControllerID) {
        std::filesystem::create_directories(MyUtils::GetLocalFolderPath() + "\\DualSenseY\\DefaultConfigs");
        std::ofstream configFile(MyUtils::GetLocalFolderPath() + "\\DualSenseY\\DefaultConfigs\\" + sanitizeFileName(ControllerID));
        if (configFile.is_open()) {
            configFile << Path;
            configFile.close();
            return true;
        }
        else {
            std::cerr<<"Failed to open file : "<<SYSERROR()<<std::endl;
            return false;
        }
    }

    void RemoveConfig(std::string ControllerID) {
        std::string path = MyUtils::GetLocalFolderPath() + "\\DualSenseY\\DefaultConfigs\\" + sanitizeFileName(ControllerID);
        std::remove(path.c_str());
    }

    std::string currentDateTimeWMS()
    {
        auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm *nowTm = std::localtime(&now_c);
    
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(nowTm, "%Y-%m-%d %H-%M-%S") << '-' << std::setfill('0') << std::setw(3) << milliseconds.count();
    return oss.str();
    }

    void SaveBitmapFromClipboard(const char* filename) {
    // Open the clipboard
    if (!OpenClipboard(NULL)) {
        std::cerr << "Failed to open clipboard" << std::endl;
        return;
    }

    // Get the bitmap from the clipboard
    HBITMAP hBitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
    if (!hBitmap) {
        std::cerr << "No bitmap available in clipboard" << std::endl;
        CloseClipboard();
        return;
    }

    // Create a file to save the bitmap
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to create file" << std::endl;
        CloseClipboard();
        return;
    }

    // Get bitmap information
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    // Create a BITMAPFILEHEADER
    BITMAPFILEHEADER bfh;
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (bmp.bmWidthBytes * bmp.bmHeight);
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    // Create a BITMAPINFOHEADER
    BITMAPINFOHEADER bih;
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = bmp.bmWidth;
    bih.biHeight = bmp.bmHeight;
    bih.biPlanes = 1;
    bih.biBitCount = 32; // Assuming 32 bits per pixel (ARGB)
    bih.biCompression = BI_RGB;
    bih.biSizeImage = 0;
    bih.biXPelsPerMeter = 0;
    bih.biYPelsPerMeter = 0;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;

    // Write headers to file
    file.write(reinterpret_cast<char*>(&bfh), sizeof(BITMAPFILEHEADER));
    file.write(reinterpret_cast<char*>(&bih), sizeof(BITMAPINFOHEADER));

    // Lock the bitmap bits
    HDC hdc = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hdc);
    SelectObject(hMemDC, hBitmap);
    
    // Write pixel data
    int widthBytes = bmp.bmWidthBytes;
    BYTE* pPixels = new BYTE[widthBytes * bmp.bmHeight];
    GetBitmapBits(hBitmap, widthBytes * bmp.bmHeight, pPixels);

    // Flip the pixel data vertically
    BYTE* pFlippedPixels = new BYTE[widthBytes * bmp.bmHeight];
    for (int y = 0; y < bmp.bmHeight; ++y) {
        memcpy(pFlippedPixels + (bmp.bmHeight - 1 - y) * widthBytes, 
               pPixels + y * widthBytes, 
               widthBytes);
    }

    // Write flipped pixel data to the file
    file.write(reinterpret_cast<char*>(pFlippedPixels), widthBytes * bmp.bmHeight);

    // Clean up
    delete[] pPixels;
    delete[] pFlippedPixels;
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hdc);
    CloseClipboard();
    file.close();

    std::cout << "Bitmap saved to " << filename << std::endl;
}

    void TakeScreenShot() {
        keybd_event(VK_CONTROL, 0, 0, 0); 
        keybd_event(VK_SNAPSHOT, 0, 0, 0); 

        keybd_event(VK_SNAPSHOT, 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    }
}
