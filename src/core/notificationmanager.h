#pragma once

#include <QObject>
#include <QSoundEffect>

class NotificationManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit NotificationManager(QObject* parent = nullptr);

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    Q_INVOKABLE void playReadySound();

signals:
    void enabledChanged();

private:
    QSoundEffect m_sound;
    bool m_enabled = true;
};
