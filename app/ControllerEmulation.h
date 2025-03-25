#ifndef EMULATION_H
#define EMULATION_H

#include "DualSense.h"
#include <ViGEm/Client.h>
#include <Windows.h>
#pragma comment(lib, "setupapi.lib")
#include <iostream>
#include <cmath>
#include <stdexcept>

int ConvertRange(int value, int oldMin, int oldMax, int newMin, int newMax);

class Rotors {
public:
    int lrotor = 0;
    int rrotor = 0;
};

class ViGEm {
private:
    PVIGEM_CLIENT client;
    bool wasEmulating360 = false;
    bool wasEmulatingDS4 = false;
    PVIGEM_TARGET x360 = nullptr;
    PVIGEM_TARGET ds4 = nullptr;
    bool working = false;

public:
    Rotors rotors;
    uint8_t Red = 0;
    uint8_t Green = 0;
    uint8_t Blue = 0;

    ViGEm();  // Constructor
    ~ViGEm(); // Destructor

    bool InitializeVigembus();
    void Free();
    bool isWorking();
    void Remove360();
    void RemoveDS4();
    bool StartX360();
    bool StartDS4();
    bool UpdateX360(ButtonState state);
    void SetDS4V1();
    void SetDS4V2();
    bool UpdateDS4(ButtonState state);

    static VOID CALLBACK notification(PVIGEM_CLIENT Client,
                                     PVIGEM_TARGET Target,
                                     UCHAR LargeMotor,
                                     UCHAR SmallMotor,
                                     UCHAR LedNumber,
                                     LPVOID UserData);

    static VOID CALLBACK EVT_VIGEM_DS4_NOTIFICATION(PVIGEM_CLIENT Client,
                                                    PVIGEM_TARGET Target,
                                                    UCHAR LargeMotor,
                                                    UCHAR SmallMotor,
                                                    DS4_LIGHTBAR_COLOR LightbarColor,
                                                    LPVOID UserData);
};

enum EmuStatus {
    None = 0,
    X360 = 1,
    DS4 = 2,
};

#endif // EMULATION_H
