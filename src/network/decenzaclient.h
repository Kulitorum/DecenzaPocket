#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QJsonObject>
#include <functional>

class Settings;

class DecenzaClient : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(bool polling READ isPolling NOTIFY pollingChanged)
    Q_PROPERTY(QString machineState READ machineState NOTIFY statusUpdated)
    Q_PROPERTY(QString machinePhase READ machinePhase NOTIFY statusUpdated)
    Q_PROPERTY(double temperature READ temperature NOTIFY statusUpdated)
    Q_PROPERTY(double waterLevelMl READ waterLevelMl NOTIFY statusUpdated)
    Q_PROPERTY(bool isHeating READ isHeating NOTIFY statusUpdated)
    Q_PROPERTY(bool isReady READ isReady NOTIFY statusUpdated)
    Q_PROPERTY(bool isAwake READ isAwake NOTIFY statusUpdated)

public:
    explicit DecenzaClient(Settings* settings, QObject* parent = nullptr);

    bool isConnected() const { return m_connected; }
    bool isPolling() const { return m_pollTimer.isActive(); }

    QString machineState() const { return m_machineState; }
    QString machinePhase() const { return m_machinePhase; }
    double temperature() const { return m_temperature; }
    double waterLevelMl() const { return m_waterLevelMl; }
    bool isHeating() const { return m_isHeating; }
    bool isReady() const { return m_isReady; }
    bool isAwake() const { return m_isAwake; }

    Q_INVOKABLE void connectToServer(const QString& serverUrl);
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void login(const QString& totpCode);
    Q_INVOKABLE void wake();
    Q_INVOKABLE void sleep();
    Q_INVOKABLE void fetchTheme();
    Q_INVOKABLE void pair(const QString& pairingToken);

signals:
    void connectedChanged();
    void pollingChanged();
    void statusUpdated();
    void loginRequired();
    void loginSuccess();
    void loginFailed(const QString& error);
    void themeReceived(const QVariantMap& colors, const QVariantMap& fonts);
    void pairingComplete(const QString& deviceId, const QString& deviceName);
    void commandSent(const QString& command, bool success);
    void connectionError(const QString& error);
    void readyNotification();

private slots:
    void pollStatus();

private:
    void get(const QString& path, std::function<void(int statusCode, const QJsonObject&)> callback);
    void post(const QString& path, const QByteArray& body,
              std::function<void(int statusCode, const QJsonObject&)> callback);

    Settings* m_settings;
    QNetworkAccessManager* m_nam;
    QTimer m_pollTimer;
    QString m_serverUrl;
    bool m_connected = false;

    // Cached status
    QString m_machineState;
    QString m_machinePhase;
    double m_temperature = 0;
    double m_waterLevelMl = 0;
    bool m_isHeating = false;
    bool m_isReady = false;
    bool m_isAwake = false;
    QString m_previousPhase; // For ready notification edge detection
};
