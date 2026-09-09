#include "settings_dialog.h"
#include "autostart_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>
#include <QMessageBox>

SettingsDialog::SettingsDialog(NotificationListener *listener, QWidget *parent)
    : QDialog(parent), m_listener(listener) {
    setWindowTitle("ESP32 Ubuntu Notifier - Settings");
    resize(450, 350);

    QSettings settings("ESP32Notifier", "Config");
    QString currentIp = settings.value("esp32_ip", "192.168.11.100").toString();
    int currentPort = settings.value("esp32_port", 5555).toInt();

    m_ipLineEdit = new QLineEdit(currentIp, this);
    m_portSpinBox = new QSpinBox(this);
    m_portSpinBox->setRange(1, 65535);
    m_portSpinBox->setValue(currentPort);

    m_autostartCheckBox = new QCheckBox("Launch automatically at login (Autostart)", this);
    m_autostartCheckBox->setChecked(AutostartManager::isAutostartEnabled());

    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);

    m_testButton = new QPushButton("Send Test Notification", this);
    m_saveButton = new QPushButton("Save & Close", this);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->addRow("ESP32 IP Address:", m_ipLineEdit);
    formLayout->addRow("TCP Port:", m_portSpinBox);
    formLayout->addRow(m_autostartCheckBox);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(m_testButton);
    btnLayout->addStretch();
    btnLayout->addWidget(m_saveButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(new QLabel("Log Output:", this));
    mainLayout->addWidget(m_logTextEdit);
    mainLayout->addLayout(btnLayout);

    connect(m_testButton, &QPushButton::clicked, this, &SettingsDialog::sendTestNotification);
    connect(m_saveButton, &QPushButton::clicked, this, &SettingsDialog::saveSettings);

    if (m_listener) {
        connect(m_listener, &NotificationListener::logMessage, this, &SettingsDialog::appendLog);
    }
}

void SettingsDialog::appendLog(const QString &msg) {
    m_logTextEdit->append(msg);
}

void SettingsDialog::saveSettings() {
    QString ip = m_ipLineEdit->text().trimmed();
    int port = m_portSpinBox->value();
    bool enableAutostart = m_autostartCheckBox->isChecked();

    if (ip.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter an IP address.");
        return;
    }

    QSettings settings("ESP32Notifier", "Config");
    settings.setValue("esp32_ip", ip);
    settings.setValue("esp32_port", port);
    settings.setValue("autostart", enableAutostart);

    if (m_listener) {
        m_listener->setEsp32Address(ip, static_cast<quint16>(port));
    }

    AutostartManager::setAutostartEnabled(enableAutostart);

    QMessageBox::information(this, "Settings Saved", "Settings saved successfully.");
    accept();
}

void SettingsDialog::sendTestNotification() {
    if (m_listener) {
        m_listener->processNotification("Test Notification", "Test notification sent from Qt C++ app.");
    }
}
