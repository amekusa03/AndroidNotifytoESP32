# ESP32 Notification Display System (ESP32wrNotify)

[English](README.md) | [日本語](README.JP.md)

A system that captures Ubuntu desktop notifications (via D-Bus) or specified image files, converts them into 320x170 pixel RGB565 image data, and transfers/displays them on an ESP32 LCD display via Bluetooth Classic (SPP: Serial Port Profile) or Wi-Fi (TCP Socket).

---

## 🌟 Features & Specifications

* **Display**: 1.9-inch ST7789V3 LCD
* **Resolution**: 320 x 170 pixels
* **Boot Welcome Screen**: Displays a centered "Welcome / ESP32 Notify" screen for 10 seconds upon ESP32 startup, then automatically enters sleep mode. Keeps a clean UI by suppressing raw initialization messages.
* **PC Service Start Notification**: Automatically sends and displays a `🔔 PC Service Start` notification when the Ubuntu monitoring script (`ubuntu_notifier.py`) daemon starts.
* **Color Emoji Support**: Automatic rendering of color emojis using `Noto Color Emoji` (with fallback to monochrome emoji fonts like `Symbola`).
* **Power Saving & Display Duration Control**:
  * **Specified Duration (1–300 sec)**: Backlight turns off and enters sleep mode after the specified duration.
  * **Permanent Display (0 sec)**: Keeps the display on indefinitely until the next image transmission.
  * **Default (unspecified)**: Displays for 10 seconds.
* **Arbitrary Image Transmission Command**:
  * Sends any image (PNG, JPEG, etc.) via CLI argument (`-i`), auto-converted to 320x170 with a customizable display duration.
* **High-Precision Color & Gradient Rendering**:
  * 16-bit Little-Endian RGB565 transmission achieves smooth images without color banding (color jumps).
* **Communication & Protocol Header**:
  * **Protocol Header (Optional)**: 4-byte header (`'N'`, `'T'`, `duration_sec` (uint16_t Big-Endian)) + RGB565 image data (108,800 bytes). Fully backward-compatible with the headerless 108,800-byte legacy format.
  * **Wi-Fi (TCP Socket)**: High-speed network image transfer (Port 5555).
  * **Bluetooth Classic SPP**: RFCOMM Channel 1 communication.
  * **Dual Standby**: ESP32 listens on both Wi-Fi and Bluetooth simultaneously. The sender script (`ubuntu_notifier.py`) supports Wi-Fi priority with automatic Bluetooth fallback (`auto` mode).

---

## 🛠️ Hardware Specifications & Pinout

* **Development Board**: Original ESP32 series (ESP32-D0WD / ESP32-WROOM-32, etc.)
  * *Note: Variants like ESP32-C3 / ESP32-C6 / ESP32-S3 only support BLE and do not support Bluetooth Classic SPP.*
* **Display Controller**: ST7789 / ST7789V3 (170x320)
* **Pinout**:
  * `SCLK`: GPIO 18
  * `MOSI`: GPIO 23
  * `DC`: GPIO 2
  * `CS`: GPIO 15
  * `RST`: GPIO 4
  * `BLK` (Backlight control): GPIO 32

---

## 🎨 Color Scheme & Font Specifications

* **Enhanced Contrast & Anti-Bleed**: For optimal readability on LCD screens, the background is solid black (`0x000000`), titles are rendered in blue (`#0078FF`), and body text in white (`#FFFFFF`).
* **Emoji Rendering**:
  * Primary: Extracts and resizes color emojis using the system's `Noto Color Emoji` font.
  * Fallback: Uses monochrome emoji fonts (`Symbola`, `DejaVuSans`, etc.) if a color glyph is unavailable.

---

## 🚀 Setup Guide

### 1. ESP32 Device Setup

#### 1.1 Bluetooth Configuration (`bt_settings.h`)
Copy `main/bt_settings.h.example` to `main/bt_settings.h` and edit the device name if needed:

```bash
cp main/bt_settings.h.example main/bt_settings.h
```

Configuration items in `main/bt_settings.h`:
* `BT_DEVICE_NAME`: Bluetooth device name (Default: `"ESP32_Notify"`)

#### 1.2 Wi-Fi Configuration (`wifi_settings.h`)
Copy `main/wifi_settings.h.example` to `main/wifi_settings.h` and configure your SSID, password, and static IP:

```bash
cp main/wifi_settings.h.example main/wifi_settings.h
```

Configuration items in `main/wifi_settings.h`:
* `WIFI_SSID`: Target Wi-Fi SSID
* `WIFI_PASS`: Target Wi-Fi Password
* `TCP_PORT`: TCP server listening port (Default: `5555`)
* `IP_ADDR_0` to `IP_ADDR_3`: Static IP address (e.g., `192.168.11.100`)

#### 1.3 Building & Flashing Firmware
Run the following commands in an ESP-IDF configured terminal:

```bash
# Set target device
idf.py set-target esp32

# Build, flash, and open serial monitor
idf.py build
idf.py flash monitor
```

---

### 2. Ubuntu Host Setup

#### 2.1 Install Required System Packages
Install packages for D-Bus communication, Bluetooth, image rendering, and fonts:

```bash
sudo apt update
sudo apt install python3-venv python3-dbus python3-gi gir1.2-glib-2.0 \
                 fonts-noto-color-emoji fonts-symbola fonts-noto-cjk bluez
```

#### 2.2 Bluetooth Pairing (When Using Bluetooth)
1. Power on the ESP32.
2. Go to Ubuntu **Settings** → **Bluetooth**, find `ESP32_Notify`, and complete pairing.

#### 2.3 Python Virtual Environment & Library Installation
Create a virtual environment with `--system-site-packages` so it can access system D-Bus / GI bindings:

```bash
# Navigate to the project directory
cd /path/to/ESP32wrNotify/ESP32

# Create virtual environment
python3 -m venv --system-site-packages .venv

# Activate virtual environment
source .venv/bin/activate

# Install image processing library (Pillow)
pip install pillow
```

*Note: `ubuntu_notifier.py` will automatically switch to the local `.venv` environment even when executed with system `python3`.*

#### 2.4 Running Notification Monitor Mode (D-Bus Daemon)
Adjust settings in `ubuntu_notifier.py` if necessary:
* `CONNECT_MODE`: Connection mode (`"auto"`, `"wifi"`, `"bt"`, `"both"`)
* `DISPLAY_DURATION`: Default display duration in seconds (Default: `10`)
* `ESP32_IP`: ESP32 IP address (for Wi-Fi)
* `ESP32_BT_ADDR`: ESP32 Bluetooth MAC address (Empty `""` for auto-discovery)

```bash
# Start D-Bus notification monitoring mode
python3 ubuntu_notifier.py
```

Send a test notification from another terminal to verify:
```bash
notify-send "Test Notification" "Hello, ESP32 Dual Mode! 🚀"
```

#### 2.5 Sending Arbitrary Image Files Directly (CLI Options)
Converts any image file (PNG/JPEG, etc.) to 320x170 pixels (maintaining aspect ratio with black letterboxing) and sends it directly to the ESP32 with a custom display duration.

```bash
# Send an image with a specific display duration (e.g., 30 seconds)
python3 ubuntu_notifier.py -i /path/to/image.png -d 30

# Specify 0 seconds for permanent display (stays until next transmission)
python3 ubuntu_notifier.py -i /path/to/image.jpg -d 0

# Force full-screen stretch to 320x170 (ignoring aspect ratio)
python3 ubuntu_notifier.py -i /path/to/image.png -d 10 --stretch
```

##### Key CLI Arguments:
* `-i`, `--image`: Path to the image file to send
* `-d`, `--duration`: Display duration in seconds. `0` = Permanent, max `300` (Default: `10`)
* `-m`, `--mode`: Connection mode (`auto`, `wifi`, `bt`, `both`)
* `--stretch`: Ignore aspect ratio and stretch to full 320x170 screen
* `--color-order`: Color channel order (`rgb`, `rbg`, `bgr`, `brg`, `grb`, `gbr`)
* `--invert`: Invert image brightness/colors (negative)
* `--swap-bytes`: Enable byte swapping (Default: enabled)
* `--no-swap-bytes`: Disable byte swapping (Big-Endian transmission)

#### 2.6 Automatic Startup via systemd User Service
To automatically start the notification monitor in the background upon login, run the included `release.sh` script:

```bash
./release.sh
```

* **Check Service Status**:
  ```bash
  systemctl --user status esp32-notify.service
  ```
* **View Real-Time Logs**:
  ```bash
  journalctl --user -u esp32-notify.service -f
  ```
* **Restart Service**:
  ```bash
  systemctl --user restart esp32-notify.service
  ```
