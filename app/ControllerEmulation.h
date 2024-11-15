#include <ViGEm/Client.h>
#include <Windows.h>
#pragma comment(lib, "setupapi.lib")
#include <cmath>
#include <iostream>
#include <stdexcept>

int ConvertRange(int value, int oldMin, int oldMax, int newMin, int newMax)
{
    if (oldMin == oldMax)
    {
        throw std::invalid_argument("Old minimum and maximum cannot be equal.");
    }
    float ratio = static_cast<float>(newMax - newMin) /
                  static_cast<float>(oldMax - oldMin);
    float scaledValue = (value - oldMin) * ratio + newMin;
    return clamp(static_cast<int>(scaledValue), newMin, newMax);
}

class Output
{
public:
    int lrotor = 0;
    int rrotor = 0;
    int R = 0;
    int G = 0;
    int B = 0;
};

class ViGEm
{
private:
    PVIGEM_CLIENT client = nullptr;
    bool wasEmulating360 = false;
    bool wasEmulatingDS4 = false;
    bool wasStarted360 = false;
    bool wasStartedDS4 = false;
    bool working = false;

public:
    ViGEm(PVIGEM_CLIENT &Client) {
        client = Client;
        if (client != nullptr) {
            working = true;
        }      
    }

    bool isWorking()
    {
        return working;
    }

    void RemoveController(PVIGEM_TARGET &target)
    {
        if (vigem_target_is_attached(target)) {
            vigem_target_remove(client, target);
            vigem_target_x360_unregister_notification(target);
            vigem_target_ds4_unregister_notification(target);
        } 
    }

    bool StartX360(PVIGEM_TARGET &target, Output &output)
    {
        if (working)
        {
            vigem_target_add(client, target);
            vigem_target_x360_register_notification(client, target, notification, &output);
            return true;
        }

        return false;
    }

    bool StartDS4(PVIGEM_TARGET &target, Output &output)
    {
        if (working)
        {
            vigem_target_add(client, target);
            vigem_target_ds4_register_notification(client,target,EVT_VIGEM_DS4_NOTIFICATION, &output);
            return true;
            
        }

        return false;
    }

    bool UpdateX360(PVIGEM_TARGET &target, ButtonState state)
    {
        if (client && target)
        {
            XUSB_REPORT report;
            USHORT buttons = 0;

            if (state.DpadUp)
                buttons = 0x001;
            if (state.DpadDown)
                buttons = buttons | 0x002;
            if (state.DpadLeft)
                buttons = buttons | 0x004;
            if (state.DpadRight)
                buttons = buttons | 0x008;
            if (state.options)
                buttons = buttons | 0x0010;
            if (state.share)
                buttons = buttons | 0x0020;
            if (state.L3)
                buttons = buttons | 0x0040;
            if (state.R3)
                buttons = buttons | 0x0080;
            if (state.L1)
                buttons = buttons | 0x0100;
            if (state.R1)
                buttons = buttons | 0x0200;
            if (state.cross)
                buttons = buttons | 0x1000;
            if (state.circle)
                buttons = buttons | 0x2000;
            if (state.square)
                buttons = buttons | 0x4000;
            if (state.triangle)
                buttons = buttons | 0x8000;

            report.wButtons = buttons;
            report.bLeftTrigger = state.L2;
            report.bRightTrigger = state.R2;
            report.sThumbLX = ConvertRange(state.LX, 0, 255, -32767, 32766);
            report.sThumbLY = ConvertRange(state.LY, 255, 0, -32767, 32766);
            report.sThumbRX = ConvertRange(state.RX, 0, 255, -32767, 32766);
            report.sThumbRY = ConvertRange(state.RY, 255, 0, -32767, 32766);
            vigem_target_x360_update(client, target, report);
            return true;
        }

        return false;
    }

    bool UpdateDS4(PVIGEM_TARGET &target, ButtonState state)
    {
        if (client && target)
        {
            DS4_REPORT_EX report;
            report.Report.bThumbLX = state.LX;
            report.Report.bThumbLY = state.LY;
            report.Report.bThumbRX = state.RX;
            report.Report.bThumbRY = state.RY;

            USHORT buttons = 0;
            buttons = state.R3 ? buttons | 1 << 15 : buttons;
            buttons = state.L3 ? buttons | 1 << 14 : buttons;
            buttons = state.options ? buttons | 1 << 13 : buttons;
            buttons = state.share ? buttons | 1 << 12 : buttons;
            buttons = state.R2Btn ? buttons | 1 << 11 : buttons;
            buttons = state.L2Btn ? buttons | 1 << 10 : buttons;
            buttons = state.R1 ? buttons | 1 << 9 : buttons;
            buttons = state.L1 ? buttons | 1 << 8 : buttons;
            buttons = state.triangle ? buttons | 1 << 7 : buttons;
            buttons = state.circle ? buttons | 1 << 6 : buttons;
            buttons = state.cross ? buttons | 1 << 5 : buttons;
            buttons = state.square ? buttons | 1 << 4 : buttons;
            if (!state.DpadUp && !state.DpadDown && !state.DpadLeft &&
                !state.DpadRight)
                buttons = buttons | 0x8;
            else
            {
                buttons &= ~0xF;
                if (state.DpadDown && state.DpadLeft)
                    buttons |= (USHORT)DS4_BUTTON_DPAD_SOUTHWEST;
                else if (state.DpadDown && state.DpadRight)
                    buttons |= (USHORT)DS4_BUTTON_DPAD_SOUTHEAST;
                else if (state.DpadUp && state.DpadRight)
                    buttons |= (USHORT)DS4_BUTTON_DPAD_NORTHEAST;
                else if (state.DpadUp && state.DpadLeft)
                    buttons |= (USHORT)DS4_BUTTON_DPAD_NORTHWEST;
                else if (state.DpadUp)
                    buttons |= (USHORT)DS4_BUTTON_DPAD_NORTH;
                else if (state.DpadRight)
                    buttons |= (USHORT)DS4_BUTTON_DPAD_EAST;
                else if (state.DpadDown)
                    buttons |= (USHORT)DS4_BUTTON_DPAD_SOUTH;
                else if (state.DpadLeft)
                    buttons |= (USHORT)DS4_BUTTON_DPAD_WEST;
            }
            report.Report.wButtons = buttons;

            USHORT specialbuttons = 0;
            specialbuttons =
                state.ps ? specialbuttons | 1 << 0 : specialbuttons;
            specialbuttons =
                state.touchBtn ? specialbuttons | 1 << 1 : specialbuttons;
            report.Report.bSpecial = specialbuttons;

            report.Report.bTriggerL = state.L2;
            report.Report.bTriggerR = state.R2;
            report.Report.bBatteryLvl = state.battery.Level;

            DS4_TOUCH touch;
            touch.bPacketCounter = state.TouchPacketNum;
            report.Report.bTouchPacketsN = state.TouchPacketNum;
            touch.bIsUpTrackingNum1 = state.trackPadTouch0.RawTrackingNum;
            touch.bTouchData1[0] = state.trackPadTouch0.X & 0xFF;
            touch.bTouchData1[1] = state.trackPadTouch0.X >> 8 & 0x0F |
                                   state.trackPadTouch0.Y << 4 & 0xF0;
            touch.bTouchData1[2] = state.trackPadTouch0.Y >> 4;

            touch.bIsUpTrackingNum2 = state.trackPadTouch1.RawTrackingNum;
            touch.bTouchData2[0] = state.trackPadTouch1.X & 0xFF;
            touch.bTouchData2[1] = state.trackPadTouch1.X >> 8 & 0x0F |
                                   state.trackPadTouch1.Y << 4 & 0xF0;
            touch.bTouchData2[2] = state.trackPadTouch1.Y >> 4;
            report.Report.sCurrentTouch = touch;

            report.Report.wAccelX = state.accelerometer.X;
            report.Report.wAccelY = state.accelerometer.Y;
            report.Report.wAccelZ = state.accelerometer.Z;
            report.Report.wGyroX = state.gyro.X;
            report.Report.wGyroY = state.gyro.Y;
            report.Report.wGyroZ = state.gyro.Z;
            report.Report.wTimestamp =
                state.accelerometer.SensorTimestamp != 0
                    ? state.accelerometer.SensorTimestamp / 16
                    : 0;

            vigem_target_ds4_update_ex(client, target, report);
            return true;
        }

        return false;
    }

    static VOID CALLBACK notification(PVIGEM_CLIENT Client,
                                      PVIGEM_TARGET Target,
                                      UCHAR LargeMotor,
                                      UCHAR SmallMotor,
                                      UCHAR LedNumber,
                                      LPVOID UserData)
    {
        Output *output = static_cast<Output *>(UserData);
        if (output != nullptr)
        {
            output->lrotor = LargeMotor;
            output->rrotor = SmallMotor;
        }
    }

    static VOID CALLBACK
    EVT_VIGEM_DS4_NOTIFICATION(PVIGEM_CLIENT Client,
                               PVIGEM_TARGET Target,
                               UCHAR LargeMotor,
                               UCHAR SmallMotor,
                               DS4_LIGHTBAR_COLOR LightbarColor,
                               LPVOID UserData)
    {
        Output *output = static_cast<Output *>(UserData);
        if (output != nullptr)
        {
            output->lrotor = LargeMotor;
            output->rrotor = SmallMotor;
            output->R = LightbarColor.Red;
            output->G = LightbarColor.Green;
            output->B = LightbarColor.Blue;
        }
    }
};

enum EmuStatus
{
    None = 0,
    X360 = 1,
    DS4 = 2,
};
