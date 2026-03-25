#pragma once

#include <QObject>
#include <QWebSocket>
#include <QTimer>

class Settings;

class RelayClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)

public:
    explicit RelayClient(Settings* settings, QObject* parent = nullptr);

    bool isConnected() const;

    Q_INVOKABLE void connectToRelay();
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void sendCommand(const QString& command);

signals:
    void connectedChanged();
    void statusReceived(const QString& state, const QString& phase,
                        double temperature, double waterLevelMl,
                        bool isHeating, bool isReady, bool isAwake);
    void commandResult(const QString& commandId, bool success);
    void readyNotification();
    void connectionError(const QString& error);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onReconnectTimer();
    void onPingTimer();

private:
    QWebSocket m_socket;
    QTimer m_reconnectTimer;
    QTimer m_pingTimer;
    Settings* m_settings;
    int m_reconnectAttempts = 0;
    bool m_wasHeating = false;
};
