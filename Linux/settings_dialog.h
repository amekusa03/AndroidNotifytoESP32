#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include "notification_listener.h"

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(NotificationListener *listener, QWidget *parent = nullptr);

public slots:
    void appendLog(const QString &msg);

private slots:
    void saveSettings();
    void sendTestNotification();

private:
    NotificationListener *m_listener;
    QLineEdit *m_ipLineEdit;
    QSpinBox *m_portSpinBox;
    QCheckBox *m_autostartCheckBox;
    QTextEdit *m_logTextEdit;
    QPushButton *m_testButton;
    QPushButton *m_saveButton;
};

#endif // SETTINGS_DIALOG_H
