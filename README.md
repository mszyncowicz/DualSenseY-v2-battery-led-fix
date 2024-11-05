# DualSenseY

DualSenseY (v2) is a C++ application that enables advanced control and visualization for DualSense controllers. Using ImGui for its UI and leveraging libraries for audio and controller emulation, DualSenseY provides features like LED color customization, adaptive trigger configuration, and audio-based haptic feedback.

## Features

- **LED Control**: Customize the RGB colors on the DualSense LED bar or use special modes:
  - **Disco Mode**: Cycles through colors in a smooth transition.
  - **Audio to LED**: Syncs LED brightness with audio peak values for visual feedback.
  
- **Adaptive Trigger Configuration**: Provides customizable force feedback for the DualSense adaptive triggers, with various force modes like rigid, pulse, and calibration.

- **Haptic Feedback**: Supports standard rumble controls and audio-to-haptics, which generates feedback based on system audio. (Available only in USB mode.)

- **Touchpad Visualization**: Displays real-time touch input on the controller's touchpad, including touch coordinates and packet number.

- **Controller Emulation**: Offers DS4 and Xbox 360 emulation using ViGEm, allowing the DualSense to be recognized as an alternative controller in other software.

- **UDP Connectivity**: Integrates UDP communication for remote control or synchronization with other applications.

## Requirements (For programmers)

- **Libraries**:
  - [Dear ImGui](https://github.com/ocornut/imgui)
  - [GLFW](https://www.glfw.org/)
  - [OpenGL](https://www.opengl.org/)
  - [miniaudio](https://github.com/mackron/miniaudio) for audio processing
  - [ViGEm](https://github.com/nefarius/ViGEmClient) for controller emulation
  - **Windows Only**: Requires `IMMDeviceEnumerator` and `IAudioMeterInformation` for audio integration.

## Build Instructions

1. Clone the repository and ensure dependencies are installed.
2. Build the project using CMake or a suitable build system.
3. Run the executable to open the DualSenseY window.

## Usage

1. Start DualSenseY.exe, and select the desired DualSense controller.
2. Use the LED and adaptive trigger controls to customize your experience.
3. Enable **Audio to LED** or **Disco Mode** under the "LED" section.
4. For haptics, select **Audio to Haptics** to experience feedback based on system audio.
5. To switch to emulation mode, choose between DS4 and Xbox 360 under "Controller Emulation."

## Notes

- **Emulation** requires the ViGEmBus driver. Install it if not already present.
- **Haptic Feedback** features are unavailable when using Bluetooth.
