#include "Settings.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <commdlg.h>
#include <fstream> 

namespace Config {
    class AppConfig {
        std::vector<std::pair<std::string, std::string>> DefaultConfigs;

        nlohmann::json to_json() const {
            nlohmann::json j;
            j["DefaultConfigs"] = DefaultConfigs;
                  
            return j;
        }
    };

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

    static Settings ReadFromFile() {

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
}

