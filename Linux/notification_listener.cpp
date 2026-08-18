#include "notification_listener.h"
#include <QPainter>
#include <QFont>
#include <QColor>
#include <QTime>
#include <QDebug>

NotificationListener::NotificationListener(QObject *parent)
    : QObject(parent), m_esp32Ip("192.168.11.100"), m_esp32Port(5555) {
}

void NotificationListener::setEsp32Address(const QString &ip, quint16 port) {
    m_esp32Ip = ip;
    m_esp32Port = port;
}

bool NotificationListener::registerDBusService() {
    QDBusConnection bus = QDBusConnection::sessionBus();

    bool regObj = bus.registerObject("/org/freedesktop/Notifications", this, QDBusConnection::ExportAllSlots);
    bool regSvc = bus.registerService("org.freedesktop.Notifications");

    // Add match rule for eavesdropping DBus method calls
    QDBusMessage matchMsg = QDBusMessage::createMethodCall(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "AddMatch"
    );
    matchMsg << "type='method_call',interface='org.freedesktop.Notifications',member='Notify',eavesdrop=true";
    QDBusMessage reply = bus.call(matchMsg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        matchMsg.setArguments(QVariantList() << "type='method_call',interface='org.freedesktop.Notifications',member='Notify'");
        bus.call(matchMsg);
    }

    emit logMessage(QString("D-Bus設定完了 (ObjectRegistered: %1, ServiceRegistered: %2)")
                    .arg(regObj ? "成功" : "失敗")
                    .arg(regSvc ? "成功" : "既存サービスと共有"));
    return regObj;
}

uint NotificationListener::Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                                   const QString &summary, const QString &body,
                                   const QStringList &actions, const QVariantMap &hints, int expire_timeout) {
    Q_UNUSED(app_name);
    Q_UNUSED(replaces_id);
    Q_UNUSED(app_icon);
    Q_UNUSED(actions);
    Q_UNUSED(hints);
    Q_UNUSED(expire_timeout);

    processNotification(summary, body);
    return 1;
}

QStringList NotificationListener::GetCapabilities() {
    return QStringList() << "body";
}

void NotificationListener::GetServerInformation(QString &name, QString &vendor, QString &version, QString &spec_version) {
    name = "ESP32UbuntuNotifier";
    vendor = "Custom";
    version = "1.0";
    spec_version = "1.2";
}

void NotificationListener::processNotification(const QString &title, const QString &body) {
    emit logMessage(QString("通知受信: [%1] %2").arg(title, body));
    emit notificationReceived(title, body);

    QImage img = createNotificationImage(title, body);
    QByteArray rgb565Payload = imageToRGB565Payload(img, 10, true);
    sendToESP32(rgb565Payload);
}

QImage NotificationListener::createNotificationImage(const QString &title, const QString &body) {
    QImage img(WIDTH, HEIGHT, QImage::Format_RGB32);
    img.fill(Qt::black);

    QPainter painter(&img);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QFont font("Sans-Serif", 12);
    font.setPixelSize(18);
    painter.setFont(font);

    // Title (Blue: 0, 120, 255)
    painter.setPen(QColor(0, 120, 255));
    QString titleText = QString("💻 %1").arg(title);
    painter.drawText(12, 28, titleText);

    // Body (White: 255, 255, 255)
    if (!body.isEmpty()) {
        font.setPixelSize(16);
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255));
        int charsPerLine = 22;
        int maxLines = 3;
        for (int i = 0; i < maxLines && (i * charsPerLine) < body.length(); ++i) {
            QString line = body.mid(i * charsPerLine, charsPerLine);
            painter.drawText(12, 65 + i * 32, line);
        }
    }

    // Timestamp (Blue: 0, 120, 255)
    font.setPixelSize(16);
    painter.setFont(font);
    painter.setPen(QColor(0, 120, 255));
    QString ts = QTime::currentTime().toString("hh:mm");
    painter.drawText(WIDTH - 65, HEIGHT - 15, ts);

    painter.end();
    return img;
}

QByteArray NotificationListener::imageToRGB565Payload(const QImage &image, uint16_t durationSec, bool swapBytes) {
    QImage img = image.convertToFormat(QImage::Format_RGB32);
    int w = img.width();
    int h = img.height();

    QByteArray rawData;
    // 4-byte header: 'N', 'T', durationSec (hi), durationSec (lo)
    rawData.resize(4 + w * h * 2);
    uchar *ptr = reinterpret_cast<uchar*>(rawData.data());

    ptr[0] = 'N';
    ptr[1] = 'T';
    ptr[2] = static_cast<uchar>((durationSec >> 8) & 0xFF);
    ptr[3] = static_cast<uchar>(durationSec & 0xFF);

    uchar *pixelPtr = ptr + 4;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            QRgb pixel = img.pixel(x, y);
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            uint16_t rgb565 = static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
            int idx = (y * w + x) * 2;
            if (swapBytes) {
                // Little Endian (Low Byte first, High Byte second)
                pixelPtr[idx]     = static_cast<uchar>(rgb565 & 0xFF);
                pixelPtr[idx + 1] = static_cast<uchar>((rgb565 >> 8) & 0xFF);
            } else {
                // Big Endian (High Byte first, Low Byte second)
                pixelPtr[idx]     = static_cast<uchar>((rgb565 >> 8) & 0xFF);
                pixelPtr[idx + 1] = static_cast<uchar>(rgb565 & 0xFF);
            }
        }
    }
    return rawData;
}

void NotificationListener::sendToESP32(const QByteArray &data) {
    QTcpSocket socket;
    socket.connectToHost(m_esp32Ip, m_esp32Port);
    if (socket.waitForConnected(5000)) {
        qint64 totalWritten = 0;
        qint64 totalSize = data.size();
        const char *buffer = data.constData();

        while (totalWritten < totalSize && socket.state() == QAbstractSocket::ConnectedState) {
            qint64 written = socket.write(buffer + totalWritten, totalSize - totalWritten);
            if (written < 0) {
                QString err = QString("TCP送信中にエラー発生: %1").arg(socket.errorString());
                qWarning() << err;
                emit logMessage(err);
                return;
            }
            totalWritten += written;
            if (totalWritten < totalSize) {
                if (!socket.waitForBytesWritten(3000)) {
                    QString err = QString("TCP書き込みタイムアウト: %1").arg(socket.errorString());
                    qWarning() << err;
                    emit logMessage(err);
                    return;
                }
            }
        }

        socket.flush();
        socket.disconnectFromHost();
        if (socket.state() != QAbstractSocket::UnconnectedState) {
            socket.waitForDisconnected(3000);
        }

        QString log = QString("ESP32へ全データ送信完了 (%1/%2 bytes → %3:%4)")
                        .arg(totalWritten).arg(totalSize).arg(m_esp32Ip).arg(m_esp32Port);
        qDebug() << log;
        emit logMessage(log);
    } else {
        QString err = QString("TCP接続エラー: %1 (%2:%3)").arg(socket.errorString()).arg(m_esp32Ip).arg(m_esp32Port);
        qWarning() << err;
        emit logMessage(err);
    }
}
