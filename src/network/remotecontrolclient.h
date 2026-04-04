#pragma once

#include <QObject>
#include <QImage>
#include <QQuickImageProvider>

class QWebSocket;

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
    void sendTouchBinary(int touchType, quint16 x, quint16 y, int pointId);

    QWebSocket* m_socket;
    QImage m_frame;
    int m_frameWidth = 0;
    int m_frameHeight = 0;
    bool m_active = false;
    qreal m_lastSentX = -1;
    qreal m_lastSentY = -1;
};

class RemoteFrameProvider : public QQuickImageProvider {
public:
    explicit RemoteFrameProvider(RemoteControlClient* client);
    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

private:
    RemoteControlClient* m_client;
};
