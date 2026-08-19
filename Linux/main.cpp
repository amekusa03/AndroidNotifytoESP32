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

    QCommandLineOption imageOption(QStringList() << "i" << "image", "ESP32に送信・表示する画像ファイルのパス", "file");
    parser.addOption(imageOption);

    QCommandLineOption durationOption(QStringList() << "d" << "duration", "表示時間(秒)。0=永久表示, Max=300 (デフォルト: 10)", "seconds", "10");
    parser.addOption(durationOption);

    QCommandLineOption stretchOption("stretch", "アスペクト比を無視して320x170に拡大・縮小");
    parser.addOption(stretchOption);

    QCommandLineOption ipOption(QStringList() << "host" << "ip", "ESP32 の IP アドレスまたは mDNS ホスト名", "address");
    parser.addOption(ipOption);

    QCommandLineOption portOption("port", "ESP32 の TCP ポート", "port");
    parser.addOption(portOption);

    parser.addPositionalArgument("image_path", "送信する画像ファイルパス (省略可)", "[image_path]");

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

    // 画像指定の判定 (-i/--image オプション、または第1位置引数)
    QString imagePath;
    if (parser.isSet(imageOption)) {
        imagePath = parser.value(imageOption);
    } else if (!parser.positionalArguments().isEmpty()) {
        imagePath = parser.positionalArguments().first();
    }

    if (!imagePath.isEmpty()) {
        int duration = parser.value(durationOption).toInt();
        bool stretch = parser.isSet(stretchOption);

        qDebug() << "指定画像送信モード:" << imagePath
                 << "(表示時間:" << duration << "秒, IP:" << ip << ":" << port
                 << ", ストレッチ:" << (stretch ? "有効" : "無効") << ")";

        bool success = listener.processCustomImageFile(imagePath, static_cast<uint16_t>(duration), stretch, true);
        if (success) {
            std::cout << "画像送信が正常に完了しました。" << std::endl;
            return 0;
        } else {
            std::cerr << "画像送信に失敗しました。" << std::endl;
            return 1;
        }
    }

    // 引数なし（または画像指定なし）の場合は常駐モード
    bool autostartSetting = settings.value("autostart", true).toBool();
    AutostartManager::setAutostartEnabled(autostartSetting);

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
