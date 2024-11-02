#include "Settings.h"
#include <ctime>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <ctime>
#include <ratio>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#pragma comment(lib, "Ws2_32.lib")

struct UDPResponse
{
    std::string Status;
    std::string TimeReceived;
    bool isControllerConnected;
    int BatteryLevel;
};

class UDP
{
public:
    bool isActive = false;
    int Battery;
    std::chrono::high_resolution_clock::time_point LastTime;
    struct UDPResponse
    {
        std::string Status;
        std::string TimeReceived;
        bool isControllerConnected;
        int BatteryLevel;

        std::string toJSON() const
        {
            std::ostringstream oss;
            oss << "{ \"Status\": \"" << Status << "\", "
                << "\"TimeReceived\": \"" << TimeReceived << "\", "
                << "\"isControllerConnected\": "
                << (isControllerConnected ? "true" : "false") << ", "
                << "\"BatteryLevel\": " << BatteryLevel << " }";
            return oss.str();
        }
    };

    UDP() : serverOn(false), Battery(100), unavailable(false)
    {
        filesystem::create_directories("C:/Temp/DualSenseX");
        std::ofstream portFile("C:\\Temp\\DualSenseX\\DualSenseX_PortNumber.txt", std::ios::trunc);     
        if (portFile) portFile << "6969";

        Connect();
    }

    ~UDP()
    {
        Dispose();
    }

    void Connect()
    {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0)
        {
            std::cerr << "WSAStartup failed: " << result << "\n";
            return;
        }

        clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (clientSocket == INVALID_SOCKET)
        {
            std::cerr << "Socket creation failed.\n";
            WSACleanup();
            return;
        }

        std::ifstream portFile("C:/Temp/DualSenseX/DualSenseX_PortNumber.txt", std::ios_base::in);
        int portNumber = 6969;
        portFile >> portNumber;

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(portNumber);
        serverAddr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(clientSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) ==
            SOCKET_ERROR)
        {
            std::cerr << "Bind failed.\n";
            closesocket(clientSocket);
            WSACleanup();
            return;
        }
        else {
            cout << "Server started at: " << portNumber << endl;
        }

        serverOn = true;
        std::thread(&UDP::Listen, this).detach();
    }

    void Listen()
    {
        sockaddr_in clientAddr{};
        int clientAddrSize = sizeof(clientAddr);
        char recvBuffer[1024];

        while (serverOn)
        {
            int bytesReceived = recvfrom(clientSocket,
                                         recvBuffer,
                                         sizeof(recvBuffer),
                                         0,
                                         (SOCKADDR *)&clientAddr,
                                         &clientAddrSize);
            if (bytesReceived == SOCKET_ERROR)
            {
                std::cerr << "recvfrom failed.\n";
                continue;
            }

            LastTime = std::chrono::high_resolution_clock::now();

            std::string packetString(recvBuffer, bytesReceived);
            newPacket = true;

            UDPResponse response{"DSX Received UDP Instructions",
                                 currentDateTime(),
                                 true,
                                 Battery};

            std::string responseStr = response.toJSON();
            sendto(clientSocket,
                   responseStr.c_str(),
                   responseStr.size(),
                   0,
                   (SOCKADDR *)&clientAddr,
                   clientAddrSize);

            // Here, you would parse the packetString manually if needed
            isActive = true;
            nlohmann::json data = nlohmann::json::parse(packetString);

            for (auto &instr : data["instructions"])
            {
                uint8_t triggers[7];
                Trigger::TriggerMode mode = Trigger::Off;

                switch (static_cast<int>(instr["type"]))
                {
                    {
                    case 1:
                        uint8_t start = 0;
                        uint8_t end = 0;
                        uint8_t snapForce = 0;
                        uint8_t force = 0;
                        uint8_t firstFoot = 0;
                        uint8_t secondFoot = 0;
                        uint8_t frequency = 0;
                        switch (static_cast<int>(instr["parameters"][2]))
                        {
                        case 0: //Normal
                        {
                            mode = Trigger::Rigid_B;
                            triggers[0] = 0;
                            triggers[1] = 0;
                            triggers[2] = 0;
                            triggers[3] = 0;
                            triggers[4] = 0;
                            triggers[5] = 0;
                            triggers[6] = 0;
                            break;
                        }
                        case 1: //GameCube
                        {
                            mode = Trigger::Pulse;
                            triggers[0] = 144;
                            triggers[1] = 160;
                            triggers[2] = 255;
                            triggers[3] = 0;
                            triggers[4] = 0;
                            triggers[5] = 0;
                            triggers[6] = 0;
                            break;
                        }
                        case 2: //VerySoft
                        {
                            mode = Trigger::Rigid_A;
                            triggers[0] = 128;
                            triggers[1] = 160;
                            triggers[2] = 255;
                            triggers[3] = 255;
                            triggers[4] = 255;
                            triggers[5] = 255;
                            triggers[6] = 255;
                            break;
                        }
                        case 3: //Soft
                        {
                            mode = Trigger::Rigid_A;
                            triggers[0] = 69;
                            triggers[1] = 160;
                            triggers[2] = 255;
                            triggers[3] = 255;
                            triggers[4] = 255;
                            triggers[5] = 255;
                            triggers[6] = 255;
                            break;
                        }
                        case 4: //Hard
                        {
                            mode = Trigger::Rigid_A;
                            triggers[0] = 32;
                            triggers[1] = 160;
                            triggers[2] = 255;
                            triggers[3] = 255;
                            triggers[4] = 255;
                            triggers[5] = 255;
                            triggers[6] = 255;
                            break;
                        }
                        case 5: //VeryHard
                        {
                            mode = Trigger::Rigid_A;
                            triggers[0] = 16;
                            triggers[1] = 160;
                            triggers[2] = 255;
                            triggers[3] = 255;
                            triggers[4] = 255;
                            triggers[5] = 255;
                            triggers[6] = 255;
                            break;
                        }
                        case 6: //Hardest
                        {
                            mode = Trigger::Rigid_A;
                            triggers[0] = 0;
                            triggers[1] = 255;
                            triggers[2] = 255;
                            triggers[3] = 255;
                            triggers[4] = 255;
                            triggers[5] = 255;
                            triggers[6] = 255;
                            break;
                        }
                        case 7: //Rigid
                        {
                            mode = Trigger::Rigid;
                            triggers[0] = 0;
                            triggers[1] = 255;
                            triggers[2] = 0;
                            triggers[3] = 0;
                            triggers[4] = 0;
                            triggers[5] = 0;
                            triggers[6] = 0;
                            break;
                        }
                        case 8: //VibrateTrigger
                        {
                            mode = Trigger::Pulse_AB;
                            triggers[0] = 37;
                            triggers[1] = 35;
                            triggers[2] = 6;
                            triggers[3] = 39;
                            triggers[4] = 33;
                            triggers[5] = 35;
                            triggers[6] = 34;
                            break;
                        }
                        case 9: //Choppy:
                        {
                            mode = Trigger::Rigid_A;
                            triggers[0] = 2;
                            triggers[1] = 39;
                            triggers[2] = 33;
                            triggers[3] = 39;
                            triggers[4] = 38;
                            triggers[5] = 2;
                            triggers[6] = 0;
                            break;
                        }
                        case 10: //Medium
                        {
                            mode = Trigger::Pulse_A;
                            triggers[0] = 2;
                            triggers[1] = 35;
                            triggers[2] = 1;
                            triggers[3] = 6;
                            triggers[4] = 6;
                            triggers[5] = 1;
                            triggers[6] = 33;
                            break;
                        }
                        case 11: //VibrateTriggerPulse
                        {
                            mode = Trigger::Pulse_AB;
                            triggers[0] = 37;
                            triggers[1] = 35;
                            triggers[2] = 6;
                            triggers[3] = 39;
                            triggers[4] = 33;
                            triggers[5] = 35;
                            triggers[6] = 34;
                            break;
                        }
                        case 12: //CustomTriggerValue
                        {
                            int CustomMode = instr["parameters"][3] != NULL ? static_cast<int>(instr["parameters"][3]) : 0;
                            switch (CustomMode) {
                            case 0:
                                mode = Trigger::Off;
                                break;
                                case 1:
                                mode = Trigger::Rigid;
                                break;
                                case 2:
                                mode = Trigger::Rigid_A;
                                break;
                                case 3:
                                mode = Trigger::Rigid_B;
                                break;
                                case 4:
                                mode = Trigger::Rigid_AB;
                                break;
                                case 5:
                                mode = Trigger::Pulse;
                                break;
                                case 6:
                                mode = Trigger::Pulse_A;
                                break;
                                case 7:
                                mode = Trigger::Pulse_B;
                                break;
                                case 8:
                                mode = Trigger::Pulse_AB;
                                break;
                                case 9:
                                mode = Trigger::Pulse_B;
                                break;
                                case 10:
                                mode = Trigger::Pulse_AB;
                                break;
                                case 11:
                                mode = Trigger::Pulse_B;
                                break;
                                case 12:
                                mode = Trigger::Pulse_AB;
                                break;
                                case 13:
                                mode = Trigger::Pulse_AB;
                                break;
                                case 14:
                                mode = Trigger::Pulse_AB;
                                break;
                                case 15:
                                mode = Trigger::Pulse_AB;
                                break;
                                case 16:
                                mode = Trigger::Pulse_AB;
                                break;
                            default:
                                cout << "Unknown trigger: " << CustomMode << endl;
                                for (auto& a : instr["parameters"]) {
                                    cout << " " << a << " ";
                                }
                                cout << endl;
                                break;
                            }

                            triggers[0] = instr["parameters"][4] != NULL ? static_cast<int>(instr["parameters"][4]) : 0;
                            triggers[1] = instr["parameters"][5] != NULL ? static_cast<int>(instr["parameters"][5]) : 0;
                            triggers[2] = instr["parameters"][6] != NULL ? static_cast<int>(instr["parameters"][6]) : 0;
                            triggers[3] = instr["parameters"][7] != NULL ? static_cast<int>(instr["parameters"][7]) : 0;
                            triggers[4] = instr["parameters"][8] != NULL ? static_cast<int>(instr["parameters"][8]) : 0;
                            triggers[5] = instr["parameters"][9] != NULL ? static_cast<int>(instr["parameters"][9]) : 0;
                            triggers[6] = instr["parameters"][10] != NULL ? static_cast<int>(instr["parameters"][10]) : 0;

                            break;
                        }
                        case 13: //Resistance
                        {
                            mode = Trigger::Pulse_A;
                            uint8_t start =
                                instr["parameters"][3] != NULL
                                    ? static_cast<int>(instr["parameters"][3])
                                    : 0;
                            uint8_t force =
                                instr["parameters"][4] != NULL
                                    ? static_cast<int>(instr["parameters"][4])
                                    : 0;

                            if (start > 9 || force > 8)
                            {
                                return;
                            }

                            if (force > 0)
                            {
                                uint8_t b =
                                    static_cast<uint8_t>((force - 1) & 7);
                                uint32_t num = 0;
                                uint16_t num2 = 0;

                                for (int i = static_cast<int>(start); i < 10;
                                     ++i)
                                {
                                    num |=
                                        (static_cast<uint32_t>(b) << (3 * i));
                                    num2 |= (1 << i);
                                }

                                triggers[0] = static_cast<uint8_t>(num2 & 0xFF);
                                triggers[1] =
                                    static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                triggers[2] = static_cast<uint8_t>(num & 0xFF);
                                triggers[3] =
                                    static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[4] =
                                    static_cast<uint8_t>((num >> 16) & 0xFF);
                                triggers[5] =
                                    static_cast<uint8_t>((num >> 24) & 0xFF);
                                triggers[6] = 0;
                            }
                            break;
                        }
                        case 14: //Bow
                        {
                            mode = Trigger::Pulse_A;
                            start =
                                instr["parameters"][3] != NULL
                                    ? static_cast<int>(instr["parameters"][3])
                                    : 0;
                            end = instr["parameters"][4] != NULL
                                      ? static_cast<int>(instr["parameters"][4])
                                      : 0;
                            force =
                                instr["parameters"][5] != NULL
                                    ? static_cast<int>(instr["parameters"][5])
                                    : 0;
                            snapForce =
                                instr["parameters"][6] != NULL
                                    ? static_cast<int>(instr["parameters"][6])
                                    : 0;

                            if (start > 8 || end > 8 || start >= end ||
                                force > 8 || snapForce > 8)
                            {
                                return;
                            }

                            if (end > 0 && force > 0 && snapForce > 0)
                            {
                                uint16_t num = static_cast<uint16_t>(
                                    (1 << start) | (1 << end));
                                uint32_t num2 = static_cast<uint32_t>(
                                    ((force - 1) & 7) |
                                    (((snapForce - 1) & 7) << 3));

                                triggers[0] = static_cast<uint8_t>(num & 0xFF);
                                triggers[1] =
                                    static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[2] = static_cast<uint8_t>(num2 & 0xFF);
                                triggers[3] =
                                    static_cast<uint8_t>((num2 >> 8) & 0xFF);
                                triggers[4] = 0;
                                triggers[5] = 0;
                                triggers[6] = 0;
                            }

                            break;
                        }
                        case 15: //Galloping
                        {
                            mode = Trigger::Pulse_A;

                            start =
                                instr["parameters"][3] != NULL
                                    ? static_cast<int>(instr["parameters"][3])
                                    : 0;
                            end = instr["parameters"][4] != NULL
                                      ? static_cast<int>(instr["parameters"][4])
                                      : 0;
                            firstFoot =
                                instr["parameters"][5] != NULL
                                    ? static_cast<int>(instr["parameters"][5])
                                    : 0;
                            secondFoot =
                                instr["parameters"][6] != NULL
                                    ? static_cast<int>(instr["parameters"][6])
                                    : 0;
                            frequency =
                                instr["parameters"][7] != NULL
                                    ? static_cast<int>(instr["parameters"][7])
                                    : 0;

                            if (start > 8 || end > 9 || start >= end ||
                                secondFoot > 7 || firstFoot > 6 ||
                                firstFoot >= secondFoot)
                            {
                                return;
                            }

                            if (frequency > 0)
                            {
                                uint16_t num = static_cast<uint16_t>(
                                    (1 << start) | (1 << end));
                                uint32_t num2 = static_cast<uint32_t>(
                                    (secondFoot & 7) | ((firstFoot & 7) << 3));

                                triggers[0] = frequency;
                                triggers[1] = static_cast<uint8_t>(num & 0xFF);
                                triggers[2] =
                                    static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[3] = static_cast<uint8_t>(num2 & 0xFF);
                                triggers[4] = 0;
                                triggers[5] = 0;
                                triggers[6] = 0;
                            }
                            break;
                        }
                        case 16: //SemiAutomaticGun
                        {
                            mode = Trigger::Pulse_AB;

                            start =
                                instr["parameters"][3] != NULL
                                    ? static_cast<int>(instr["parameters"][3])
                                    : 0;
                            end = instr["parameters"][4] != NULL
                                      ? static_cast<int>(instr["parameters"][4])
                                      : 0;
                            force =
                                instr["parameters"][5] != NULL
                                    ? static_cast<int>(instr["parameters"][5])
                                    : 0;

                            if (start > 7 || start < 2 || end > 8 ||
                                end <= start || force > 8)
                            {
                                return;
                            }

                            if (force > 0)
                            {
                                uint16_t num = static_cast<uint16_t>(
                                    (1 << start) | (1 << end));

                                triggers[0] = static_cast<uint8_t>(num & 0xFF);
                                triggers[1] =
                                    static_cast<uint8_t>((num >> 8) & 0xFF);
                                triggers[2] = force - 1;
                                triggers[3] = 0;
                                triggers[4] = 0;
                                triggers[5] = 0;
                                triggers[6] = 0;
                            }

                            break;
                        }
                        case 17: //AutomaticGun
                        {
                            mode = Trigger::Pulse_B;
                            start =
                                instr["parameters"][3] != NULL
                                    ? static_cast<int>(instr["parameters"][3])
                                    : 0;
                            uint8_t strength =
                                instr["parameters"][4] != NULL
                                    ? static_cast<int>(instr["parameters"][4])
                                    : 0;
                            uint8_t frequency =
                                instr["parameters"][5] != NULL
                                    ? static_cast<int>(instr["parameters"][5])
                                    : 0;

                            triggers[0] = frequency;
                            triggers[1] = strength;
                            triggers[2] = start;
                            triggers[3] = 255;
                            triggers[4] = 255;
                            triggers[5] = 255;
                            triggers[6] = 255;

                            break;
                        }
                        case 18: //Machine
                        {
                            mode = Trigger::Pulse_B;
                            triggers[0] =
                                instr["parameters"][7] != NULL
                                    ? static_cast<int>(instr["parameters"][7])
                                    : 0;
                            triggers[1] =
                                instr["parameters"][4] != NULL
                                    ? static_cast<int>(instr["parameters"][4])
                                    : 0;
                            triggers[2] =
                                instr["parameters"][8] != NULL
                                    ? static_cast<int>(instr["parameters"][8])
                                    : 0;
                            triggers[3] =
                                instr["parameters"][5] != NULL
                                    ? static_cast<int>(instr["parameters"][5])
                                    : 0;
                            triggers[4] =
                                instr["parameters"][6] != NULL
                                    ? static_cast<int>(instr["parameters"][6])
                                    : 0;
                            triggers[5] = 0;
                            triggers[6] = 0;
                            break;
                        }
                        }

                        if (static_cast<int>(instr["parameters"][1]) == 1)
                        {
                            thisSettings.ControllerInput.LeftTriggerMode = mode;
                            thisSettings.ControllerInput.LeftTriggerForces[0] =
                                triggers[0];
                            thisSettings.ControllerInput.LeftTriggerForces[1] =
                                triggers[1];
                            thisSettings.ControllerInput.LeftTriggerForces[2] =
                                triggers[2];
                            thisSettings.ControllerInput.LeftTriggerForces[3] =
                                triggers[3];
                            thisSettings.ControllerInput.LeftTriggerForces[4] =
                                triggers[4];
                            thisSettings.ControllerInput.LeftTriggerForces[5] =
                                triggers[5];
                            thisSettings.ControllerInput.LeftTriggerForces[6] =
                                triggers[6];
                        }
                        else if (static_cast<int>(instr["parameters"][1]) == 2)
                        {
                            thisSettings.ControllerInput.RightTriggerMode =
                                mode;
                            thisSettings.ControllerInput.RightTriggerForces[0] =
                                triggers[0];
                            thisSettings.ControllerInput.RightTriggerForces[1] =
                                triggers[1];
                            thisSettings.ControllerInput.RightTriggerForces[2] =
                                triggers[2];
                            thisSettings.ControllerInput.RightTriggerForces[3] =
                                triggers[3];
                            thisSettings.ControllerInput.RightTriggerForces[4] =
                                triggers[4];
                            thisSettings.ControllerInput.RightTriggerForces[5] =
                                triggers[5];
                            thisSettings.ControllerInput.RightTriggerForces[6] =
                                triggers[6];
                        }
                        break;
                    }
                case 2:
                {
                    thisSettings.ControllerInput.Red =
                        static_cast<int>(instr["parameters"][1]);
                    thisSettings.ControllerInput.Green =
                        static_cast<int>(instr["parameters"][2]);
                    thisSettings.ControllerInput.Blue =
                        static_cast<int>(instr["parameters"][3]);
                    break;
                }
                default:
                    break;
                }
            }
        }
    }

    static void StartFakeDSXProcess()
    {
        system("start /B DSX.exe");
    }

    void Dispose()
    {
        serverOn = false;
        closesocket(clientSocket);
        WSACleanup();
    }

    Settings GetSettings()
    {
        return thisSettings;
    }

private:
    SOCKET clientSocket = INVALID_SOCKET;
    bool serverOn;
    bool newPacket = false;
    bool unavailable;
    Settings thisSettings;

    std::string currentDateTime()
    {
        std::time_t now = std::time(nullptr);
        std::tm *nowTm = std::localtime(&now);
        char buf[100];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", nowTm);
        return buf;
    }
};
