#ifndef NOTIFICATION_LISTENER_H
#define NOTIFICATION_LISTENER_H

#include <QObject>
#include <QtDBus/QtDBus>
#include <QImage>
#include <QTcpSocket>

class NotificationListener : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    explicit NotificationListener(QObject *parent = nullptr);

    void setEsp32Address(const QString &ip, quint16 port);
    QString esp32Ip() const { return m_esp32Ip; }
    quint16 esp32Port() const { return m_esp32Port; }

    bool registerDBusService();
    QImage createNotificationImage(const QString &title, const QString &body);
    QByteArray imageToRGB565Payload(const QImage &image, uint16_t durationSec = 10, bool swapBytes = true);
    void sendToESP32(const QByteArray &data);
    void processNotification(const QString &title, const QString &body);

public slots:
    uint Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                const QString &summary, const QString &body,
                const QStringList &actions, const QVariantMap &hints, int expire_timeout);

    QStringList GetCapabilities();
    void GetServerInformation(QString &name, QString &vendor, QString &version, QString &spec_version);

signals:
    void logMessage(const QString &msg);
    void notificationReceived(const QString &title, const QString &body);

private:
    QString m_esp32Ip;
    quint16 m_esp32Port;
    static constexpr int WIDTH = 320;
    static constexpr int HEIGHT = 170;
};

#endif // NOTIFICATION_LISTENER_H
