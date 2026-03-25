#include "network/connectionmanager.h"
#include "network/decenzaclient.h"
#include "network/relayclient.h"
#include "network/discovery.h"
#include "core/settings.h"

#include <QDebug>

ConnectionManager::ConnectionManager(Settings* settings, Discovery* discovery,
                                     DecenzaClient* localClient, RelayClient* remoteClient,
                                     QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_discovery(discovery)
    , m_localClient(localClient)
    , m_remoteClient(remoteClient)
{
    // Local client signals
    connect(m_localClient, &DecenzaClient::statusUpdated,
            this, &ConnectionManager::onLocalStatusUpdated);
    connect(m_localClient, &DecenzaClient::connectedChanged,
            this, &ConnectionManager::onLocalConnectedChanged);
    connect(m_localClient, &DecenzaClient::loginRequired,
            this, &ConnectionManager::onLocalLoginRequired);

    // Remote client signals
    connect(m_remoteClient, &RelayClient::statusReceived,
            this, &ConnectionManager::onRemoteStatusReceived);
    connect(m_remoteClient, &RelayClient::connectedChanged,
            this, &ConnectionManager::onRemoteConnectedChanged);

    // Discovery signals
    connect(m_discovery, &Discovery::deviceFound,
            this, &ConnectionManager::onDiscoveryFound);
    connect(m_discovery, &Discovery::searchFailed,
            this, &ConnectionManager::onDiscoveryFailed);
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

    qDebug() << "ConnectionManager: starting discovery";
    m_discovery->startSearch();
}

void ConnectionManager::wake()
{
    if (m_localClient->isConnected()) {
        m_localClient->wake();
    } else if (m_remoteClient->isConnected()) {
        m_remoteClient->sendCommand(QStringLiteral("wake"));
    }
}

void ConnectionManager::sleep()
{
    if (m_localClient->isConnected()) {
        m_localClient->sleep();
    } else if (m_remoteClient->isConnected()) {
        m_remoteClient->sendCommand(QStringLiteral("sleep"));
    }
}

void ConnectionManager::disconnect()
{
    m_localClient->disconnect();
    m_remoteClient->disconnect();
    setMode(QStringLiteral("disconnected"));
}

// ---------------------------------------------------------------------------
// Local client slots
// ---------------------------------------------------------------------------

void ConnectionManager::onLocalStatusUpdated()
{
    m_machineState = m_localClient->machineState();
    m_machinePhase = m_localClient->machinePhase();
    m_temperature = m_localClient->temperature();
    m_waterLevelMl = m_localClient->waterLevelMl();
    m_isHeating = m_localClient->isHeating();
    m_isReady = m_localClient->isReady();
    m_isAwake = m_localClient->isAwake();

    emit statusChanged();
}

void ConnectionManager::onLocalConnectedChanged()
{
    if (m_localClient->isConnected()) {
        setMode(QStringLiteral("local"));
    } else if (m_mode == QLatin1String("local")) {
        // Local disconnected while we were in local mode -- try remote
        tryRemoteFallback();
    }
}

void ConnectionManager::onLocalLoginRequired()
{
    emit loginRequired();
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
    if (!m_remoteClient->isConnected() && m_mode == QLatin1String("remote")) {
        setMode(QStringLiteral("disconnected"));
    }
}

// ---------------------------------------------------------------------------
// Discovery slots
// ---------------------------------------------------------------------------

void ConnectionManager::onDiscoveryFound(const QString& deviceName, const QString& serverUrl,
                                         int port, bool secure)
{
    Q_UNUSED(deviceName)
    Q_UNUSED(port)
    Q_UNUSED(secure)

    qDebug() << "ConnectionManager: local device found at" << serverUrl;
    m_localClient->connectToServer(serverUrl);
    setMode(QStringLiteral("local"));
}

void ConnectionManager::onDiscoveryFailed()
{
    qDebug() << "ConnectionManager: local discovery failed, trying remote";
    if (m_settings->isPaired()) {
        tryRemoteFallback();
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

void ConnectionManager::tryRemoteFallback()
{
    qDebug() << "ConnectionManager: attempting remote relay connection";
    m_remoteClient->connectToRelay();
}
