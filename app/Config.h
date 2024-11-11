#include "Settings.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <commdlg.h>
#include <fstream> 

namespace Config {
    class AppConfig {
    public:
        bool ElevateOnStartup = false;
        bool HideToTray = false;
        bool ShowWindow = true;
        bool HideWindowOnStartup = false;
        bool RunWithWindows = false;

        nlohmann::json to_json() const {
            nlohmann::json j;
            j["ElevateOnStartup"] = ElevateOnStartup;
            j["HideToTray"] = HideToTray;
            //j["ShowWindow"] = ShowWindow;
            j["HideWindowOnStartup"] = HideWindowOnStartup;
            j["RunWithWindows"] = RunWithWindows;

            return j;
        }

        static AppConfig from_json(const nlohmann::json& j) {
            AppConfig appconfig;

            if (j.contains("ElevateOnStartup"))       j.at("ElevateOnStartup").get_to(appconfig.ElevateOnStartup);
            if (j.contains("HideToTray"))       j.at("HideToTray").get_to(appconfig.HideToTray);
            //if (j.contains("ShowWindow"))       j.at("ShowWindow").get_to(appconfig.ShowWindow);
            if (j.contains("HideWindowOnStartup"))       j.at("HideWindowOnStartup").get_to(appconfig.HideWindowOnStartup);
            if (j.contains("RunWithWindows"))       j.at("RunWithWindows").get_to(appconfig.RunWithWindows);

            return appconfig;
        }
    };

    static void WriteAppConfigToFile(AppConfig &config) {
        nlohmann::json j = config.to_json();

        std::string fileLocation = MyUtils::GetExecutableFolderPath() + "\\AppConfig.txt";
        ofstream file(fileLocation);
        if (file.is_open()) {
            file << j.dump(4); 
            file.close();
        }
    }

    static AppConfig ReadAppConfigFromFile() {
        AppConfig appconfig;
        nlohmann::json j;
        std::string fileLocation = MyUtils::GetExecutableFolderPath() + "\\AppConfig.txt";
        ifstream file(fileLocation);

        if (file.is_open()) {
            j = j.parse(file); 
            file.close();
            appconfig = appconfig.from_json(j);
            return appconfig;
        }

        return appconfig;
    }

    static void WriteToFile(Settings &settings) {
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
        wstring ws( ofn.lpstrFile ); 
        string filename = string( ws.begin(), ws.end() );
        ofstream file(filename);
        if (file.is_open()) {
            file << j.dump(4); 
            file.close();
        }
    }

    static Settings ReadFromFile(std::string filename) {
        Settings settings;
        nlohmann::json j;
        ifstream file(filename);

        if (file.is_open()) {
            j = j.parse(file); 
            file.close();
            settings = settings.from_json(j);
            return settings;
        }

        return settings;
    }

    static std::string GetConfigPath() {
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
        wstring ws( ofn.lpstrFile ); 
        string filename = string( ws.begin(), ws.end() );

        return filename;
    }
}

