#include "UDP.h"
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <ratio>
#include <direct.h>
#include <filesystem>
#pragma comment(lib, "Ws2_32.lib")

nlohmann::json Device::to_json() const {
    nlohmann::json j;
    j["Index"] = Index;
    j["MacAddress"] = MacAddress;
    j["DeviceType"] = DeviceType;
    j["ConnectionType"] = ConnectionType;
    j["BatteryLevel"] = BatteryLevel;
    j["IsSupportAT"] = IsSupportAT;
    j["IsSupportLightBar"] = IsSupportLightBar;
    j["IsSupportPlayerLED"] = IsSupportPlayerLED;
    j["IsSupportMicLED"] = IsSupportMicLED;
    return j;
}

nlohmann::json UDPResponse::to_json() const {
    nlohmann::json j;
    j["Status"] = Status;
    j["TimeReceived"] = TimeReceived;
    j["isControllerConnected"] = isControllerConnected;
    j["BatteryLevel"] = BatteryLevel;
    for (const auto& device : Devices) {
        j["Devices"].push_back(device.to_json());
    }
    return j;
}

UDP::UDP(int port) : serverOn(false), Battery(100), unavailable(false) {
    std::error_code ec;
    if (!std::filesystem::create_directories("C:/Temp/DualSenseX", ec) && ec) {
        std::cerr << "Directory creation failed: " << ec.message() << std::endl;
    }

    std::ofstream portFile("C:/Temp/DualSenseX/DualSenseX_PortNumber.txt", std::ios::trunc);
    if (!portFile.is_open()) {
        std::cerr << "Failed to open port file" << std::endl;
    } else {
        portFile << port;
        if (!portFile.good()) {
            std::cerr << "Failed to write to port file" << std::endl;
        }
    }

    Connect();
}

UDP::~UDP() {
    Dispose();
}

void UDP::Connect() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << "\n";
        return;
    }

    clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return;
    }

    int portNumber = 6969;
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(portNumber);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        std::cerr << "Bind failed. " << err << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return;
    } else {
        std::cout << "Server started at: " << portNumber << std::endl;
    }

    serverOn = true;
    std::thread(&UDP::Listen, this).detach();
}

int UDP::getParameterValue(const nlohmann::json& param) {
    if (param.is_null()) return 0;
    else if (param.is_number()) return param.get<int>();
    else return 0;
}

bool UDP::getBoolValue(const nlohmann::json& param) {
    if (param.is_null()) return false;
    else if (param.is_boolean()) return param.get<bool>();
    else return false;
}

float UDP::getFloatValue(const nlohmann::json& param) {
    if (param.is_null()) return 0.0f;
    else if (param.is_number_float() || param.is_number()) return param.get<float>();
    else if (param.is_string()) {
        try {
            return std::stof(param.get<std::string>());
        } catch (const std::invalid_argument&) {
            return 0.0f;
        } catch (const std::out_of_range&) {
            return 0.0f;
        }
    }
    else return 0.0f;
}

std::string UDP::getParameterValueAsString(const nlohmann::json& param) {
    if (param.is_null()) return "";
    else if (param.is_string()) return param.get<std::string>();
    else if (param.is_boolean()) return param.get<bool>() ? "true" : "false";
    else if (param.is_number()) return std::to_string(param.get<double>());
    else return "";
}

void UDP::Listen() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    sockaddr_in clientAddr{};
    int clientAddrSize = sizeof(clientAddr);
    char recvBuffer[1024];

    while (serverOn) {
        int bytesReceived = recvfrom(clientSocket, recvBuffer, sizeof(recvBuffer), 0, (SOCKADDR*)&clientAddr, &clientAddrSize);
        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "recvfrom failed.\n";
            continue;
        }

        LastTime = std::chrono::high_resolution_clock::now();
        std::string packetString(recvBuffer, bytesReceived);
        newPacket = true;

        Device dev{1, MacAddress, DeviceType::DUALSENSE, ConnectionType::USB, Battery, true, true, true, true};
        UDPResponse response{"DSX Received UDP Instructions", MyUtils::currentDateTime(), true, Battery, {dev}};
        nlohmann::json j = response.to_json();
        std::string responseStr = j.dump(4);
        std::cout << responseStr << std::endl;
        sendto(clientSocket, responseStr.c_str(), responseStr.size(), 0, (SOCKADDR*)&clientAddr, clientAddrSize);

        isActive = true;
        nlohmann::json data = nlohmann::json::parse(packetString);

        for (auto& instr : data["instructions"]) {
            uint8_t triggers[11];
            Trigger::TriggerMode mode = Trigger::Off;
            AudioEdit edit;
            AudioPlay play;

            int type = getParameterValue(instr["type"]);
            int intParam0 = getParameterValue(instr["parameters"][0]);
            int intParam1 = getParameterValue(instr["parameters"][1]);
            int intParam2 = getParameterValue(instr["parameters"][2]);
            int intParam3 = getParameterValue(instr["parameters"][3]);
            int intParam4 = getParameterValue(instr["parameters"][4]);
            int intParam5 = getParameterValue(instr["parameters"][5]);
            int intParam6 = getParameterValue(instr["parameters"][6]);
            int intParam7 = getParameterValue(instr["parameters"][7]);
            int intParam8 = getParameterValue(instr["parameters"][8]);
            int intParam9 = getParameterValue(instr["parameters"][9]);
            int intParam10 = getParameterValue(instr["parameters"][10]);
            int intParam11 = getParameterValue(instr["parameters"][11]);
            int intParam12 = getParameterValue(instr["parameters"][12]);
            int intParam13 = getParameterValue(instr["parameters"][13]);

            switch (type) {
            case 1: {
                uint8_t start = 0, end = 0, snapForce = 0, force = 0, firstFoot = 0, secondFoot = 0, frequency = 0;
                switch (intParam2) {
                case 0: // Normal
                {
                    mode = Trigger::Rigid_B;
                    triggers[0] = 0; triggers[1] = 0; triggers[2] = 0; triggers[3] = 0;
                    triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 1: // GameCube
                {
                    mode = Trigger::Pulse;
                    triggers[0] = 144; triggers[1] = 160; triggers[2] = 255; triggers[3] = 0;
                    triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 2: // VerySoft
                {
                    mode = Trigger::Pulse;
                    triggers[0] = 128; triggers[1] = 160; triggers[2] = 255; triggers[3] = 0;
                    triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 3: // Soft
                {
                    mode = Trigger::Rigid_A;
                    triggers[0] = 69; triggers[1] = 160; triggers[2] = 255; triggers[3] = 0;
                    triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 4: // Hard
                {
                    mode = Trigger::Rigid_A;
                    triggers[0] = 32; triggers[1] = 160; triggers[2] = 255; triggers[3] = 255;
                    triggers[4] = 255; triggers[5] = 255; triggers[6] = 255; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 5: // VeryHard
                {
                    mode = Trigger::Rigid_A;
                    triggers[0] = 16; triggers[1] = 160; triggers[2] = 255; triggers[3] = 255;
                    triggers[4] = 255; triggers[5] = 255; triggers[6] = 255; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 6: // Hardest
                {
                    mode = Trigger::Pulse;
                    triggers[0] = 0; triggers[1] = 255; triggers[2] = 255; triggers[3] = 255;
                    triggers[4] = 255; triggers[5] = 255; triggers[6] = 255; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 7: // Rigid
                {
                    mode = Trigger::Rigid;
                    triggers[0] = 0; triggers[1] = 255; triggers[2] = 0; triggers[3] = 0;
                    triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 8: // VibrateTrigger
                {
                    mode = Trigger::Pulse_AB;
                    triggers[0] = 37; triggers[1] = 35; triggers[2] = 6; triggers[3] = 39;
                    triggers[4] = 33; triggers[5] = 35; triggers[6] = 34; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 9: // Choppy
                {
                    mode = Trigger::Rigid_A;
                    triggers[0] = 2; triggers[1] = 39; triggers[2] = 33; triggers[3] = 39;
                    triggers[4] = 38; triggers[5] = 2; triggers[6] = 0; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 10: // Medium
                {
                    mode = Trigger::Pulse_A;
                    triggers[0] = 2; triggers[1] = 35; triggers[2] = 1; triggers[3] = 6;
                    triggers[4] = 6; triggers[5] = 1; triggers[6] = 33; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 11: // VibrateTriggerPulse
                {
                    mode = Trigger::Pulse_AB;
                    triggers[0] = 37; triggers[1] = 35; triggers[2] = 6; triggers[3] = 39;
                    triggers[4] = 33; triggers[5] = 35; triggers[6] = 34; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 12: // CustomTriggerValue
                {
                    switch (intParam3) {
                    case 0: mode = Trigger::Off; break;
                    case 1: mode = Trigger::Rigid; break;
                    case 2: mode = Trigger::Rigid_A; break;
                    case 3: mode = Trigger::Rigid_B; break;
                    case 4: mode = Trigger::Rigid_AB; break;
                    case 5: mode = Trigger::Pulse; break;
                    case 6: mode = Trigger::Pulse_A; break;
                    case 7: mode = Trigger::Pulse_B; break;
                    case 8: mode = Trigger::Pulse_AB; break;
                    case 9: mode = Trigger::Pulse_B; break;
                    case 10: mode = Trigger::Pulse_B2; break;
                    case 11: mode = Trigger::Pulse_B; break;
                    case 12: mode = Trigger::Pulse_B2; break;
                    case 13: mode = Trigger::Pulse_AB; break;
                    case 14: mode = Trigger::Pulse_AB; break;
                    case 15: mode = Trigger::Pulse_AB; break;
                    case 16: mode = Trigger::Pulse_AB; break;
                    default:
                        std::cout << "Unknown trigger: " << intParam2 << std::endl;
                        for (auto& a : instr["parameters"]) std::cout << " " << a << " ";
                        std::cout << std::endl;
                        break;
                    }
                    triggers[0] = intParam4; triggers[1] = intParam5; triggers[2] = intParam6;
                    triggers[3] = intParam7; triggers[4] = intParam8; triggers[5] = intParam9;
                    triggers[6] = intParam10; triggers[7] = 0; triggers[8] = 0;
                    triggers[9] = intParam11; triggers[10] = 0;
                    break;
                }
                case 13: // Resistance
                {
                    mode = Trigger::Rigid_B;
                    start = intParam3; force = intParam4;
                    if (start <= 9 && force <= 8 && force > 0) {
                        uint8_t b = static_cast<uint8_t>((force - 1) & 7);
                        uint32_t num = 0;
                        uint16_t num2 = 0;
                        for (int i = static_cast<int>(start); i < 10; ++i) {
                            num |= (static_cast<uint32_t>(b) << (3 * i));
                            num2 |= (1 << i);
                        }
                        triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                        triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                        triggers[2] = static_cast<uint8_t>(num & 0xFF);
                        triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                        triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                        triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                        triggers[6] = 0; triggers[7] = 0; triggers[8] = 0;
                        triggers[9] = 0; triggers[10] = 0;
                    }
                    break;
                }
                case 14: // Bow
                {
                    mode = Trigger::Pulse_A;
                    start = intParam3; end = intParam4; force = intParam5; snapForce = intParam6;
                    if (start <= 8 && end <= 8 && start < end && force <= 8 && snapForce <= 8 && end > 0 && force > 0 && snapForce > 0) {
                        uint16_t num = static_cast<uint16_t>((1 << start) | (1 << end));
                        uint32_t num2 = static_cast<uint32_t>(((force - 1) & 7) | (((snapForce - 1) & 7) << 3));
                        triggers[0] = static_cast<uint8_t>(num & 0xFF);
                        triggers[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
                        triggers[2] = static_cast<uint8_t>(num2 & 0xFF);
                        triggers[3] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                        triggers[4] = 0; triggers[5] = 0; triggers[6] = 0;
                        triggers[7] = 0; triggers[8] = 0; triggers[9] = 0;
                        triggers[10] = 0;
                    }
                    break;
                }
                case 15: // Galloping
                {
                    mode = Trigger::Pulse_A2;
                    start = intParam3; end = intParam4; firstFoot = intParam5; secondFoot = intParam6; frequency = intParam7;
                    if (start <= 8 && end <= 9 && start < end && secondFoot <= 7 && firstFoot <= 6 && firstFoot < secondFoot && frequency > 0) {
                        uint16_t num = static_cast<uint16_t>((1 << start) | (1 << end));
                        uint32_t num2 = static_cast<uint32_t>((secondFoot & 7) | ((firstFoot & 7) << 3));
                        triggers[0] = static_cast<uint8_t>(num & 0xFF);
                        triggers[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
                        triggers[2] = static_cast<uint8_t>(num2 & 0xFF);
                        triggers[3] = frequency;
                        triggers[4] = 0; triggers[5] = 0; triggers[6] = 0;
                        triggers[7] = 0; triggers[8] = 0; triggers[9] = 0;
                    }
                    break;
                }
                case 16: // SemiAutomaticGun
                {
                    mode = Trigger::Rigid_AB;
                    start = intParam3; end = intParam4; force = intParam5;
                    if (start <= 7 && start >= 2 && end <= 8 && end > start && force <= 8 && force > 0) {
                        uint16_t num = static_cast<uint16_t>((1 << start) | (1 << end));
                        triggers[0] = static_cast<uint8_t>(num & 0xFF);
                        triggers[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
                        triggers[2] = force - 1;
                        triggers[3] = 0; triggers[4] = 0; triggers[5] = 0;
                        triggers[6] = 0; triggers[7] = 0; triggers[8] = 0;
                        triggers[9] = 0; triggers[10] = 0;
                    }
                    break;
                }
                case 17: // AutomaticGun
                {
                    mode = Trigger::Pulse_B2;
                    start = intParam3; uint8_t strength = intParam4; frequency = intParam5;
                    if (start <= 9 && strength <= 8 && strength > 0 && frequency > 0) {
                        uint8_t b = (strength - 1) & 7;
                        uint32_t num = 0; uint16_t num2 = 0;
                        for (int i = static_cast<int>(start); i < 10; i++) {
                            num |= static_cast<uint32_t>(b) << (3 * i);
                            num2 |= static_cast<uint16_t>(1 << i);
                        }
                        triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                        triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                        triggers[2] = static_cast<uint8_t>(num & 0xFF);
                        triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                        triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                        triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                        triggers[6] = 0; triggers[7] = 0; triggers[8] = frequency;
                        triggers[9] = 0; triggers[10] = 0;
                    }
                    break;
                }
                case 18: // Machine
                {
                    mode = Trigger::Pulse_AB;
                    start = intParam3; end = intParam4; uint8_t strengthA = intParam5; uint8_t strengthB = intParam6; frequency = intParam7; uint8_t period = intParam8;
                    if (start <= 8 && end <= 9 && end > start && strengthA <= 7 && strengthB <= 7 && frequency > 0) {
                        uint16_t num = static_cast<uint16_t>((1 << start) | (1 << end));
                        uint32_t num2 = static_cast<uint32_t>((strengthA & 7) | ((strengthB & 7) << 3));
                        triggers[0] = static_cast<uint8_t>(num & 0xFF);
                        triggers[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
                        triggers[2] = static_cast<uint8_t>(num2 & 0xFF);
                        triggers[3] = frequency; triggers[4] = period;
                        triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                        triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    }
                    break;
                }
                case 19: // VIBRATE_TRIGGER_10Hz
                {
                    mode = Trigger::Pulse_B;
                    triggers[0] = 10; triggers[1] = 255; triggers[2] = 40; triggers[3] = 0;
                    triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 20: // OFF
                {
                    mode = Trigger::Rigid_B;
                    triggers[0] = 0; triggers[1] = 0; triggers[2] = 0; triggers[3] = 0;
                    triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                    triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    break;
                }
                case 21: // FEEDBACK
                {
                    mode = Trigger::Rigid_A;
                    uint8_t position = intParam3; uint8_t strength = intParam4;
                    if (position <= 9 && strength <= 8) {
                        if (strength > 0) {
                            uint8_t b = (strength - 1) & 7;
                            uint32_t num = 0; uint16_t num2 = 0;
                            for (int i = position; i < 10; i++) {
                                num |= static_cast<uint32_t>(b) << (3 * i);
                                num2 |= static_cast<uint16_t>(1 << i);
                            }
                            triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                            triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                            triggers[2] = static_cast<uint8_t>(num & 0xFF);
                            triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                            triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                            triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                            triggers[6] = 0; triggers[7] = 0; triggers[8] = 0;
                            triggers[9] = 0; triggers[10] = 0;
                        }
                        else {
                            mode = Trigger::Rigid_B;
                            triggers[0] = 0; triggers[1] = 0; triggers[2] = 0; triggers[3] = 0;
                            triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                            triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                        }
                    }
                    break;
                }
                case 22: // WEAPON
                {
                    mode = Trigger::Rigid_AB;
                    uint8_t startPosition = intParam3; uint8_t endPosition = intParam4; uint8_t strength = intParam5;
                    if (startPosition <= 7 && startPosition >= 2 && endPosition <= 8 && endPosition > startPosition && strength <= 8) {
                        if (strength > 0) {
                            uint16_t num = static_cast<uint16_t>(1 << startPosition | 1 << endPosition);
                            triggers[0] = static_cast<uint8_t>(num & 0xFF);
                            triggers[1] = static_cast<uint8_t>((num >> 8) & 0xFF);
                            triggers[2] = strength - 1;
                            triggers[3] = 0; triggers[4] = 0; triggers[5] = 0;
                            triggers[6] = 0; triggers[7] = 0; triggers[8] = 0;
                            triggers[9] = 0; triggers[10] = 0;
                        }
                        else {
                            mode = Trigger::Rigid_B;
                            triggers[0] = 0; triggers[1] = 0; triggers[2] = 0; triggers[3] = 0;
                            triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                            triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                        }
                    }
                    break;
                }
                case 23: // VIBRATION
                {
                    mode = Trigger::Vibration;
                    uint8_t position = intParam3; uint8_t amplitude = intParam4; frequency = intParam5;
                    if (position <= 9 && amplitude <= 8 && amplitude > 0 && frequency > 0) {
                        uint8_t b = (amplitude - 1) & 7;
                        uint32_t num = 0; uint16_t num2 = 0;
                        for (int i = position; i < 10; i++) {
                            num |= static_cast<uint32_t>(b) << (3 * i);
                            num2 |= static_cast<uint16_t>(1 << i);
                        }
                        if (intParam2 == 2) {
                            triggers[1] = static_cast<uint8_t>(num2 & 0xFF);
                            triggers[2] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                            triggers[3] = static_cast<uint8_t>(num & 0xFF);
                            triggers[4] = static_cast<uint8_t>((num >> 8) & 0xFF);
                            triggers[5] = static_cast<uint8_t>((num >> 16) & 0xFF);
                            triggers[6] = static_cast<uint8_t>((num >> 24) & 0xFF);
                            triggers[7] = 0; triggers[8] = 0; triggers[9] = frequency;
                            triggers[10] = 0; triggers[0] = 0;
                        }
                        else {
                            triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                            triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                            triggers[2] = static_cast<uint8_t>(num & 0xFF);
                            triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                            triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                            triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                            triggers[6] = 0; triggers[7] = 0; triggers[8] = frequency;
                            triggers[9] = 0; triggers[10] = 0;
                        }
                    }
                    else {
                        mode = Trigger::Rigid_B;
                        triggers[0] = 0; triggers[1] = 0; triggers[2] = 0; triggers[3] = 0;
                        triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                        triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    }
                    break;
                }
                case 24: // SLOPE_FEEDBACK
                {
                    mode = Trigger::Rigid_A;
                    uint8_t startPosition = intParam3; uint8_t endPosition = intParam4; uint8_t startStrength = intParam5; uint8_t endStrength = intParam6;
                    if (startPosition <= 8 && startPosition >= 0 && endPosition <= 9 && endPosition > startPosition && startStrength <= 8 && startStrength >= 1 && endStrength <= 8 && endStrength >= 1) {
                        uint8_t array[10] = { 0 };
                        float slope = static_cast<float>(endStrength - startStrength) / static_cast<float>(endPosition - startPosition);
                        for (int i = startPosition; i < 10; i++) {
                            if (i <= endPosition) {
                                float strengthFloat = static_cast<float>(startStrength) + slope * static_cast<float>(i - startPosition);
                                array[i] = static_cast<uint8_t>(std::round(strengthFloat));
                            }
                            else {
                                array[i] = endStrength;
                            }
                        }
                        bool anyStrength = false;
                        for (int i = 0; i < 10; i++) {
                            if (array[i] > 0) anyStrength = true;
                        }
                        if (anyStrength) {
                            uint32_t num = 0; uint16_t num2 = 0;
                            for (int i = 0; i < 10; i++) {
                                if (array[i] > 0) {
                                    uint8_t b = (array[i] - 1) & 7;
                                    num |= static_cast<uint32_t>(b) << (3 * i);
                                    num2 |= static_cast<uint16_t>(1 << i);
                                }
                            }
                            triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                            triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                            triggers[2] = static_cast<uint8_t>(num & 0xFF);
                            triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                            triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                            triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                            triggers[6] = 0; triggers[7] = 0; triggers[8] = 0;
                            triggers[9] = 0; triggers[10] = 0;
                        }
                        else {
                            mode = Trigger::Rigid_B;
                            triggers[0] = 0; triggers[1] = 0; triggers[2] = 0; triggers[3] = 0;
                            triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                            triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                        }
                    }
                    break;
                }
                case 25: // MULTIPLE_POSITION_FEEDBACK
                {
                    mode = Trigger::Rigid_A;
                    uint8_t strength[10] = {
                        static_cast<uint8_t>(intParam3), static_cast<uint8_t>(intParam4),
                        static_cast<uint8_t>(intParam5), static_cast<uint8_t>(intParam6),
                        static_cast<uint8_t>(intParam7), static_cast<uint8_t>(intParam8),
                        static_cast<uint8_t>(intParam9), static_cast<uint8_t>(intParam10),
                        static_cast<uint8_t>(intParam11), static_cast<uint8_t>(intParam12)
                    };
                    bool anyStrength = false;
                    for (int i = 0; i < 10; i++) {
                        if (strength[i] > 0) anyStrength = true;
                    }
                    if (anyStrength) {
                        uint32_t num = 0; uint16_t num2 = 0;
                        for (int i = 0; i < 10; i++) {
                            if (strength[i] > 0) {
                                uint8_t b = (strength[i] - 1) & 7;
                                num |= static_cast<uint32_t>(b) << (3 * i);
                                num2 |= static_cast<uint16_t>(1 << i);
                            }
                        }
                        triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                        triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                        triggers[2] = static_cast<uint8_t>(num & 0xFF);
                        triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                        triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                        triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                        triggers[6] = 0; triggers[7] = 0; triggers[8] = 0;
                        triggers[9] = 0; triggers[10] = 0;
                    }
                    else {
                        mode = Trigger::Rigid_B;
                        triggers[0] = 0; triggers[1] = 0; triggers[2] = 0; triggers[3] = 0;
                        triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                        triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    }
                    break;
                }
                case 26: // MULTIPLE_POSITION_VIBRATION
                {
                    mode = Trigger::Pulse_B2;
                    frequency = intParam3;
                    uint8_t amplitude[10] = {
                        static_cast<uint8_t>(intParam4), static_cast<uint8_t>(intParam5),
                        static_cast<uint8_t>(intParam6), static_cast<uint8_t>(intParam7),
                        static_cast<uint8_t>(intParam8), static_cast<uint8_t>(intParam9),
                        static_cast<uint8_t>(intParam10), static_cast<uint8_t>(intParam11),
                        static_cast<uint8_t>(intParam12), static_cast<uint8_t>(intParam13)
                    };
                    if (frequency > 0) {
                        bool anyAmplitude = false;
                        for (int i = 0; i < 10; i++) {
                            if (amplitude[i] > 0) anyAmplitude = true;
                        }
                        if (anyAmplitude) {
                            uint32_t num = 0; uint16_t num2 = 0;
                            for (int i = 0; i < 10; i++) {
                                if (amplitude[i] > 0) {
                                    uint8_t b = (amplitude[i] - 1) & 7;
                                    num |= static_cast<uint32_t>(b) << (3 * i);
                                    num2 |= static_cast<uint16_t>(1 << i);
                                }
                            }
                            triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                            triggers[1] = static_cast<uint8_t>((num2 >> 8) & 0xFF);
                            triggers[2] = static_cast<uint8_t>(num & 0xFF);
                            triggers[3] = static_cast<uint8_t>((num >> 8) & 0xFF);
                            triggers[4] = static_cast<uint8_t>((num >> 16) & 0xFF);
                            triggers[5] = static_cast<uint8_t>((num >> 24) & 0xFF);
                            triggers[6] = 0; triggers[7] = 0; triggers[8] = frequency;
                            triggers[9] = 0; triggers[10] = 0;
                        }
                        else {
                            mode = Trigger::Rigid_B;
                            triggers[0] = 0; triggers[1] = 0; triggers[2] = 0; triggers[3] = 0;
                            triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                            triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                        }
                    }
                    else {
                        mode = Trigger::Rigid_B;
                        triggers[0] = 0; triggers[1] = 0; triggers[2] = 0; triggers[3] = 0;
                        triggers[4] = 0; triggers[5] = 0; triggers[6] = 0; triggers[7] = 0;
                        triggers[8] = 0; triggers[9] = 0; triggers[10] = 0;
                    }
                    break;
                }
                }

                if (intParam1 == 1) {
                    thisSettings.ControllerInput.LeftTriggerMode = mode;
                    thisSettings.ControllerInput.LeftTriggerForces[0] = triggers[0];
                    thisSettings.ControllerInput.LeftTriggerForces[1] = triggers[1];
                    thisSettings.ControllerInput.LeftTriggerForces[2] = triggers[2];
                    thisSettings.ControllerInput.LeftTriggerForces[3] = triggers[3];
                    thisSettings.ControllerInput.LeftTriggerForces[4] = triggers[4];
                    thisSettings.ControllerInput.LeftTriggerForces[5] = triggers[5];
                    thisSettings.ControllerInput.LeftTriggerForces[6] = triggers[6];
                    thisSettings.ControllerInput.LeftTriggerForces[7] = triggers[7];
                    thisSettings.ControllerInput.LeftTriggerForces[8] = triggers[8];
                    thisSettings.ControllerInput.LeftTriggerForces[9] = triggers[9];
                    thisSettings.ControllerInput.LeftTriggerForces[10] = triggers[10];
                } else if (intParam1 == 2) {
                    thisSettings.ControllerInput.RightTriggerMode = mode;
                    thisSettings.ControllerInput.RightTriggerForces[0] = triggers[0];
                    thisSettings.ControllerInput.RightTriggerForces[1] = triggers[1];
                    thisSettings.ControllerInput.RightTriggerForces[2] = triggers[2];
                    thisSettings.ControllerInput.RightTriggerForces[3] = triggers[3];
                    thisSettings.ControllerInput.RightTriggerForces[4] = triggers[4];
                    thisSettings.ControllerInput.RightTriggerForces[5] = triggers[5];
                    thisSettings.ControllerInput.RightTriggerForces[6] = triggers[6];
                    thisSettings.ControllerInput.RightTriggerForces[7] = triggers[7];
                    thisSettings.ControllerInput.RightTriggerForces[8] = triggers[8];
                    thisSettings.ControllerInput.RightTriggerForces[9] = triggers[9];
                    thisSettings.ControllerInput.RightTriggerForces[10] = triggers[10];
                }
                break;
            }
            case 2:
                thisSettings.ControllerInput.Red = intParam1;
                thisSettings.ControllerInput.Green = intParam2;
                thisSettings.ControllerInput.Blue = intParam3;
                break;
            case 4: // Trigger threshold
                if (intParam1 == 1) thisSettings.L2UDPDeadzone = intParam2;
                else if (intParam1 == 2) thisSettings.R2UDPDeadzone = intParam2;
                break;
            case 20: // Haptic feedback
                play.File = getParameterValueAsString(instr["parameters"][0]);
                play.DontPlayIfAlreadyPlaying = getBoolValue(instr["parameters"][1]);
                play.Loop = getBoolValue(instr["parameters"][2]);
                thisSettings.AudioPlayQueue.push_back(play);
                break;
            case 21: // Edit audio
                edit.File = getParameterValueAsString(instr["parameters"][0]);
                edit.EditType = static_cast<AudioEditType>(intParam1);
                edit.Value = getFloatValue(instr["parameters"][2]);
                thisSettings.AudioEditQueue.push_back(edit);
                break;
            default:
                std::cout << "Unrecognized instruction received! Number: " << type << std::endl;
                break;
            }
        }
    }
    std::cout << "Server stopped!" << std::endl;
}

void UDP::StartFakeDSXProcess() {
    if (std::filesystem::exists("utilities\\DSX.exe")) {
        STARTUPINFO si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::string command = "utilities\\DSX.exe";
        if (CreateProcess(NULL, (LPSTR)command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    } else {
        std::cout << "DSX.exe is not present" << std::endl;
    }
}

void UDP::Dispose() {
    serverOn = false;
    closesocket(clientSocket);
    WSACleanup();
}
