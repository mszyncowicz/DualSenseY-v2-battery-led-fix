#include "MyUtils.h"
#include <errno.h>
#define SYSERROR()  errno

namespace MyUtils {
    bool LoadTextureFromMemory(const void* data, size_t data_size, GLuint* out_texture, int* out_width, int* out_height)
    {
        // Load from file
        int image_width = 0;
        int image_height = 0;
        unsigned char* image_data = stbi_load_from_memory((const unsigned char*)data, (int)data_size, &image_width, &image_height, NULL, 4);
        if (image_data == NULL)
            return false;

        // Create a OpenGL texture identifier
        GLuint image_texture;
        glGenTextures(1, &image_texture);
        glBindTexture(GL_TEXTURE_2D, image_texture);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Upload pixels into texture
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        stbi_image_free(image_data);

        *out_texture = image_texture;
        *out_width = image_width;
        *out_height = image_height;

        return true;
    }

    // Open and read a file, then forward to LoadTextureFromMemory()
    bool LoadTextureFromFile(const char* file_name, GLuint* out_texture, int* out_width, int* out_height)
    {
        FILE* f = fopen(file_name, "rb");
        if (f == NULL)
            return false;
        fseek(f, 0, SEEK_END);
        size_t file_size = (size_t)ftell(f);
        if (file_size == -1)
            return false;
        fseek(f, 0, SEEK_SET);
        void* file_data = IM_ALLOC(file_size);
        fread(file_data, 1, file_size, f);
        bool ret = LoadTextureFromMemory(file_data, file_size, out_texture, out_width, out_height);
        IM_FREE(file_data);
        return ret;
    }

    std::string GetExecutablePath() {
    char buffer[MAX_PATH];
    HMODULE hModule = GetModuleHandle(NULL); // Get handle of current module
    if (hModule != NULL) {
        // Get the full path of the executable
        GetModuleFileName(hModule, buffer, sizeof(buffer));
        return std::string(buffer); // Return as std::string
    }
    return ""; // Return an empty string if failed
    }

    std::string GetExecutableFolderPath() {
        char buffer[MAX_PATH];
        HMODULE hModule = GetModuleHandle(NULL); // Get handle of current module
        if (hModule != NULL) {
            // Get the full path of the executable
            GetModuleFileName(hModule, buffer, sizeof(buffer));

            // Convert to std::string for easier manipulation
            std::string exePath(buffer);

            // Find the last occurrence of the path separator
            std::string::size_type pos = exePath.find_last_of("\\/");
            if (pos != std::string::npos) {
                // Return the substring up to the last separator
                return exePath.substr(0, pos);
            }
        }
        return ""; // Return an empty string if failed
    }

    void AddToStartup() {
        std::string progPath = GetExecutablePath(); // Update this path
        HKEY hkey;
    
        // Create or open the registry key
        long createStatus = RegCreateKey(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
        if (createStatus == ERROR_SUCCESS) {
            // Set the value in the registry
            long status = RegSetValueEx(hkey, "DualSenseY", 0, REG_SZ, (BYTE*)progPath.c_str(), (progPath.size() + 1) * sizeof(wchar_t));
            RegCloseKey(hkey); // Close the key
        }
    }

    void RemoveFromStartup() {
        HKEY hkey;

        // Open the registry key
        long openStatus = RegOpenKey(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
        if (openStatus == ERROR_SUCCESS) {
            // Delete the specified value from the registry
            long deleteStatus = RegDeleteValue(hkey, "DualSenseY");
            RegCloseKey(hkey); // Close the key

            if (deleteStatus == ERROR_SUCCESS) {
                std::cout << "Successfully removed from startup: " << "DualSenseY" << std::endl;
            } else {
                std::cerr << "Failed to remove registry value. It may not exist." << std::endl;
            }
        } else {
            std::cerr << "Failed to open registry key." << std::endl;
        }
    }

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
        std::strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S %p", nowTm);
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

    int scaleFloatToInt(float input_float) {
        const float max_float = 0.0500000f;
        const int max_int = 255;
    
        // Ensure input is within the valid range
        if (input_float < 0.0f) {
            input_float = 0.0f;
        } else if (input_float > max_float) {
            input_float = max_float;
        }

        // Scale the float to an integer in the range [0, 255]
        int scaled_int = static_cast<int>((input_float / max_float) * max_int);
    
        return scaled_int;
    }

    bool RestartHIDDevice(const std::string& deviceInstanceID) {
        // Get a handle to the device information set for all devices
        HDEVINFO hDevInfo = SetupDiGetClassDevsA(
            nullptr, // All classes
            nullptr, // Enumerator
            nullptr, // Parent window
            DIGCF_ALLCLASSES | DIGCF_PRESENT // Present devices
        );

        if (hDevInfo == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to get device information set." << std::endl;
            return false;
        }

        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        // Enumerate through devices
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i) {
            char buffer[512];
            if (SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, buffer, sizeof(buffer), nullptr)) {
                std::string currentID(buffer);

                if (currentID == deviceInstanceID) {
                    SP_PROPCHANGE_PARAMS params = {};
                    params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
                    params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
                    params.StateChange = DICS_DISABLE;
                    params.Scope = DICS_FLAG_GLOBAL;
                    params.HwProfile = 0;

                    // Disable the device
                    if (!SetupDiSetClassInstallParams(hDevInfo, &devInfoData, &params.ClassInstallHeader, sizeof(params))) {
                        std::cerr << "Failed to set class install params for disabling." << std::endl;
                        SetupDiDestroyDeviceInfoList(hDevInfo);
                        return false;
                    }

                    if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, hDevInfo, &devInfoData)) {
                        std::cerr << "Failed to disable the device." << std::endl;
                        SetupDiDestroyDeviceInfoList(hDevInfo);
                        return false;
                    }

                    // Enable the device
                    params.StateChange = DICS_ENABLE;
                    if (!SetupDiSetClassInstallParams(hDevInfo, &devInfoData, &params.ClassInstallHeader, sizeof(params))) {
                        std::cerr << "Failed to set class install params for enabling." << std::endl;
                        SetupDiDestroyDeviceInfoList(hDevInfo);
                        return false;
                    }

                    if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, hDevInfo, &devInfoData)) {
                        std::cerr << "Failed to enable the device." << std::endl;
                        SetupDiDestroyDeviceInfoList(hDevInfo);
                        return false;
                    }

                    SetupDiDestroyDeviceInfoList(hDevInfo);
                    return true;
                }
            }
        }

        std::cerr << "Device not found." << std::endl;
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return false;
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
