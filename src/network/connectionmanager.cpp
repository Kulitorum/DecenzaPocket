#include "network/connectionmanager.h"
#include "network/relayclient.h"
#include "core/settings.h"

#include <QDebug>


ConnectionManager::ConnectionManager(Settings* settings, RelayClient* remoteClient,
                                     QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_remoteClient(remoteClient)
{
    connect(m_remoteClient, &RelayClient::statusReceived,
            this, &ConnectionManager::onRemoteStatusReceived);
    connect(m_remoteClient, &RelayClient::connectedChanged,
            this, &ConnectionManager::onRemoteConnectedChanged);
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void ConnectionManager::start()
{
    if (!m_settings->isPaired()) {
        qDebug() << "ConnectionManager: not paired, cannot start";
        return;
    }

    qDebug() << "ConnectionManager: starting relay connection";
    m_remoteClient->connectToRelay();
}

void ConnectionManager::wake()
{
    if (m_remoteClient->isConnected()) {
        m_remoteClient->sendCommand(QStringLiteral("wake"));
    } else {
        qWarning() << "ConnectionManager: wake() called but no connection available";
    }
}

void ConnectionManager::sleep()
{
    if (m_remoteClient->isConnected()) {
        m_remoteClient->sendCommand(QStringLiteral("sleep"));
    } else {
        qWarning() << "ConnectionManager: sleep() called but no connection available";
    }
}

void ConnectionManager::disconnect()
{
    m_remoteClient->disconnect();
    setMode(QStringLiteral("disconnected"));
    qDebug() << "ConnectionManager: disconnected";
}

// ---------------------------------------------------------------------------
// Remote client slots
// ---------------------------------------------------------------------------

void ConnectionManager::onRemoteStatusReceived(const QString& state, const QString& phase,
                                               double temperature, double waterLevelMl,
                                               bool isHeating, bool isReady, bool isAwake)
{
    m_machineState = state;
    m_machinePhase = phase;
    m_temperature = temperature;
    m_waterLevelMl = waterLevelMl;
    m_isHeating = isHeating;
    m_isReady = isReady;
    m_isAwake = isAwake;

    setMode(QStringLiteral("remote"));
    emit statusChanged();
}

void ConnectionManager::onRemoteConnectedChanged()
{
    qDebug() << "ConnectionManager: remote connected changed:"
             << m_remoteClient->isConnected();
    if (m_remoteClient->isConnected()) {
        setMode(QStringLiteral("remote"));
    } else {
        setMode(QStringLiteral("disconnected"));
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ConnectionManager::setMode(const QString& mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        qDebug() << "ConnectionManager: mode changed to" << m_mode;
        emit modeChanged();
    }
}
