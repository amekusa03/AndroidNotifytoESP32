#ifndef AUTOSTART_MANAGER_H
#define AUTOSTART_MANAGER_H

#include <QString>

class AutostartManager {
public:
    static QString getAutostartFilePath();
    static bool isAutostartEnabled();
    static bool setAutostartEnabled(bool enable, const QString &appPath = QString());
};

#endif // AUTOSTART_MANAGER_H
