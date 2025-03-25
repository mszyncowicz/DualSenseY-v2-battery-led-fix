#ifndef UDP_H
#define UDP_H

#include "Settings.h"
#include "MyUtils.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

enum ConnectionType {
    USB,
    BLUETOOTH,
    DONGLE
};

enum DeviceType {
    DUALSENSE,
    DUALSENSE_EDGE,
    DUALSHOCK_V1,
    DUALSHOCK_V2,
    DUALSHOCK_DONGLE,
    PS_VR2_LeftController,
    PS_VR2_RightController,
    ACCESS_CONTROLLER
};

struct Device {
    int Index;
    std::string MacAddress;
    DeviceType DeviceType;
    ConnectionType ConnectionType;
    int BatteryLevel;
    bool IsSupportAT;
    bool IsSupportLightBar;
    bool IsSupportPlayerLED;
    bool IsSupportMicLED;

    nlohmann::json to_json() const;
};

struct UDPResponse {
    std::string Status;
    std::string TimeReceived;
    bool isControllerConnected;
    int BatteryLevel;
    std::vector<Device> Devices;

    nlohmann::json to_json() const;
};

class UDP {
public:
    bool isActive = false;
    int Battery;
    Settings thisSettings;
    std::string MacAddress;

    std::chrono::high_resolution_clock::time_point LastTime = std::chrono::high_resolution_clock::now();

    UDP(int port = 6969);
    ~UDP();

    void Connect();
    void Listen();
    void Dispose();
    static void StartFakeDSXProcess();

    int getParameterValue(const nlohmann::json& param);
    bool getBoolValue(const nlohmann::json& param);
    float getFloatValue(const nlohmann::json& param);
    std::string getParameterValueAsString(const nlohmann::json& param);

private:
    SOCKET clientSocket = INVALID_SOCKET;
    bool serverOn;
    bool newPacket = false;
    bool unavailable;
};

#endif // UDP_H
