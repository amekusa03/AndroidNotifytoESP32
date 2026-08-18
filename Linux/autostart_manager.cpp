#include "autostart_manager.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDebug>

QString AutostartManager::getAutostartFilePath() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QDir(configDir).filePath("autostart/esp32-notifier.desktop");
}

bool AutostartManager::isAutostartEnabled() {
    return QFile::exists(getAutostartFilePath());
}

bool AutostartManager::setAutostartEnabled(bool enable, const QString &appPath) {
    QString filePath = getAutostartFilePath();
    if (!enable) {
        if (QFile::exists(filePath)) {
            bool removed = QFile::remove(filePath);
            qDebug() << "Autostart desktop file removed:" << removed;
            return removed;
        }
        return true;
    }

    QString executablePath = appPath.isEmpty() ? QCoreApplication::applicationFilePath() : appPath;
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create autostart file:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Name=ESP32 Ubuntu Notifier\n";
    out << "Comment=Send Ubuntu Desktop notifications to ESP32-C6 over TCP\n";
    out << "Exec=" << executablePath << "\n";
    out << "Terminal=false\n";
    out << "X-GNOME-Autostart-enabled=true\n";
    out << "Categories=Utility;\n";
    file.close();

    qDebug() << "Autostart desktop file created at:" << filePath;
    return true;
}
