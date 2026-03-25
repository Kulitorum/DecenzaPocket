#include "network/discovery.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>

Discovery::Discovery(QObject* parent)
    : QObject(parent)
{
    m_broadcastTimer.setInterval(1000);
    connect(&m_broadcastTimer, &QTimer::timeout,
            this, &Discovery::sendDiscoveryBroadcast);

    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(10000);
    connect(&m_timeoutTimer, &QTimer::timeout,
            this, &Discovery::onSearchTimeout);
}

void Discovery::startSearch()
{
    if (m_searching)
        return;

    m_foundDeviceName.clear();
    m_foundServerUrl.clear();

    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::AnyIPv4, 0)) {
        delete m_socket;
        m_socket = nullptr;
        emit searchFailed();
        return;
    }

    connect(m_socket, &QUdpSocket::readyRead,
            this, &Discovery::onReadyRead);

    m_searching = true;
    emit searchingChanged();

    sendDiscoveryBroadcast();
    m_broadcastTimer.start();
    m_timeoutTimer.start();
}

void Discovery::stopSearch()
{
    m_broadcastTimer.stop();
    m_timeoutTimer.stop();

    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    if (m_searching) {
        m_searching = false;
        emit searchingChanged();
    }
}

void Discovery::sendDiscoveryBroadcast()
{
    if (!m_socket)
        return;

    const QByteArray datagram = QByteArrayLiteral("DECENZA_DISCOVER");

    // Send to broadcast address on each IPv4 interface
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp))
            continue;
        if (!(iface.flags() & QNetworkInterface::IsRunning))
            continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack)
            continue;

        const auto entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;

            QHostAddress broadcast = entry.broadcast();
            if (!broadcast.isNull()) {
                m_socket->writeDatagram(datagram, broadcast, DISCOVERY_PORT);
            }
        }
    }

    // Fallback: send to 255.255.255.255
    m_socket->writeDatagram(datagram, QHostAddress::Broadcast, DISCOVERY_PORT);
}

void Discovery::onReadyRead()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        m_socket->readDatagram(data.data(), data.size());

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError)
            continue;

        QJsonObject obj = doc.object();
        if (obj.value("type").toString() != QLatin1String("DECENZA_SERVER"))
            continue;

        QString deviceName = obj.value("deviceName").toString();
        QString serverUrl = obj.value("serverUrl").toString();
        int port = obj.value("port").toInt();
        bool secure = obj.value("secure").toBool();

        if (deviceName.isEmpty() || serverUrl.isEmpty())
            continue;

        m_foundDeviceName = deviceName;
        m_foundServerUrl = serverUrl;

        stopSearch();
        emit deviceFound(deviceName, serverUrl, port, secure);
        return;
    }
}

void Discovery::onSearchTimeout()
{
    stopSearch();
    emit searchFailed();
}
