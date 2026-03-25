#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QTimer>

class Discovery : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool searching READ isSearching NOTIFY searchingChanged)
    Q_PROPERTY(QString foundDeviceName READ foundDeviceName NOTIFY deviceFound)
    Q_PROPERTY(QString foundServerUrl READ foundServerUrl NOTIFY deviceFound)

public:
    explicit Discovery(QObject* parent = nullptr);

    bool isSearching() const { return m_searching; }
    QString foundDeviceName() const { return m_foundDeviceName; }
    QString foundServerUrl() const { return m_foundServerUrl; }

    Q_INVOKABLE void startSearch();
    Q_INVOKABLE void stopSearch();

signals:
    void searchingChanged();
    void deviceFound(const QString& deviceName, const QString& serverUrl,
                     int port, bool secure);
    void searchFailed();

private slots:
    void onReadyRead();
    void onSearchTimeout();
    void sendDiscoveryBroadcast();

private:
    QUdpSocket* m_socket = nullptr;
    QTimer m_broadcastTimer;
    QTimer m_timeoutTimer;
    bool m_searching = false;
    QString m_foundDeviceName;
    QString m_foundServerUrl;

    static constexpr quint16 DISCOVERY_PORT = 8889;
};
