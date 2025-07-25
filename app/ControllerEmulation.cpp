#include "ControllerEmulation.h"

int ConvertRange(int value, int oldMin, int oldMax, int newMin, int newMax) {
    if (oldMin == oldMax) {
        throw std::invalid_argument("Old minimum and maximum cannot be equal.");
    }
    float ratio = static_cast<float>(newMax - newMin) / static_cast<float>(oldMax - oldMin);
    float scaledValue = (value - oldMin) * ratio + newMin;
    return std::clamp(static_cast<int>(scaledValue), newMin, newMax);
}

ViGEm::ViGEm() {
    x360 = vigem_target_x360_alloc();
    ds4 = vigem_target_ds4_alloc();
}

ViGEm::~ViGEm() {
    Free();
}

bool ViGEm::InitializeVigembus() {
    client = vigem_alloc();
    if (client == nullptr) {
        std::cerr << "Failed to allocate ViGEm Client!" << std::endl;
        working = false;
        return false;
    }

    const auto retval = vigem_connect(client);
    if (!VIGEM_SUCCESS(retval)) {
        std::cerr << "ViGEm Bus connection failed with error code: 0x"
                  << std::hex << retval << std::endl;
        working = false;
        return false;
    }

    working = true;
    return true;
}

void ViGEm::Free() {
    if (client) {
        vigem_free(client);
        client = nullptr;
    }
    if (x360) {
        vigem_target_free(x360);
        x360 = nullptr;
    }
    if (ds4) {
        vigem_target_free(ds4);
        ds4 = nullptr;
    }
}

bool ViGEm::isWorking() {
    return working;
}

void ViGEm::Remove360() {
    if (wasEmulating360) {
        VIGEM_ERROR error = vigem_target_remove(client, x360);
        std::cout << "Xbox 360 controller removed! ERROR CODE: " << error << std::endl;
        vigem_target_x360_unregister_notification(x360);
        wasEmulating360 = false;
    }
}

void ViGEm::RemoveDS4() {
    if (wasEmulatingDS4) {
        VIGEM_ERROR error = vigem_target_remove(client, ds4);
        std::cout << "DualShock 4 controller removed! ERROR CODE: " << error << std::endl;
        vigem_target_ds4_unregister_notification(ds4);
        wasEmulatingDS4 = false;
    }
}

bool ViGEm::StartX360() {
    if (working && !wasEmulating360) {
        VIGEM_ERROR error = vigem_target_add(client, x360);
        std::cout << "Xbox 360 controller started, ERROR CODE: " << error << std::endl;
        if (error == _VIGEM_ERRORS::VIGEM_ERROR_NONE) {
            vigem_target_x360_register_notification(client, x360, notification, this);
            wasEmulating360 = true;
            return true;
        }
        return false;
    }
    return false;
}

bool ViGEm::StartDS4() {
    if (working && !wasEmulatingDS4) {
        VIGEM_ERROR error = vigem_target_add(client, ds4);
        std::cout << "DualShock 4 controller started, ERROR CODE: " << error << std::endl;
        if (error == _VIGEM_ERRORS::VIGEM_ERROR_NONE) {
            vigem_target_ds4_register_notification(client, ds4, EVT_VIGEM_DS4_NOTIFICATION, this);
            wasEmulatingDS4 = true;
            return true;
        }
        return false;
    }
    return false;
}

bool ViGEm::UpdateX360(ButtonState state) {
    XUSB_REPORT report;
    USHORT buttons = 0;

    if (state.DpadUp) buttons |= 0x001;
    if (state.DpadDown) buttons |= 0x002;
    if (state.DpadLeft) buttons |= 0x004;
    if (state.DpadRight) buttons |= 0x008;
    if (state.options) buttons |= 0x0010;
    if (state.share) buttons |= 0x0020;
    if (state.L3) buttons |= 0x0040;
    if (state.R3) buttons |= 0x0080;
    if (state.L1) buttons |= 0x0100;
    if (state.R1) buttons |= 0x0200;
    if (state.ps) buttons |= 0x0400;
    if (state.cross) buttons |= 0x1000;
    if (state.circle) buttons |= 0x2000;
    if (state.square) buttons |= 0x4000;
    if (state.triangle) buttons |= 0x8000;
    
    report.wButtons = buttons;
    report.bLeftTrigger = state.L2;
    report.bRightTrigger = state.R2;
    report.sThumbLX = ConvertRange(state.LX, 0, 255, -32767, 32766);
    report.sThumbLY = ConvertRange(state.LY, 255, 0, -32767, 32766);
    report.sThumbRX = ConvertRange(state.RX, 0, 255, -32767, 32766);
    report.sThumbRY = ConvertRange(state.RY, 255, 0, -32767, 32766);
    vigem_target_x360_update(client, x360, report);
    return true;
}

void ViGEm::SetDS4V1() {
    vigem_target_set_pid(ds4, 0x05c4);
}

void ViGEm::SetDS4V2() {
    vigem_target_set_pid(ds4, 0x09cc);
}

bool ViGEm::UpdateDS4(ButtonState state) {
    DS4_REPORT_EX report{};
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
    if (!state.DpadUp && !state.DpadDown && !state.DpadLeft && !state.DpadRight)
        buttons |= 0x8;
    else {
        buttons &= ~0xF;
        if (state.DpadDown && state.DpadLeft) buttons |= (USHORT)DS4_BUTTON_DPAD_SOUTHWEST;
        else if (state.DpadDown && state.DpadRight) buttons |= (USHORT)DS4_BUTTON_DPAD_SOUTHEAST;
        else if (state.DpadUp && state.DpadRight) buttons |= (USHORT)DS4_BUTTON_DPAD_NORTHEAST;
        else if (state.DpadUp && state.DpadLeft) buttons |= (USHORT)DS4_BUTTON_DPAD_NORTHWEST;
        else if (state.DpadUp) buttons |= (USHORT)DS4_BUTTON_DPAD_NORTH;
        else if (state.DpadRight) buttons |= (USHORT)DS4_BUTTON_DPAD_EAST;
        else if (state.DpadDown) buttons |= (USHORT)DS4_BUTTON_DPAD_SOUTH;
        else if (state.DpadLeft) buttons |= (USHORT)DS4_BUTTON_DPAD_WEST;
    }
    report.Report.wButtons = buttons;

    USHORT specialbuttons = 0;
    specialbuttons = state.ps ? specialbuttons | 1 << 0 : specialbuttons;
    specialbuttons = state.touchBtn ? specialbuttons | 1 << 1 : specialbuttons;
    report.Report.bSpecial = specialbuttons;

    report.Report.bTriggerL = state.L2;
    report.Report.bTriggerR = state.R2;
    report.Report.bBatteryLvl = state.battery.Level;

    DS4_TOUCH touch;
    touch.bPacketCounter = state.TouchPacketNum;
    report.Report.bTouchPacketsN = state.TouchPacketNum;
    touch.bIsUpTrackingNum1 = state.trackPadTouch0.RawTrackingNum;
    touch.bTouchData1[0] = state.trackPadTouch0.X & 0xFF;
    touch.bTouchData1[1] = state.trackPadTouch0.X >> 8 & 0x0F | state.trackPadTouch0.Y << 4 & 0xF0;
    touch.bTouchData1[2] = state.trackPadTouch0.Y >> 4;

    touch.bIsUpTrackingNum2 = state.trackPadTouch1.RawTrackingNum;
    touch.bTouchData2[0] = state.trackPadTouch1.X & 0xFF;
    touch.bTouchData2[1] = state.trackPadTouch1.X >> 8 & 0x0F | state.trackPadTouch1.Y << 4 & 0xF0;
    touch.bTouchData2[2] = state.trackPadTouch1.Y >> 4;
    report.Report.sCurrentTouch = touch;

    report.Report.wAccelX = state.gyro.X;
    report.Report.wAccelY = state.gyro.Y;
    report.Report.wAccelZ = state.gyro.Z;
    report.Report.wGyroX = state.accelerometer.X;
    report.Report.wGyroY = state.accelerometer.Y;
    report.Report.wGyroZ = state.accelerometer.Z;
    report.Report.wTimestamp = state.accelerometer.SensorTimestamp != 0 ? state.accelerometer.SensorTimestamp / 16 : 0;

    vigem_target_ds4_update_ex(client, ds4, report);
    return true;
}

VOID CALLBACK ViGEm::notification(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData) {
    ViGEm* vigemInstance = static_cast<ViGEm*>(UserData);
    vigemInstance->rotors.lrotor = LargeMotor;
    vigemInstance->rotors.rrotor = SmallMotor;
}

VOID CALLBACK ViGEm::EVT_VIGEM_DS4_NOTIFICATION(PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor, UCHAR SmallMotor, DS4_LIGHTBAR_COLOR LightbarColor, LPVOID UserData) {
    ViGEm* vigemInstance = static_cast<ViGEm*>(UserData);
    if (vigemInstance != nullptr) {
        vigemInstance->rotors.lrotor = LargeMotor;
        vigemInstance->rotors.rrotor = SmallMotor;
        vigemInstance->Red = LightbarColor.Red;
        vigemInstance->Green = LightbarColor.Green;
        vigemInstance->Blue = LightbarColor.Blue;
    }
}
