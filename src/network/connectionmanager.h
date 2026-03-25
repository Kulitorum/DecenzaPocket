#pragma once

#include <QObject>

class Discovery;
class DecenzaClient;
class RelayClient;
class Settings;

class ConnectionManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(QString machineState READ machineState NOTIFY statusChanged)
    Q_PROPERTY(QString machinePhase READ machinePhase NOTIFY statusChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY statusChanged)
    Q_PROPERTY(double waterLevelMl READ waterLevelMl NOTIFY statusChanged)
    Q_PROPERTY(bool isHeating READ isHeating NOTIFY statusChanged)
    Q_PROPERTY(bool isReady READ isReady NOTIFY statusChanged)
    Q_PROPERTY(bool isAwake READ isAwake NOTIFY statusChanged)

public:
    explicit ConnectionManager(Settings* settings, Discovery* discovery,
                                DecenzaClient* localClient, RelayClient* remoteClient,
                                QObject* parent = nullptr);

    QString mode() const { return m_mode; }
    QString machineState() const { return m_machineState; }
    QString machinePhase() const { return m_machinePhase; }
    double temperature() const { return m_temperature; }
    double waterLevelMl() const { return m_waterLevelMl; }
    bool isHeating() const { return m_isHeating; }
    bool isReady() const { return m_isReady; }
    bool isAwake() const { return m_isAwake; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void wake();
    Q_INVOKABLE void sleep();
    Q_INVOKABLE void disconnect();

signals:
    void modeChanged();
    void statusChanged();
    void readyNotification();
    void loginRequired();

private slots:
    void onLocalStatusUpdated();
    void onLocalConnectedChanged();
    void onLocalLoginRequired();
    void onRemoteStatusReceived(const QString& state, const QString& phase,
                                 double temperature, double waterLevelMl,
                                 bool isHeating, bool isReady, bool isAwake);
    void onRemoteConnectedChanged();
    void onDiscoveryFound(const QString& deviceName, const QString& serverUrl,
                           int port, bool secure);
    void onDiscoveryFailed();

private:
    void setMode(const QString& mode);
    void tryRemoteFallback();

    Settings* m_settings;
    Discovery* m_discovery;
    DecenzaClient* m_localClient;
    RelayClient* m_remoteClient;
    QString m_mode = "disconnected";

    // Unified status (updated from whichever client is active)
    QString m_machineState = "Disconnected";
    QString m_machinePhase;
    double m_temperature = 0;
    double m_waterLevelMl = 0;
    bool m_isHeating = false;
    bool m_isReady = false;
    bool m_isAwake = false;
};
