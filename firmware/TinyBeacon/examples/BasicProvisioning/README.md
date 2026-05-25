# BasicProvisioning Example

This example demonstrates how to use the [TinyBeacon](file:///home/nikolai/Development/tiny-beacon/firmware/TinyBeacon/TinyBeacon.h) library on an ESP32 for dynamic BLE-based provisioning of WiFi and custom metadata fields.

## How to Run

You can compile and run this example using either the **Arduino IDE** or **PlatformIO**.

### Option 1: Arduino IDE

1. Open the Arduino IDE.
2. Go to **File** -> **Open...** and select [BasicProvisioning.ino](file:///home/nikolai/Development/tiny-beacon/firmware/TinyBeacon/examples/BasicProvisioning/BasicProvisioning.ino).
3. Ensure you have the `TinyBeacon` library in your Arduino libraries directory (e.g., by copying the [TinyBeacon](file:///home/nikolai/Development/tiny-beacon/firmware/TinyBeacon) folder into your Arduino `libraries` folder).
4. Select your **ESP32 Dev Module** board and the correct serial port.
5. Click **Upload** and open the Serial Monitor set to `115200` baud.

### Option 2: PlatformIO

This directory includes a [platformio.ini](file:///home/nikolai/Development/tiny-beacon/firmware/TinyBeacon/examples/BasicProvisioning/platformio.ini) configuration file, making it ready to be opened and built as a PlatformIO project.

1. Open VS Code and ensure the PlatformIO extension is installed.
2. Go to **File** -> **Open Folder...** and select the [BasicProvisioning](file:///home/nikolai/Development/tiny-beacon/firmware/TinyBeacon/examples/BasicProvisioning) directory.
3. PlatformIO will automatically load the project, download the Espressif32 platform toolchain, and symlink the local [TinyBeacon](file:///home/nikolai/Development/tiny-beacon/firmware/TinyBeacon) library.
4. Click **Build** (checkmark icon) or **Upload** (arrow icon) on the PlatformIO status bar.
5. Click **Serial Monitor** (plug icon) to view logs at `115200` baud.
