# ESP32 Notifier for Linux (Qt5 C++ Version)

[English](README.md) | [日本語](README.JP.md)

A desktop notification capture and ESP32 forwarding application for Ubuntu / Linux desktop environments built with Qt5 / C++17.  
It monitors desktop notifications in real time via D-Bus (`org.freedesktop.Notifications`), converts notification titles and body text into 320x170 pixel RGB565 images, and automatically forwards them to an ESP32 display over TCP.

---

## 🌟 Features

- **Automatic D-Bus Notification Capture**: Intercepts desktop notifications (`org.freedesktop.Notifications`), renders them onto a 320x170 canvas formatted for the ESP32 display, and transfers the image.
- **System Tray Residency**: Allows opening settings, sending test notifications, and quitting the application from the system tray icon and context menu.
- **GUI Settings Dialog**:
  - Configure ESP32 IP address and TCP port.
  - Toggle auto-start at user login on/off.
  - Send test notifications to verify operation.
  - View real-time transmission logs.
- **XDG Autostart Support**: Integrates seamlessly with XDG Desktop Autostart (`~/.config/autostart/esp32-notifier.desktop`).
- **Fast & Lightweight**: Native Qt5 / C++17 implementation ensures minimal memory footprint and background CPU usage.

---

## 📦 Dependencies (Build Prerequisites)

Install the required packages on Ubuntu / Debian-based Linux distributions:

```bash
sudo apt update
sudo apt install build-essential cmake qtbase5-dev libqt5dbus5
```

---

## 🛠️ Build & Run

### 1. Build
```bash
cd Linux
cmake -B build -S .
cmake --build build
```

Upon completion, the `build/esp32-notifier` binary will be created.

### 2. Run
```bash
./build/esp32-notifier
```

---

## ⚙️ Usage

1. **Launch**: Running the application displays an icon in the system tray.
2. **Settings**:
   - Click the tray icon (or right-click and select "Settings...") to open the configuration dialog.
   - Enter your ESP32's IP address (Default: `192.168.11.100`) and TCP port (Default: `5555`), then click "Save & Close".
3. **Test Notification**:
   - Click "Send Test Notification" in the settings dialog or choose "Send Test Notification" from the tray menu to test connectivity.
4. **Autostart**:
   - Check "Launch automatically at login (Autostart)" to enable background startup upon user login.

---

## 📐 Technical Specifications

- **Target Screen Size**: 320 x 170 pixels (16-bit RGB565 Little-Endian)
- **Network Protocol**: TCP Socket (Default Port: `5555`)
- **Protocol Header**: 4-byte header (`'N'`, `'T'`, `duration_hi`, `duration_lo`) + 108,800 bytes (320x170x2) Raw RGB565 Data
- **Config File Path**: `~/.config/ESP32Notifier/Config.conf` (QSettings)
- **Autostart File Path**: `~/.config/autostart/esp32-notifier.desktop`
