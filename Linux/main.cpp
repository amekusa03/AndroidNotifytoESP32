#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QStyle>
#include <QDebug>
#include "notification_listener.h"
#include "settings_dialog.h"
#include "autostart_manager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("ESP32UbuntuNotifier");

    QSettings settings("ESP32Notifier", "Config");
    QString ip = settings.value("esp32_ip", "192.168.11.100").toString();
    int port = settings.value("esp32_port", 5555).toInt();
    
    // Auto-enable autostart if first launch or setting enabled
    bool autostartSetting = settings.value("autostart", true).toBool();
    AutostartManager::setAutostartEnabled(autostartSetting);

    NotificationListener listener;
    listener.setEsp32Address(ip, static_cast<quint16>(port));

    if (!listener.registerDBusService()) {
        qWarning() << "D-Busオブジェクトの登録に失敗しました。";
    }

    QSystemTrayIcon trayIcon;
    trayIcon.setIcon(app.style()->standardIcon(QStyle::SP_ComputerIcon));
    trayIcon.setToolTip("ESP32 Ubuntu Notifier (Qt C++)");

    QMenu trayMenu;
    QAction *settingsAction = trayMenu.addAction("設定(&S)...");
    QAction *testAction = trayMenu.addAction("テスト通知送信(&T)");
    trayMenu.addSeparator();
    QAction *quitAction = trayMenu.addAction("終了(&Q)");

    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();

    SettingsDialog dialog(&listener);

    QObject::connect(settingsAction, &QAction::triggered, [&dialog]() {
        dialog.show();
        dialog.raise();
        dialog.activateWindow();
    });

    QObject::connect(testAction, &QAction::triggered, [&listener]() {
        listener.processNotification("テスト通知", "C++/Qtアプリからの動作確認テストです");
    });

    QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, [&dialog](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            dialog.show();
            dialog.raise();
            dialog.activateWindow();
        }
    });

    qDebug() << "ESP32 Ubuntu Notifier (Qt C++) 起動完了。";
    qDebug() << "送信先:" << ip << ":" << port;

    return app.exec();
}
