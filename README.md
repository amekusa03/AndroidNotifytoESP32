# AndroidNotifytoESP32 / ESP32wrNotify

[English](README.md) | [日本語](README.JP.md)

An application and system that catches Android smartphone notifications or Ubuntu/Linux desktop D-Bus notifications, converts them into 320x170 pixel image data (RGB565 format), and transfers them to an ESP32 LCD display for viewing.

---

## 🌟 Key Features

- **Auto-Catch & Image Conversion of Notifications**:
  - **Android App**: Captures system notifications and renders the title, body, and timestamp into an image in real time.
  - **Linux (C++/Qt) App**: System tray resident GUI app. Automatically catches D-Bus notifications and forwards them to ESP32. Supports settings UI and autostart.
  - **Ubuntu (Python) Script**: Monitors D-Bus notifications and renders them as images with color emoji support. Also supports sending arbitrary images from the CLI.
- **ESP32 Boot Welcome Screen**:
  - Displays a "Welcome / ESP32 Notify" screen for 10 seconds upon ESP32 boot, then automatically sleeps. Hides initialization messages for better visibility.
- **Two Communication Modes**:
  - **TCP (Wi-Fi / mDNS)**: High-speed transmission via `esp32-notify.local` or IP address without requiring a fixed IP thanks to mDNS support (Port 5555).
  - **Bluetooth Classic (SPP)**: Allows transmission via Bluetooth Serial even in environments without Wi-Fi (RFCOMM Ch 1).
- **Display Duration Control Option (0 seconds for permanent display, Max 300 seconds)**:
  - **1 to 300 seconds**: After displaying for the specified duration, the backlight turns off and the display IC goes into sleep mode.
  - **0 seconds**: Stays on permanently until the next image is sent.
  - **Default**: Displays for 10 seconds when not specified.
- **Ubuntu/Linux Arbitrary Image Sending Command (Python)**:
  - Automatically converts specified image files (PNG/JPEG, etc.) to 320x170 pixels and sends them with a specified display duration.
- **High-Precision Color Rendering**:
  - Little-Endian RGB565 optimization achieves smooth photo and gradient display without color banding (color jumping).

---

## 📱 How to Use the Android App

### 1. Setup
1. After launching the app, grant notification access permissions from the "Open Notification Access Settings" button.
2. If using Bluetooth transmission, complete the **pairing** between your Android device and the ESP32 in the Android settings beforehand.

### 2. Operation
1. **Select Transmission Mode**: Choose "TCP (Wi-Fi)" or "Bluetooth".
2. **Connection Info / Settings**:
   - **TCP**: Enter the ESP32's IP address or mDNS hostname (`esp32-notify.local`).
   - **Bluetooth**: Enter the name of the paired ESP32 device.
   - **Display Time (s)**: Enter the image display duration (0: Permanent, 1-300 seconds, Default: 10 seconds).
3. **Forward Notifications**: When a notification arrives, it will automatically be sent as an image to the ESP32.

---

## 🐧 How to Use the Linux / Ubuntu App

### 1. Qt5 C++ GUI Version (`Linux/`) *Recommended resident app*

A C++ client supporting system tray residency, a GUI settings screen, and autostart.

#### Build and Run
```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake qtbase5-dev libqt5dbus5

# Build
cd Linux
cmake -B build -S .
cmake --build build

# Run
./build/esp32-notifier
```
Please see [`Linux/README.md`](Linux/README.md) for details.

---

### 2. Python Script Version (`ESP32/ubuntu_notifier.py`)

#### D-Bus Notification Monitoring Mode (Resident)
```bash
python3 ESP32/ubuntu_notifier.py
```
*Automatically monitors system notifications and sends them to the ESP32 when received.*

#### Specifying and Sending Arbitrary Image Files (CLI Command)
Automatically resizes arbitrary image files (PNG/JPEG, etc.) to 320x170 and sends them with a specified display duration.

```bash
# Specify image and display time (e.g., 30 seconds)
python3 ESP32/ubuntu_notifier.py -i /path/to/image.png -d 30

# Specify 0 seconds (permanent display until next transmission)
python3 ESP32/ubuntu_notifier.py -i /path/to/image.jpg -d 0

# Ignore aspect ratio and force stretch/shrink to 320x170
python3 ESP32/ubuntu_notifier.py -i /path/to/image.png -d 10 --stretch
```

##### Main CLI Command Options
- `-i`, `--image`: Path to the image file to send
- `-d`, `--duration`: Display time (seconds). `0` = Permanent, max `300` (Default: `10`)
- `-m`, `--mode`: Connection mode (`auto`, `wifi`, `bt`, `both`)
- `--stretch`: Ignore aspect ratio and force full-screen resize to 320x170
- `--color-order`: Color channel order (e.g., `rgb`, `rbg`, `bgr`)
- `--invert`: Invert the image's brightness/color (negative)

---

## ⚡ ESP32 Firmware Setup

Please see [`ESP32/README.md`](ESP32/README.md) for firmware build, pinout, and setup instructions.

---

## 🛠️ Technical Specifications

- **Resolution**: 320 x 170 pixels
- **Data Format**: 16-bit RGB565 (Little-Endian, 108,800 bytes/frame)
- **Protocol Header (Optional)**: First 4 bytes (`'N'`, `'T'`, `duration_sec` (uint16_t Big-Endian))
- **Bluetooth Profile**: SPP (Serial Port Profile) - UUID: `00001101-0000-1000-8000-00805f9b34fb`

---

## Development Environment
- **Android App**: Android Studio Ladybug | Kotlin / Jetpack Navigation / ViewBinding
- **Linux App (Qt)**: Qt5 / C++17 / CMake (D-Bus, Widgets, Network)
- **ESP32 Firmware**: ESP-IDF v5.4 / LovyanGFX
- **Ubuntu Client (Python)**: Python 3.12 / Pillow / dbus-python
