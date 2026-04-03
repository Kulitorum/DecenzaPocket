# Remote Control Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Stream the Decenza tablet screen to the Pocket phone app with touch input, using tile-based delta compression over the existing WebSocket relay.

**Architecture:** Three codebases change: relay server adds binary frame passthrough and new commands, tablet adds screen capture service, phone adds remote control UI. Binary WebSocket frames carry screen tiles (tablet→phone) and touch events (phone→tablet). JSON commands manage session lifecycle.

**Tech Stack:** Qt 6 (C++17/QML), AWS API Gateway WebSocket + Lambda (TypeScript), WebP image encoding, binary WebSocket frames.

**Codebases:**
- Tablet: `C:\CODE\Decenza`
- Phone: `C:\CODE\DecenzaPocket`
- Relay: `C:\CODE\decenza.coffee`

---

### Task 1: Relay — Add binary frame passthrough

**Files:**
- Modify: `C:\CODE\decenza.coffee\backend\lambdas\wsMessage.ts`
- Modify: `C:\CODE\decenza.coffee\backend\shared\broadcast.ts`

**Step 1: Add binary relay function to broadcast.ts**

Add this function to `backend/shared/broadcast.ts`:

```typescript
import { getConnection, getConnectionsByDeviceId } from './dynamo.js';

export async function relayBinary(senderConnectionId: string, binaryData: Buffer): Promise<{sent: number, failed: number}> {
  const conn = await getConnection(senderConnectionId);
  if (!conn?.device_id || !conn?.role) {
    return { sent: 0, failed: 0 };
  }

  const targetRole = conn.role === 'device' ? 'controller' : 'device';
  const targets = await getConnectionsByDeviceId(conn.device_id, targetRole);

  let sent = 0, failed = 0;
  await Promise.all(targets.map(async (target) => {
    const ok = await sendBinaryToConnection(target.connection_id, binaryData);
    if (ok) sent++; else failed++;
  }));

  return { sent, failed };
}
```

Add `sendBinaryToConnection` next to existing `sendToConnection`:

```typescript
export async function sendBinaryToConnection(connectionId: string, data: Buffer): Promise<boolean> {
  try {
    await client.send(new PostToConnectionCommand({
      ConnectionId: connectionId,
      Data: data,
    }));
    return true;
  } catch (err: any) {
    if (err.name === 'GoneException') {
      await deleteConnection(connectionId);
    }
    return false;
  }
}
```

**Step 2: Handle binary frames in wsMessage.ts**

Add binary detection at the top of the handler, before JSON parsing:

```typescript
// At the top of the handler, before JSON.parse:
if (event.isBase64Encoded) {
  const binaryData = Buffer.from(event.body || '', 'base64');
  const { sent, failed } = await relayBinary(connectionId, binaryData);
  console.log(`Binary relay: ${binaryData.length} bytes -> ${sent} sent, ${failed} failed`);
  return { statusCode: 200, body: 'OK' };
}
```

Import `relayBinary` at the top of the file.

**Step 3: Deploy and verify**

Run: `cd C:\CODE\decenza.coffee && npm run deploy`
Verify: Check CloudWatch logs for binary relay messages.

**Step 4: Commit**

```bash
cd C:\CODE\decenza.coffee
git add backend/shared/broadcast.ts backend/lambdas/wsMessage.ts
git commit -m "feat: add binary WebSocket frame passthrough for relay"
```

---

### Task 2: Relay — Add start_remote / stop_remote commands

**Files:**
- Modify: `C:\CODE\decenza.coffee\backend\shared\validate.ts`

**Step 1: Update command enum in validation schema**

In `validate.ts`, find the command schema:
```typescript
command: z.enum(['wake', 'sleep', 'status'])
```

Change to:
```typescript
command: z.enum(['wake', 'sleep', 'status', 'start_remote', 'stop_remote'])
```

Also add `.passthrough()` to the command schema if not already present (it is), so the optional `scale` field passes through.

**Step 2: Deploy**

Run: `cd C:\CODE\decenza.coffee && npm run deploy`

**Step 3: Commit**

```bash
cd C:\CODE\decenza.coffee
git add backend/shared/validate.ts
git commit -m "feat: add start_remote/stop_remote relay commands"
```

---

### Task 3: Tablet — Add ScreenCaptureService header

**Files:**
- Create: `C:\CODE\Decenza\src\network\screencaptureservice.h`

**Step 1: Write the header**

```cpp
#pragma once

#include <QObject>
#include <QImage>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>

class QQuickWindow;
class QWebSocket;

class ScreenCaptureService : public QObject {
    Q_OBJECT

public:
    explicit ScreenCaptureService(QQuickWindow* window, QWebSocket* socket,
                                   double scaleFactor = 0.5, QObject* parent = nullptr);
    ~ScreenCaptureService();

    void handleTouchEvent(const QByteArray& data);

private slots:
    void onFrameSwapped();
    void onHeartbeatTimer();

private:
    static constexpr int kTileSize = 64;
    static constexpr int kMaxMessageSize = 120000; // Stay under 128KB
    static constexpr int kHeartbeatMs = 30000;

    void captureAndSend();
    QByteArray encodeTile(const QImage& image, int x, int y, int w, int h);
    void sendTiles(const QVector<QPair<int,int>>& changedTiles, const QImage& frame);

    QQuickWindow* m_window;
    QWebSocket* m_socket;
    double m_scaleFactor;
    QImage m_previousFrame;
    QTimer m_heartbeatTimer;
    QElapsedTimer m_throttleTimer;
    qint64 m_bytesSentThisSecond = 0;
    bool m_captureScheduled = false;
    bool m_initialFrameSent = false;
};
```

**Step 2: Commit**

```bash
cd C:\CODE\Decenza
git add src/network/screencaptureservice.h
git commit -m "feat: add ScreenCaptureService header"
```

---

### Task 4: Tablet — Implement ScreenCaptureService

**Files:**
- Create: `C:\CODE\Decenza\src\network\screencaptureservice.cpp`
- Modify: `C:\CODE\Decenza\CMakeLists.txt` — add to SOURCES and HEADERS

**Step 1: Write the implementation**

```cpp
#include "network/screencaptureservice.h"

#include <QQuickWindow>
#include <QWebSocket>
#include <QBuffer>
#include <QDebug>
#include <QtGui/qpa/qwindowsysteminterface.h>

ScreenCaptureService::ScreenCaptureService(QQuickWindow* window, QWebSocket* socket,
                                           double scaleFactor, QObject* parent)
    : QObject(parent)
    , m_window(window)
    , m_socket(socket)
    , m_scaleFactor(qBound(0.1, scaleFactor, 1.0))
{
    connect(m_window, &QQuickWindow::frameSwapped,
            this, &ScreenCaptureService::onFrameSwapped);

    m_heartbeatTimer.setInterval(kHeartbeatMs);
    connect(&m_heartbeatTimer, &QTimer::timeout,
            this, &ScreenCaptureService::onHeartbeatTimer);
    m_heartbeatTimer.start();

    m_throttleTimer.start();

    qDebug() << "ScreenCaptureService: started, scale:" << m_scaleFactor;
}

ScreenCaptureService::~ScreenCaptureService()
{
    qDebug() << "ScreenCaptureService: stopped";
}

void ScreenCaptureService::onFrameSwapped()
{
    if (m_captureScheduled) return;
    m_captureScheduled = true;

    // Throttle: reset byte counter each second
    if (m_throttleTimer.elapsed() >= 1000) {
        m_bytesSentThisSecond = 0;
        m_throttleTimer.restart();
    }
    if (m_bytesSentThisSecond > 500000) return; // 500KB/s limit

    QMetaObject::invokeMethod(this, &ScreenCaptureService::captureAndSend,
                              Qt::QueuedConnection);
}

void ScreenCaptureService::onHeartbeatTimer()
{
    // Force a full capture even if nothing changed
    captureAndSend();
}

void ScreenCaptureService::captureAndSend()
{
    m_captureScheduled = false;

    QImage frame = m_window->grabWindow();
    if (frame.isNull()) return;

    // Scale down
    int scaledW = static_cast<int>(frame.width() * m_scaleFactor);
    int scaledH = static_cast<int>(frame.height() * m_scaleFactor);
    QImage scaled = frame.scaled(scaledW, scaledH, Qt::IgnoreAspectRatio,
                                  Qt::SmoothTransformation);
    scaled = scaled.convertToFormat(QImage::Format_RGB32);

    int cols = (scaled.width() + kTileSize - 1) / kTileSize;
    int rows = (scaled.height() + kTileSize - 1) / kTileSize;

    QVector<QPair<int,int>> changedTiles;

    for (int ty = 0; ty < rows; ++ty) {
        for (int tx = 0; tx < cols; ++tx) {
            int x = tx * kTileSize;
            int y = ty * kTileSize;
            int w = qMin(kTileSize, scaled.width() - x);
            int h = qMin(kTileSize, scaled.height() - y);

            bool changed = !m_initialFrameSent;

            if (!changed && !m_previousFrame.isNull()) {
                // Compare tile pixels
                for (int py = y; py < y + h && !changed; ++py) {
                    const QRgb* newLine = reinterpret_cast<const QRgb*>(scaled.scanLine(py));
                    const QRgb* oldLine = reinterpret_cast<const QRgb*>(m_previousFrame.scanLine(py));
                    for (int px = x; px < x + w; ++px) {
                        if (newLine[px] != oldLine[px]) {
                            changed = true;
                            break;
                        }
                    }
                }
            }

            if (changed) {
                changedTiles.append({tx, ty});
            }
        }
    }

    if (!changedTiles.isEmpty()) {
        sendTiles(changedTiles, scaled);
        m_heartbeatTimer.start(); // Reset heartbeat since we just sent
    }

    m_previousFrame = scaled;
    m_initialFrameSent = true;
}

QByteArray ScreenCaptureService::encodeTile(const QImage& image, int x, int y, int w, int h)
{
    QImage tile = image.copy(x, y, w, h);
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    tile.save(&buffer, "WEBP", 80); // Quality 80
    return data;
}

void ScreenCaptureService::sendTiles(const QVector<QPair<int,int>>& changedTiles,
                                      const QImage& frame)
{
    int idx = 0;
    while (idx < changedTiles.size()) {
        // Build a binary message with as many tiles as fit under kMaxMessageSize
        QByteArray msg;
        msg.reserve(kMaxMessageSize);

        // Header: type(1) + width(2) + height(2) + tileSize(1) + count(1) = 7 bytes
        msg.append(static_cast<char>(0x01)); // message type
        quint16 w = static_cast<quint16>(frame.width());
        quint16 h = static_cast<quint16>(frame.height());
        msg.append(static_cast<char>((w >> 8) & 0xFF));
        msg.append(static_cast<char>(w & 0xFF));
        msg.append(static_cast<char>((h >> 8) & 0xFF));
        msg.append(static_cast<char>(h & 0xFF));
        msg.append(static_cast<char>(kTileSize));

        int countPos = msg.size();
        msg.append(static_cast<char>(0)); // placeholder for tile count

        int tileCount = 0;

        while (idx < changedTiles.size()) {
            auto [tx, ty] = changedTiles[idx];
            int x = tx * kTileSize;
            int y = ty * kTileSize;
            int tw = qMin(kTileSize, frame.width() - x);
            int th = qMin(kTileSize, frame.height() - y);

            QByteArray tileData = encodeTile(frame, x, y, tw, th);

            // Check if adding this tile would exceed message size
            // tile header: tileX(1) + tileY(1) + dataLen(2) = 4 bytes
            if (msg.size() + 4 + tileData.size() > kMaxMessageSize && tileCount > 0) {
                break; // Send current batch, continue in next message
            }

            msg.append(static_cast<char>(tx));
            msg.append(static_cast<char>(ty));
            quint16 len = static_cast<quint16>(tileData.size());
            msg.append(static_cast<char>((len >> 8) & 0xFF));
            msg.append(static_cast<char>(len & 0xFF));
            msg.append(tileData);

            tileCount++;
            idx++;
        }

        msg[countPos] = static_cast<char>(tileCount);
        m_socket->sendBinaryMessage(msg);
        m_bytesSentThisSecond += msg.size();

        qDebug() << "ScreenCaptureService: sent" << tileCount << "tiles,"
                 << msg.size() << "bytes";
    }
}

void ScreenCaptureService::handleTouchEvent(const QByteArray& data)
{
    if (data.size() < 7) return;

    // Parse: type(1) + touchType(1) + x(2) + y(2) + pointId(1)
    quint8 touchType = static_cast<quint8>(data[1]);
    quint16 normX = (static_cast<quint8>(data[2]) << 8) | static_cast<quint8>(data[3]);
    quint16 normY = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[5]);
    quint8 pointId = static_cast<quint8>(data[6]);

    // Map normalized coords to window coords
    qreal x = (normX / 65535.0) * m_window->width();
    qreal y = (normY / 65535.0) * m_window->height();

    QPointF pos(x, y);

    QEvent::Type eventType;
    switch (touchType) {
    case 0: eventType = QEvent::MouseButtonPress; break;
    case 1: eventType = QEvent::MouseMove; break;
    case 2: eventType = QEvent::MouseButtonRelease; break;
    default: return;
    }

    Q_UNUSED(pointId) // TODO: multitouch support later

    // Inject as mouse events (simpler and more compatible than touch events)
    Qt::MouseButton button = (eventType == QEvent::MouseMove) ? Qt::NoButton : Qt::LeftButton;
    Qt::MouseButtons buttons = (eventType == QEvent::MouseButtonRelease) ? Qt::NoButton : Qt::LeftButton;

    QWindowSystemInterface::handleMouseEvent(
        m_window, pos, m_window->mapToGlobal(pos),
        buttons, button, eventType);
}
```

**Step 2: Add to CMakeLists.txt**

In `C:\CODE\Decenza\CMakeLists.txt`, add to SOURCES list:
```
src/network/screencaptureservice.cpp
```

Add to HEADERS list:
```
src/network/screencaptureservice.h
```

**Step 3: Commit**

```bash
cd C:\CODE\Decenza
git add src/network/screencaptureservice.cpp src/network/screencaptureservice.h CMakeLists.txt
git commit -m "feat: implement ScreenCaptureService with tile-based delta"
```

---

### Task 5: Tablet — Wire ScreenCaptureService into RelayClient

**Files:**
- Modify: `C:\CODE\Decenza\src\network\relayclient.h`
- Modify: `C:\CODE\Decenza\src\network\relayclient.cpp`

**Step 1: Add ScreenCaptureService support to RelayClient header**

Add includes and members:
```cpp
// In relayclient.h, add:
#include <memory>
class ScreenCaptureService;
class QQuickWindow;

// Add public method:
void setWindow(QQuickWindow* window);

// Add private slot:
void onBinaryMessageReceived(const QByteArray& data);

// Add private members:
QQuickWindow* m_window = nullptr;
std::unique_ptr<ScreenCaptureService> m_captureService;
```

**Step 2: Implement in RelayClient**

In constructor, connect binary message signal:
```cpp
connect(&m_socket, &QWebSocket::binaryMessageReceived,
        this, &RelayClient::onBinaryMessageReceived);
```

Add `setWindow`:
```cpp
void RelayClient::setWindow(QQuickWindow* window)
{
    m_window = window;
}
```

Add binary handler:
```cpp
void RelayClient::onBinaryMessageReceived(const QByteArray& data)
{
    if (data.isEmpty()) return;
    quint8 type = static_cast<quint8>(data[0]);

    if (type == 0x02 && m_captureService) {
        // Touch event from controller
        m_captureService->handleTouchEvent(data);
    }
}
```

Update `handleCommand` to handle start_remote/stop_remote:
```cpp
// In handleCommand(), add:
} else if (command == "start_remote") {
    if (m_window && !m_captureService) {
        double scale = 0.5; // Could parse from command data later
        m_captureService = std::make_unique<ScreenCaptureService>(
            m_window, &m_socket, scale);
    }
} else if (command == "stop_remote") {
    m_captureService.reset();
}
```

Also reset capture service on disconnect:
```cpp
// In onDisconnected(), add:
m_captureService.reset();
```

**Step 3: Wire window in main.cpp**

In `C:\CODE\Decenza\src\main.cpp`, after the engine loads and root objects are available:
```cpp
// After engine.load(), find the QQuickWindow:
if (!engine.rootObjects().isEmpty()) {
    QQuickWindow* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
        relayClient.setWindow(window);
    }
}
```

**Step 4: Commit**

```bash
cd C:\CODE\Decenza
git add src/network/relayclient.h src/network/relayclient.cpp src/main.cpp
git commit -m "feat: wire ScreenCaptureService into RelayClient"
```

---

### Task 6: Phone — Add RemoteControlClient C++ class

**Files:**
- Create: `C:\CODE\DecenzaPocket\src\network\remotecontrolclient.h`
- Create: `C:\CODE\DecenzaPocket\src\network\remotecontrolclient.cpp`

**Step 1: Write the header**

```cpp
#pragma once

#include <QObject>
#include <QImage>
#include <QQuickImageProvider>

class QWebSocket;
class Settings;

class RemoteControlClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY frameSizeChanged)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY frameSizeChanged)

public:
    explicit RemoteControlClient(QWebSocket* socket, QObject* parent = nullptr);

    bool isActive() const { return m_active; }
    int frameWidth() const { return m_frameWidth; }
    int frameHeight() const { return m_frameHeight; }

    Q_INVOKABLE void sendTouch(int touchType, qreal normalizedX,
                                qreal normalizedY, int pointId = 0);

    void handleBinaryMessage(const QByteArray& data);
    QImage currentFrame() const { return m_frame; }

signals:
    void activeChanged();
    void frameSizeChanged();
    void frameUpdated();

private:
    void processTileMessage(const QByteArray& data);

    QWebSocket* m_socket;
    QImage m_frame;
    int m_frameWidth = 0;
    int m_frameHeight = 0;
    bool m_active = false;
};

class RemoteFrameProvider : public QQuickImageProvider {
public:
    explicit RemoteFrameProvider(RemoteControlClient* client);
    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

private:
    RemoteControlClient* m_client;
};
```

**Step 2: Write the implementation**

```cpp
#include "network/remotecontrolclient.h"

#include <QWebSocket>
#include <QBuffer>
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
    quint16 x = static_cast<quint16>(qBound(0.0, normalizedX, 1.0) * 65535);
    quint16 y = static_cast<quint16>(qBound(0.0, normalizedY, 1.0) * 65535);

    QByteArray msg(7, Qt::Uninitialized);
    msg[0] = 0x02; // touch message type
    msg[1] = static_cast<char>(touchType);
    msg[2] = static_cast<char>((x >> 8) & 0xFF);
    msg[3] = static_cast<char>(x & 0xFF);
    msg[4] = static_cast<char>((y >> 8) & 0xFF);
    msg[5] = static_cast<char>(y & 0xFF);
    msg[6] = static_cast<char>(pointId);

    m_socket->sendBinaryMessage(msg);
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
```

**Step 3: Commit**

```bash
cd C:\CODE\DecenzaPocket
git add src/network/remotecontrolclient.h src/network/remotecontrolclient.cpp
git commit -m "feat: add RemoteControlClient with tile compositor"
```

---

### Task 7: Phone — Add RemoteControlPage QML

**Files:**
- Create: `C:\CODE\DecenzaPocket\qml\RemoteControlPage.qml`

**Step 1: Write the QML page**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "black"

    signal back()

    property bool connected: RemoteControl.active

    // Remote screen display
    Image {
        id: remoteScreen
        anchors.centerIn: parent
        width: {
            if (RemoteControl.frameWidth === 0) return 0
            var scaleX = parent.width / RemoteControl.frameWidth
            var scaleY = parent.height / RemoteControl.frameHeight
            var s = Math.min(scaleX, scaleY)
            return RemoteControl.frameWidth * s
        }
        height: {
            if (RemoteControl.frameHeight === 0) return 0
            var scaleX = parent.width / RemoteControl.frameWidth
            var scaleY = parent.height / RemoteControl.frameHeight
            var s = Math.min(scaleX, scaleY)
            return RemoteControl.frameHeight * s
        }
        cache: false
        source: "image://remoteframe/frame"

        // Force refresh when frame updates
        property int frameCounter: 0
        Connections {
            target: RemoteControl
            function onFrameUpdated() {
                remoteScreen.frameCounter++
                remoteScreen.source = "image://remoteframe/frame?" + remoteScreen.frameCounter
            }
        }

        // Touch handling
        MultiPointTouchArea {
            anchors.fill: parent
            mouseEnabled: true
            minimumTouchPoints: 1
            maximumTouchPoints: 5

            onPressed: function(touchPoints) {
                for (var i = 0; i < touchPoints.length; i++) {
                    var tp = touchPoints[i]
                    RemoteControl.sendTouch(0, tp.x / width, tp.y / height, tp.pointId)
                }
            }
            onUpdated: function(touchPoints) {
                for (var i = 0; i < touchPoints.length; i++) {
                    var tp = touchPoints[i]
                    RemoteControl.sendTouch(1, tp.x / width, tp.y / height, tp.pointId)
                }
            }
            onReleased: function(touchPoints) {
                for (var i = 0; i < touchPoints.length; i++) {
                    var tp = touchPoints[i]
                    RemoteControl.sendTouch(2, tp.x / width, tp.y / height, tp.pointId)
                }
            }
        }
    }

    // Connecting overlay
    ColumnLayout {
        anchors.centerIn: parent
        visible: !root.connected
        spacing: Theme.spacingMedium

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: !root.connected
            palette.dark: Theme.primaryColor
        }
        Text {
            text: "Connecting to tablet..."
            color: Theme.textSecondaryColor
            font.pixelSize: Theme.bodySize
            Layout.alignment: Qt.AlignHCenter
        }
    }

    // Back button (floating)
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Theme.spacingMedium
        width: 40; height: 40
        radius: 20
        color: Theme.surfaceColor
        opacity: 0.7

        Text {
            anchors.centerIn: parent
            text: "\u2190"
            color: Theme.textColor
            font.pixelSize: Theme.titleSize
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.back()
        }
    }
}
```

**Step 2: Commit**

```bash
cd C:\CODE\DecenzaPocket
git add qml/RemoteControlPage.qml
git commit -m "feat: add RemoteControlPage QML with touch forwarding"
```

---

### Task 8: Phone — Wire everything together

**Files:**
- Modify: `C:\CODE\DecenzaPocket\CMakeLists.txt`
- Modify: `C:\CODE\DecenzaPocket\src\main.cpp`
- Modify: `C:\CODE\DecenzaPocket\src\network\relayclient.h`
- Modify: `C:\CODE\DecenzaPocket\src\network\relayclient.cpp`
- Modify: `C:\CODE\DecenzaPocket\qml\Main.qml`

**Step 1: Update CMakeLists.txt**

Add to SOURCES list:
```
src/network/remotecontrolclient.h
src/network/remotecontrolclient.cpp
```

Add to QML resources:
```
qml/RemoteControlPage.qml
```

**Step 2: Add binary message handling to RelayClient**

In `relayclient.h`, add signal:
```cpp
void binaryMessageReceived(const QByteArray& data);
```

In `relayclient.cpp` constructor, add:
```cpp
connect(&m_socket, &QWebSocket::binaryMessageReceived,
        this, &RelayClient::binaryMessageReceived);
```

**Step 3: Wire up in main.cpp**

```cpp
// Add includes:
#include "network/remotecontrolclient.h"

// After RelayClient creation:
RemoteControlClient remoteControl(&remoteClient.socket());
// Wait -- RelayClient doesn't expose socket. Instead, connect the signal:
RemoteControlClient remoteControl;

// Connect binary messages from relay to remote control client
QObject::connect(&remoteClient, &RelayClient::binaryMessageReceived,
                 &remoteControl, &RemoteControlClient::handleBinaryMessage);

// Register image provider
RemoteFrameProvider* frameProvider = new RemoteFrameProvider(&remoteControl);
engine.addImageProvider("remoteframe", frameProvider);

// Expose to QML
engine.rootContext()->setContextProperty("RemoteControl", &remoteControl);
```

Update `RemoteControlClient` constructor — it needs the socket pointer for sending binary touch events. Since RelayClient doesn't expose the socket, add a method to RelayClient:

In `relayclient.h`:
```cpp
QWebSocket* socket() { return &m_socket; }
```

Then in main.cpp:
```cpp
RemoteControlClient remoteControl(remoteClient.socket());
```

**Step 4: Add Remote Control button and navigation to Main.qml**

Add a "Remote Control" button to the status view, and add the remoteControlView component:

```qml
// Add Component for remote control view:
Component {
    id: remoteControlView
    RemoteControlPage {
        onBack: {
            Connection.sendCommand("stop_remote")
            stackView.pop()
        }
    }
}
```

Add button in the statusView ColumnLayout (between PowerButton and ConnectionIndicator):

```qml
// Remote control button
Button {
    Layout.fillWidth: true
    Layout.preferredHeight: 48
    enabled: Connection.mode !== "disconnected"
    onClicked: {
        Connection.sendCommand("start_remote")
        stackView.push(remoteControlView)
    }
    background: Rectangle {
        color: parent.enabled ? Theme.surfaceColor : Theme.surfaceColor
        radius: Theme.cardRadius / 2
        border.color: parent.enabled ? Theme.primaryColor : Theme.borderColor
        border.width: 1
    }
    contentItem: Text {
        text: "Remote Control"
        color: parent.enabled ? Theme.primaryColor : Theme.textSecondaryColor
        font.pixelSize: Theme.bodySize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
```

**Step 5: Add sendCommand to ConnectionManager**

The `Connection.sendCommand()` needs to be exposed. In `connectionmanager.h`:
```cpp
Q_INVOKABLE void sendCommand(const QString& command);
```

In `connectionmanager.cpp`:
```cpp
void ConnectionManager::sendCommand(const QString& command)
{
    if (m_remoteClient->isConnected()) {
        m_remoteClient->sendCommand(command);
    }
}
```

**Step 6: Commit**

```bash
cd C:\CODE\DecenzaPocket
git add CMakeLists.txt src/main.cpp src/network/relayclient.h src/network/relayclient.cpp src/network/connectionmanager.h src/network/connectionmanager.cpp qml/Main.qml
git commit -m "feat: wire remote control into Pocket app"
```

---

### Task 9: Deploy and test end-to-end

**Step 1: Deploy relay server**

```bash
cd C:\CODE\decenza.coffee
npm run deploy
```

**Step 2: Build and deploy tablet app**

Build in Qt Creator. Deploy to tablet.

**Step 3: Build and deploy phone app**

Build in Qt Creator. Deploy to phone.

**Step 4: Test**

1. Open Pocket app, verify relay connects
2. Tap "Remote Control" button
3. Verify tablet log shows `ScreenCaptureService: started`
4. Verify phone shows tablet screen
5. Tap on the phone screen, verify tablet responds
6. Press back, verify capture stops

**Step 5: Commit any fixes**

```bash
git add -A && git commit -m "fix: end-to-end remote control fixes"
```
