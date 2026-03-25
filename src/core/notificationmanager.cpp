#include "notificationmanager.h"
#include <QDebug>

NotificationManager::NotificationManager(QObject* parent)
    : QObject(parent)
{
    m_sound.setSource(QUrl(QStringLiteral("qrc:/resources/sounds/speech.wav")));
    m_sound.setVolume(1.0);
}

void NotificationManager::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit enabledChanged();
    }
}

void NotificationManager::playReadySound()
{
    if (m_enabled) {
        qDebug() << "NotificationManager: Playing ready sound";
        m_sound.play();
    }
}
