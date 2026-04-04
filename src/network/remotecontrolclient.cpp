#include "network/remotecontrolclient.h"

#include <QWebSocket>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImageReader>
#include <QDebug>

RemoteControlClient::RemoteControlClient(QWebSocket* socket, QObject* parent)
    : QObject(parent)
    , m_socket(socket)
{
}

QString RemoteControlClient::debugInfo() const
{
    return QStringLiteral("msgs: %1 | tiles ok/fail: %2/%3\nformats: %4\n%5")
        .arg(m_messagesReceived)
        .arg(m_tilesDecoded)
        .arg(m_tilesFailed)
        .arg(m_supportedFormats.isEmpty() ? "not yet loaded" : m_supportedFormats)
        .arg(m_lastTileError);
}

void RemoteControlClient::handleBinaryMessage(const QByteArray& data)
{
    if (data.isEmpty()) {
        qDebug() << "RemoteControl: received empty binary message";
        return;
    }
    quint8 type = static_cast<quint8>(data[0]);
    m_messagesReceived++;

    if (m_messagesReceived <= 3 || m_messagesReceived % 100 == 0) {
        qDebug() << "RemoteControl: binary msg #" << m_messagesReceived
                 << "type:" << Qt::hex << type << Qt::dec
                 << "size:" << data.size() << "bytes";
    }

    if (type == 0x01) {
        processTileMessage(data);
    } else {
        qDebug() << "RemoteControl: unknown binary type:" << Qt::hex << type;
    }
}

void RemoteControlClient::processTileMessage(const QByteArray& data)
{
    if (data.size() < 7) {
        qDebug() << "RemoteControl: tile msg too short:" << data.size();
        return;
    }

    quint16 w = (static_cast<quint8>(data[1]) << 8) | static_cast<quint8>(data[2]);
    quint16 h = (static_cast<quint8>(data[3]) << 8) | static_cast<quint8>(data[4]);
    quint8 tileSize = static_cast<quint8>(data[5]);
    quint8 tileCount = static_cast<quint8>(data[6]);

    if (w != m_frameWidth || h != m_frameHeight) {
        m_frameWidth = w;
        m_frameHeight = h;
        m_frame = QImage(w, h, QImage::Format_RGB32);
        m_frame.fill(Qt::black);
        qDebug() << "RemoteControl: frame initialized" << w << "x" << h
                 << "tileSize:" << tileSize;

        // Log supported image formats once
        auto fmts = QImageReader::supportedImageFormats();
        QStringList fmtList;
        for (const auto& f : fmts) fmtList << QString::fromLatin1(f);
        m_supportedFormats = fmtList.join(", ");
        qDebug() << "RemoteControl: supported formats:" << m_supportedFormats;
        emit frameSizeChanged();
    }

    if (!m_active) {
        m_active = true;
        qDebug() << "RemoteControl: active = true";
        emit activeChanged();
    }

    int tilesDecoded = 0;
    int tilesFailed = 0;
    int offset = 7;
    for (int i = 0; i < tileCount && offset + 4 <= data.size(); ++i) {
        quint8 tx = static_cast<quint8>(data[offset]);
        quint8 ty = static_cast<quint8>(data[offset + 1]);
        quint16 dataLen = (static_cast<quint8>(data[offset + 2]) << 8)
                        | static_cast<quint8>(data[offset + 3]);
        offset += 4;

        if (offset + dataLen > data.size()) {
            qDebug() << "RemoteControl: tile" << i << "truncated, need" << dataLen
                     << "have" << (data.size() - offset);
            break;
        }

        QByteArray tileData = data.mid(offset, dataLen);
        offset += dataLen;

        QImage tile;
        tile.loadFromData(tileData, "WEBP");
        if (tile.isNull()) {
            // Try without format hint (auto-detect)
            tile.loadFromData(tileData);
            if (tile.isNull()) {
                tilesFailed++;
                if (tilesFailed <= 3) {
                    QByteArray header = tileData.left(16).toHex(' ');
                    m_lastTileError = QStringLiteral("FAIL sz:%1 hdr:%2")
                        .arg(dataLen)
                        .arg(QString::fromLatin1(header));
                    qDebug() << "RemoteControl: tile decode FAILED, size:" << dataLen
                             << "header:" << header;
                }
                continue;
            } else {
                if (tilesDecoded == 0)
                    qDebug() << "RemoteControl: tile decoded via auto-detect, not WEBP";
            }
        }

        // Convert tile to match frame format
        tile = tile.convertToFormat(QImage::Format_RGB32);
        tilesDecoded++;

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

    m_tilesTotal += tileCount;
    m_tilesDecoded += tilesDecoded;
    m_tilesFailed += tilesFailed;

    if (m_messagesReceived <= 3 || m_messagesReceived % 100 == 0) {
        qDebug() << "RemoteControl: tiles this msg:" << tilesDecoded << "/" << tileCount
                 << "total decoded/failed:" << m_tilesDecoded << "/" << m_tilesFailed;
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
