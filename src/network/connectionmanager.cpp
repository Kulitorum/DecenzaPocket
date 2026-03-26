#include "network/connectionmanager.h"
#include "network/decenzaclient.h"
#include "network/relayclient.h"
#include "network/discovery.h"
#include "core/settings.h"

#include <QDebug>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QCoreApplication>
#include <QtCore/qcoreapplication_platform.h>

static void startKeepAliveService()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        qWarning() << "ConnectionManager: no Android context for service start";
        return;
    }

    QJniObject className = QJniObject::fromString(
        "io.github.kulitorum.decenzapocket.KeepAliveService");
    QJniObject cls = QJniObject::callStaticObjectMethod(
        "java/lang/Class", "forName",
        "(Ljava/lang/String;)Ljava/lang/Class;",
        className.object<jstring>());

    QJniObject intent("android/content/Intent",
        "(Landroid/content/Context;Ljava/lang/Class;)V",
        context.object(), cls.object());

    context.callObjectMethod("startForegroundService",
        "(Landroid/content/Intent;)Landroid/content/ComponentName;",
        intent.object());
    qDebug() << "ConnectionManager: started KeepAliveService";
}

static void stopKeepAliveService()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) return;

    QJniObject className = QJniObject::fromString(
        "io.github.kulitorum.decenzapocket.KeepAliveService");
    QJniObject cls = QJniObject::callStaticObjectMethod(
        "java/lang/Class", "forName",
        "(Ljava/lang/String;)Ljava/lang/Class;",
        className.object<jstring>());

    QJniObject intent("android/content/Intent",
        "(Landroid/content/Context;Ljava/lang/Class;)V",
        context.object(), cls.object());

    context.callMethod<jboolean>("stopService",
        "(Landroid/content/Intent;)Z",
        intent.object());
    qDebug() << "ConnectionManager: stopped KeepAliveService";
}
#endif

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

    m_active = true;
    qDebug() << "ConnectionManager: starting discovery + remote in parallel";
    m_discovery->startSearch();
    m_remoteClient->connectToRelay();
}

void ConnectionManager::wake()
{
    qDebug() << "ConnectionManager: wake() mode:" << m_mode
             << "local:" << m_localClient->isConnected()
             << "remote:" << m_remoteClient->isConnected();
    if (m_localClient->isConnected()) {
        m_localClient->wake();
    } else if (m_remoteClient->isConnected()) {
        m_remoteClient->sendCommand(QStringLiteral("wake"));
    } else {
        qWarning() << "ConnectionManager: wake() called but no connection available";
    }
}

void ConnectionManager::sleep()
{
    qDebug() << "ConnectionManager: sleep() mode:" << m_mode
             << "local:" << m_localClient->isConnected()
             << "remote:" << m_remoteClient->isConnected();
    if (m_localClient->isConnected()) {
        m_localClient->sleep();
    } else if (m_remoteClient->isConnected()) {
        m_remoteClient->sendCommand(QStringLiteral("sleep"));
    } else {
        qWarning() << "ConnectionManager: sleep() called but no connection available";
    }
}

void ConnectionManager::disconnect()
{
    m_active = false;
    m_localClient->disconnect();
    m_remoteClient->disconnect();
    m_discovery->stopSearch();
    setMode(QStringLiteral("disconnected"));
    qDebug() << "ConnectionManager: disconnected, active=false";
}

// ---------------------------------------------------------------------------
// Local client slots
// ---------------------------------------------------------------------------

void ConnectionManager::onLocalStatusUpdated()
{
    if (!m_active) return;
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
    if (!m_active) return;
    qDebug() << "ConnectionManager: local connected changed:"
             << m_localClient->isConnected() << "current mode:" << m_mode;
    if (m_localClient->isConnected()) {
        setMode(QStringLiteral("local"));
    } else if (m_mode == QLatin1String("local")) {
        qDebug() << "ConnectionManager: local connection lost, falling back to remote";
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
    qDebug() << "ConnectionManager: remote connected changed:"
             << m_remoteClient->isConnected() << "current mode:" << m_mode;
    if (m_remoteClient->isConnected() && m_mode != QLatin1String("local")) {
        setMode(QStringLiteral("remote"));
    } else if (!m_remoteClient->isConnected() && m_mode == QLatin1String("remote")) {
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

    if (!m_active) {
        qDebug() << "ConnectionManager: ignoring discovery (not active)";
        return;
    }

    qDebug() << "ConnectionManager: local device found at" << serverUrl;
    m_remoteClient->disconnect(); // Local wins, stop remote
    m_localClient->connectToServer(serverUrl);
    setMode(QStringLiteral("local"));
}

void ConnectionManager::onDiscoveryFailed()
{
    if (!m_active) return;
    qDebug() << "ConnectionManager: local discovery failed, remote already connecting";
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ConnectionManager::setMode(const QString& mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        qDebug() << "ConnectionManager: mode changed to" << m_mode;
#ifdef Q_OS_ANDROID
        // Start foreground service when connected to keep WebSocket alive
        if (m_mode == QLatin1String("local") || m_mode == QLatin1String("remote")) {
            startKeepAliveService();
        } else {
            stopKeepAliveService();
        }
#endif
        emit modeChanged();
    }
}

void ConnectionManager::tryRemoteFallback()
{
    qDebug() << "ConnectionManager: attempting remote relay connection";
    m_remoteClient->connectToRelay();
}
