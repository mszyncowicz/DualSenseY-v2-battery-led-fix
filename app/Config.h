#ifndef CONFIG_H
#define CONFIG_H

#include "Settings.h"
#include <commdlg.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

namespace Config
{
    std::string GetExecutableFolderPath();

class AppConfig
{
public:
    bool ElevateOnStartup = false;
    bool HideToTray = false;
    bool ShowWindow = true;
    bool HideWindowOnStartup = false;
    bool RunWithWindows = false;
    bool ShowConsole = false;
    std::string Language = "en";
    std::string DefaultStyleFilepath = "";
    bool SkipVersionCheck = false;
    bool DisconnectAllBTControllersOnExit = false;

    nlohmann::json to_json() const;
    static AppConfig from_json(const nlohmann::json &j);
};

void WriteAppConfigToFile(AppConfig &config);
AppConfig ReadAppConfigFromFile();
void WriteToFile(Settings &settings);
Settings ReadFromFile(std::string filename);
std::string GetConfigPath();
std::string GetStylePath();
} // namespace Config

#endif // CONFIG_H
