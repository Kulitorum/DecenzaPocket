#include "network/relayclient.h"
#include "core/settings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QDebug>

RelayClient::RelayClient(Settings* settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
{
    connect(&m_socket, &QWebSocket::connected,
            this, &RelayClient::onConnected);
    connect(&m_socket, &QWebSocket::disconnected,
            this, &RelayClient::onDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived,
            this, &RelayClient::onTextMessageReceived);
    connect(&m_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, [](QAbstractSocket::SocketError error) {
        qWarning() << "RelayClient: socket error:" << error;
    });
    connect(&m_socket, &QWebSocket::sslErrors, this, [](const QList<QSslError>& errors) {
        for (const auto& e : errors)
            qWarning() << "RelayClient: SSL error:" << e.errorString();
    });
    connect(&m_socket, &QWebSocket::binaryMessageReceived,
            this, &RelayClient::binaryMessageReceived);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &RelayClient::onReconnectTimer);

    m_pingTimer.setInterval(5 * 60 * 1000); // 5 minutes
    connect(&m_pingTimer, &QTimer::timeout,
            this, &RelayClient::onPingTimer);
}

bool RelayClient::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void RelayClient::connectToRelay()
{
    QString deviceId = m_settings->pairedDeviceId();
    if (deviceId.isEmpty()) {
        return; // Not paired
    }

    QString pairingToken = m_settings->pairingToken();
    QString url = QStringLiteral("wss://ws.decenza.coffee?device_id=%1&role=controller")
                      .arg(deviceId);

    qDebug() << "RelayClient: connecting to" << url
             << "deviceId:" << deviceId
             << "hasToken:" << !pairingToken.isEmpty();
    m_socket.open(QUrl(url));
}

void RelayClient::disconnect()
{
    m_reconnectTimer.stop();
    m_pingTimer.stop();
    m_reconnectAttempts = 0;
    m_socket.close();
}

void RelayClient::sendCommand(const QString& command)
{
    qDebug() << "RelayClient: sending command:" << command
             << "connected:" << isConnected();

    QJsonObject msg;
    msg["action"] = QStringLiteral("command");
    msg["device_id"] = m_settings->pairedDeviceId();
    msg["pairing_token"] = m_settings->pairingToken();
    msg["command"] = command;

    QString payload = QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    qDebug() << "RelayClient: >> " << payload;
    m_socket.sendTextMessage(payload);
}

// ---------------------------------------------------------------------------
// WebSocket event handlers
// ---------------------------------------------------------------------------

void RelayClient::onConnected()
{
    qDebug() << "RelayClient: connected, sending register message";

    QJsonObject msg;
    msg["action"] = QStringLiteral("register");
    msg["device_id"] = m_settings->pairedDeviceId();
    msg["role"] = QStringLiteral("controller");
    msg["pairing_token"] = m_settings->pairingToken();

    QString payload = QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    m_socket.sendTextMessage(payload);

    m_pingTimer.start();
    m_reconnectAttempts = 0;

    emit connectedChanged();
}

void RelayClient::onDisconnected()
{
    qDebug() << "RelayClient: disconnected";

    m_pingTimer.stop();

    // Exponential backoff: 5s, 10s, 20s, 40s, max 60s
    int delaySec = qMin(5 * (1 << m_reconnectAttempts), 60);
    m_reconnectAttempts++;

    qDebug() << "RelayClient: will reconnect in" << delaySec << "seconds";
    m_reconnectTimer.start(delaySec * 1000);

    emit connectedChanged();
}

void RelayClient::onTextMessageReceived(const QString& message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "RelayClient: received non-object message:" << message;
        return;
    }

    QJsonObject json = doc.object();
    QString type = json.value("type").toString();
    qDebug() << "RelayClient: << type:" << type;

    if (type == QLatin1String("relay_status")) {
        QString state = json.value("state").toString();
        QString phase = json.value("phase").toString();
        double temperature = json.value("temperature").toDouble();
        double goalTemperature = json.value("goalTemperature").toDouble();
        double waterLevelMl = json.value("waterLevelMl").toDouble();
        bool heating = json.value("isHeating").toBool();
        bool ready = json.value("isReady").toBool();
        bool awake = json.value("isAwake").toBool();

        // Override phase: DE1 reports "Ready" before reaching target temp
        if (goalTemperature > 0 && temperature > 0 &&
            (phase == "Ready" || phase == "Idle") &&
            temperature < goalTemperature - 0.5) {
            phase = QStringLiteral("Heating");
        }

        emit statusReceived(state, phase, temperature, waterLevelMl,
                            heating, ready, awake);

        // Ready notification: fire when phase changes from Heating to Ready
        if (m_previousPhase == "Heating" && phase == "Ready") {
            qDebug() << "RelayClient: Heating->Ready transition detected, firing notification";
            emit readyNotification();
        }
        m_previousPhase = phase;

    } else if (type == QLatin1String("relay_response")) {
        QString commandId = json.value("commandId").toString();
        bool success = json.value("success").toBool();
        emit commandResult(commandId, success);

    } else if (type == QLatin1String("registered")) {
        qDebug() << "RelayClient: registered successfully";

    } else if (type == QLatin1String("error")) {
        QString error = json.value("error").toString();
        qWarning() << "RelayClient: error from relay:" << error;
        emit connectionError(error);

    } else if (type == QLatin1String("pong")) {
        // Heartbeat response, nothing to do

    } else if (type == QLatin1String("binary_relay")) {
        // Decode base64 data and forward as binary
        QByteArray binaryData = QByteArray::fromBase64(json.value("data").toString().toLatin1());
        emit binaryMessageReceived(binaryData);

    } else {
        qDebug() << "RelayClient: unknown message type:" << type;
    }
}

void RelayClient::onReconnectTimer()
{
    qDebug() << "RelayClient: attempting reconnect";
    connectToRelay();
}

void RelayClient::onPingTimer()
{
    QJsonObject msg;
    msg["action"] = QStringLiteral("ping");

    QString payload = QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    m_socket.sendTextMessage(payload);
}
