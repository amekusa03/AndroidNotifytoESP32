#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QStyle>
#include <QDebug>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <iostream>
#include "notification_listener.h"
#include "settings_dialog.h"
#include "autostart_manager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("ESP32UbuntuNotifier");
    app.setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("ESP32 Ubuntu Notifier & Image Sender");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption imageOption(QStringList() << "i" << "image", "Path of image file to send and display on ESP32", "file");
    parser.addOption(imageOption);

    QCommandLineOption durationOption(QStringList() << "d" << "duration", "Display duration in seconds. 0=Permanent, Max=300 (Default: 10)", "seconds", "10");
    parser.addOption(durationOption);

    QCommandLineOption stretchOption("stretch", "Ignore aspect ratio and stretch to 320x170");
    parser.addOption(stretchOption);

    QCommandLineOption ipOption(QStringList() << "host" << "ip", "ESP32 IP address or mDNS hostname", "address");
    parser.addOption(ipOption);

    QCommandLineOption portOption("port", "ESP32 TCP port", "port");
    parser.addOption(portOption);

    parser.addPositionalArgument("image_path", "Image file path to send (optional)", "[image_path]");

    parser.process(app);

    QSettings settings("ESP32Notifier", "Config");
    QString ip = settings.value("esp32_ip", "192.168.11.100").toString();
    int port = settings.value("esp32_port", 5555).toInt();

    if (parser.isSet(ipOption)) {
        ip = parser.value(ipOption);
    }
    if (parser.isSet(portOption)) {
        bool ok = false;
        int p = parser.value(portOption).toInt(&ok);
        if (ok && p > 0 && p <= 65535) {
            port = p;
        }
    }

    NotificationListener listener;
    listener.setEsp32Address(ip, static_cast<quint16>(port));

    // Determine custom image mode (-i/--image option or 1st positional argument)
    QString imagePath;
    if (parser.isSet(imageOption)) {
        imagePath = parser.value(imageOption);
    } else if (!parser.positionalArguments().isEmpty()) {
        imagePath = parser.positionalArguments().first();
    }

    if (!imagePath.isEmpty()) {
        int duration = parser.value(durationOption).toInt();
        bool stretch = parser.isSet(stretchOption);

        qDebug() << "Custom image send mode:" << imagePath
                 << "(Duration:" << duration << "s, IP:" << ip << ":" << port
                 << ", Stretch:" << (stretch ? "Enabled" : "Disabled") << ")";

        bool success = listener.processCustomImageFile(imagePath, static_cast<uint16_t>(duration), stretch, true);
        if (success) {
            std::cout << "Image sent successfully." << std::endl;
            return 0;
        } else {
            std::cerr << "Failed to send image." << std::endl;
            return 1;
        }
    }

    // Run in system tray background mode if no image arguments given
    bool autostartSetting = settings.value("autostart", true).toBool();
    AutostartManager::setAutostartEnabled(autostartSetting);

    if (!listener.registerDBusService()) {
        qWarning() << "Failed to register D-Bus object.";
    }

    QSystemTrayIcon trayIcon;
    trayIcon.setIcon(app.style()->standardIcon(QStyle::SP_ComputerIcon));
    trayIcon.setToolTip("ESP32 Ubuntu Notifier (Qt C++)");

    QMenu trayMenu;
    QAction *settingsAction = trayMenu.addAction("&Settings...");
    QAction *testAction = trayMenu.addAction("Send &Test Notification");
    trayMenu.addSeparator();
    QAction *quitAction = trayMenu.addAction("&Quit");

    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    SettingsDialog dialog(&listener);

    QObject::connect(settingsAction, &QAction::triggered, [&dialog]() {
        dialog.show();
        dialog.raise();
        dialog.activateWindow();
    });

    QObject::connect(testAction, &QAction::triggered, [&listener]() {
        listener.processNotification("Test Notification", "This is a test notification from the Qt C++ app.");
    });

    QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, [&dialog](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            dialog.show();
            dialog.raise();
            dialog.activateWindow();
        }
    });

    qDebug() << "ESP32 Ubuntu Notifier (Qt C++) started.";
    qDebug() << "Target:" << ip << ":" << port;

    return app.exec();
}
