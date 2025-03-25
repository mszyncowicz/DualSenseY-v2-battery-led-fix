#include "Strings.h"
#include "Config.h"
#include <fstream>
#include <iostream>
#include <exception>

// Define the languages vector
std::vector<std::string> languages = {"en", "pl", "it", "es", "pt", "fr", "ja", "zh", "ko", "ru", "tr", "vi"};

// Constructor implementation with default values
Strings::Strings() {
    #define X(name, default) name = default;
    STRING_MEMBERS
    #undef X
    _Touchpad = "Touchpad";
}

// to_json method implementation
json Strings::to_json() const {
    json j;
    #define X(name, default) j[#name] = name;
    STRING_MEMBERS
    #undef X
    j["Touchpad"] = _Touchpad;
    return j;
}

// from_json method implementation
Strings Strings::from_json(const json& j) {
    Strings s; // Calls constructor to initialize with defaults
    #define X(name, default) if (j.contains(#name)) j.at(#name).get_to(s.name);
    STRING_MEMBERS
    #undef X
    if (j.contains("Touchpad")) j.at("Touchpad").get_to(s._Touchpad);
    return s;
}

// ReadStringsFromFile function implementation
Strings ReadStringsFromFile(const std::string& language) {
    Strings strings; // Default constructor initializes with defaults
    std::string fileLocation = Config::GetExecutableFolderPath() + "\\localizations\\" + language + ".json";
    
    std::ifstream file(fileLocation);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << fileLocation << std::endl;
        return strings;
    }

    try {
        json j;
        file >> j;
        strings = Strings::from_json(j);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }
    file.close();
    return strings;
}
