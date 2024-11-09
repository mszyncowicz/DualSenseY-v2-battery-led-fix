#include "Settings.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <commdlg.h>
#include <fstream> 

namespace Config {
    class AppConfig {
    public:
        bool ElevateOnStartup = false;

        nlohmann::json to_json() const {
            nlohmann::json j;
            j["ElevateOnStartup"] = ElevateOnStartup;
        
            return j;
        }

        static AppConfig from_json(const nlohmann::json& j) {
            AppConfig appconfig;

            if (j.contains("ElevateOnStartup"))       j.at("ElevateOnStartup").get_to(appconfig.ElevateOnStartup);
            
            return appconfig;
        }
    };

    static void WriteAppConfigToFile(AppConfig &config) {
        nlohmann::json j = config.to_json();

        ofstream file("AppConfig.txt");
        if (file.is_open()) {
            file << j.dump(4); 
            file.close();
        }
    }

    static AppConfig ReadAppConfigFromFile() {
        AppConfig appconfig;
        nlohmann::json j;
        ifstream file("AppConfig.txt");

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

