#include "Config.h"

namespace Config {
    std::string GetExecutableFolderPath()
    {
        char buffer[MAX_PATH];
        HMODULE hModule = GetModuleHandle(NULL); // Get handle of current module
        if (hModule != NULL)
        {
            // Get the full path of the executable
            GetModuleFileName(hModule, buffer, sizeof(buffer));

            // Convert to std::string for easier manipulation
            std::string exePath(buffer);

            // Find the last occurrence of the path separator
            std::string::size_type pos = exePath.find_last_of("\\/");
            if (pos != std::string::npos)
            {
                // Return the substring up to the last separator
                return exePath.substr(0, pos);
            }
        }
        return ""; // Return an empty string if failed
    }


    nlohmann::json AppConfig::to_json() const {
        nlohmann::json j;
        j["DisconnectAllBTControllersOnExit"] = DisconnectAllBTControllersOnExit;
        j["DefaultStyleFilepath"] = DefaultStyleFilepath;
        j["ElevateOnStartup"] = ElevateOnStartup;
        j["HideToTray"] = HideToTray;
        //j["ShowWindow"] = ShowWindow;
        j["HideWindowOnStartup"] = HideWindowOnStartup;
        j["RunWithWindows"] = RunWithWindows;
        j["ShowConsole"] = ShowConsole;
        j["Language"] = Language;
        j["SkipVersionCheck"] = SkipVersionCheck;
        j["ShowNotifications"] = ShowNotifications;

        return j;
    }

    AppConfig AppConfig::from_json(const nlohmann::json& j) {
        AppConfig appconfig;

        if (j.contains("DisconnectAllBTControllersOnExit")) j.at("DisconnectAllBTControllersOnExit").get_to(appconfig.DisconnectAllBTControllersOnExit);
        if (j.contains("SkipVersionCheck")) j.at("SkipVersionCheck").get_to(appconfig.SkipVersionCheck);
        if (j.contains("ElevateOnStartup")) j.at("ElevateOnStartup").get_to(appconfig.ElevateOnStartup);
        if (j.contains("HideToTray")) j.at("HideToTray").get_to(appconfig.HideToTray);
        //if (j.contains("ShowWindow")) j.at("ShowWindow").get_to(appconfig.ShowWindow);
        if (j.contains("HideWindowOnStartup")) j.at("HideWindowOnStartup").get_to(appconfig.HideWindowOnStartup);
        if (j.contains("RunWithWindows")) j.at("RunWithWindows").get_to(appconfig.RunWithWindows);
        if (j.contains("ShowConsole")) j.at("ShowConsole").get_to(appconfig.ShowConsole);
        if (j.contains("Language")) j.at("Language").get_to(appconfig.Language);
        if (j.contains("DefaultStyleFilepath")) j.at("DefaultStyleFilepath").get_to(appconfig.DefaultStyleFilepath);
        if (j.contains("ShowNotifications")) j.at("ShowNotifications").get_to(appconfig.ShowNotifications);

        return appconfig;
    }

    void WriteAppConfigToFile(AppConfig& config) {
        nlohmann::json j = config.to_json();
        std::string fileLocation = GetExecutableFolderPath() + "\\AppConfig.txt";
        std::ofstream file(fileLocation);
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
        }
    }

    AppConfig ReadAppConfigFromFile() {
        AppConfig appconfig;
        nlohmann::json j;
        std::string fileLocation = GetExecutableFolderPath() + "\\AppConfig.txt";
        std::ifstream file(fileLocation);

        if (file.is_open()) {
            j = nlohmann::json::parse(file);
            file.close();
            appconfig = AppConfig::from_json(j);
            return appconfig;
        }

        return appconfig;
    }

    void WriteToFile(Settings& settings) {
        nlohmann::json j = settings.to_json();
        OPENFILENAMEW ofn;
        char szFileName[MAX_PATH] = "";

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = (LPCWSTR)L"DualSenseY Config (*.dscf)\0*.dscf";
        ofn.lpstrFile = (LPWSTR)szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = (LPCWSTR)L"dscf";

        GetSaveFileNameW(&ofn);
        std::wstring ws(ofn.lpstrFile);
        std::string filename(ws.begin(), ws.end());
        std::ofstream file(filename);
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
        }
    }

    Settings ReadFromFile(std::string filename) {
        Settings settings;
        nlohmann::json j;
        std::ifstream file(filename);

        if (file.is_open()) {
            j = nlohmann::json::parse(file);
            file.close();
            settings = settings.from_json(j);
            return settings;
        }

        return settings;
    }

    std::string GetConfigPath() {
        OPENFILENAMEW ofn;
        char szFileName[MAX_PATH] = "";

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = (LPCWSTR)L"DualSenseY Config (*.dscf)\0*.dscf";
        ofn.lpstrFile = (LPWSTR)szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = (LPCWSTR)L"dscf";

        GetOpenFileNameW(&ofn);
        std::wstring ws(ofn.lpstrFile);
        std::string filename(ws.begin(), ws.end());

        return filename;
    }

    std::string GetStylePath() {
        OPENFILENAMEW ofn;
        char szFileName[MAX_PATH] = "";

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = (LPCWSTR)L"DualSenseY Style (*.dsst)\0*.dsst";
        ofn.lpstrFile = (LPWSTR)szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = (LPCWSTR)L"dsst";

        GetOpenFileNameW(&ofn);
        std::wstring ws(ofn.lpstrFile);
        std::string filename(ws.begin(), ws.end());

        return filename;
    }
}
