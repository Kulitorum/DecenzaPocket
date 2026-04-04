#include "network/remotecontrolclient.h"

#include <QWebSocket>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

RemoteControlClient::RemoteControlClient(QWebSocket* socket, QObject* parent)
    : QObject(parent)
    , m_socket(socket)
{
}

void RemoteControlClient::handleBinaryMessage(const QByteArray& data)
{
    if (data.isEmpty()) return;
    quint8 type = static_cast<quint8>(data[0]);

    if (type == 0x01) {
        processTileMessage(data);
    }
}

void RemoteControlClient::processTileMessage(const QByteArray& data)
{
    if (data.size() < 7) return;

    quint16 w = (static_cast<quint8>(data[1]) << 8) | static_cast<quint8>(data[2]);
    quint16 h = (static_cast<quint8>(data[3]) << 8) | static_cast<quint8>(data[4]);
    quint8 tileSize = static_cast<quint8>(data[5]);
    quint8 tileCount = static_cast<quint8>(data[6]);

    if (w != m_frameWidth || h != m_frameHeight) {
        m_frameWidth = w;
        m_frameHeight = h;
        m_frame = QImage(w, h, QImage::Format_RGB32);
        m_frame.fill(Qt::black);
        emit frameSizeChanged();
    }

    if (!m_active) {
        m_active = true;
        emit activeChanged();
    }

    int offset = 7;
    for (int i = 0; i < tileCount && offset + 4 <= data.size(); ++i) {
        quint8 tx = static_cast<quint8>(data[offset]);
        quint8 ty = static_cast<quint8>(data[offset + 1]);
        quint16 dataLen = (static_cast<quint8>(data[offset + 2]) << 8)
                        | static_cast<quint8>(data[offset + 3]);
        offset += 4;

        if (offset + dataLen > data.size()) break;

        QByteArray tileData = data.mid(offset, dataLen);
        offset += dataLen;

        QImage tile;
        tile.loadFromData(tileData, "WEBP");
        if (tile.isNull()) continue;

        // Convert tile to match frame format
        tile = tile.convertToFormat(QImage::Format_RGB32);

        int x = tx * tileSize;
        int y = ty * tileSize;

        // Paint tile onto frame
        for (int py = 0; py < tile.height() && (y + py) < m_frame.height(); ++py) {
            const uchar* src = tile.scanLine(py);
            uchar* dst = m_frame.scanLine(y + py) + x * 4;
            int copyWidth = qMin(tile.width(), m_frame.width() - x) * 4;
            memcpy(dst, src, copyWidth);
        }
    }

    emit frameUpdated();
}

void RemoteControlClient::sendTouch(int touchType, qreal normalizedX,
                                     qreal normalizedY, int pointId)
{
    // For move events, skip if position barely changed (prevents flooding relay)
    if (touchType == 1) { // move
        qreal dx = normalizedX - m_lastSentX;
        qreal dy = normalizedY - m_lastSentY;
        if (dx * dx + dy * dy < 0.0001) return; // ~1% movement threshold
    }

    m_lastSentX = normalizedX;
    m_lastSentY = normalizedY;

    // Reset tracking on release
    if (touchType == 2) {
        m_lastSentX = -1;
        m_lastSentY = -1;
    }

    quint16 x = static_cast<quint16>(qBound(0.0, normalizedX, 1.0) * 65535);
    quint16 y = static_cast<quint16>(qBound(0.0, normalizedY, 1.0) * 65535);
    sendTouchBinary(touchType, x, y, pointId);
}

void RemoteControlClient::sendTouchBinary(int touchType, quint16 x, quint16 y, int pointId)
{
    QByteArray msg(7, Qt::Uninitialized);
    msg[0] = 0x02;
    msg[1] = static_cast<char>(touchType);
    msg[2] = static_cast<char>((x >> 8) & 0xFF);
    msg[3] = static_cast<char>(x & 0xFF);
    msg[4] = static_cast<char>((y >> 8) & 0xFF);
    msg[5] = static_cast<char>(y & 0xFF);
    msg[6] = static_cast<char>(pointId);

    QJsonObject envelope;
    envelope["action"] = QStringLiteral("binary_relay");
    envelope["data"] = QString::fromLatin1(msg.toBase64());
    QByteArray jsonMsg = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    m_socket->sendTextMessage(QString::fromUtf8(jsonMsg));
}

// --- Image Provider ---

RemoteFrameProvider::RemoteFrameProvider(RemoteControlClient* client)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_client(client)
{
}

QImage RemoteFrameProvider::requestImage(const QString& id, QSize* size,
                                          const QSize& requestedSize)
{
    Q_UNUSED(id)
    Q_UNUSED(requestedSize)

    QImage frame = m_client->currentFrame();
    if (size) *size = frame.size();
    return frame;
}
