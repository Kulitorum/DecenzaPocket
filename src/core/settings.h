#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantMap>

class Settings : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString pairedDeviceId READ pairedDeviceId NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QString pairedDeviceName READ pairedDeviceName NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QString pairedServerUrl READ pairedServerUrl NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QString pairingToken READ pairingToken NOTIFY pairedDeviceChanged)
    Q_PROPERTY(bool isPaired READ isPaired NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QVariantMap themeColors READ themeColors NOTIFY themeChanged)
    Q_PROPERTY(QVariantMap themeFonts READ themeFonts NOTIFY themeChanged)
    Q_PROPERTY(bool themeAutoSync READ themeAutoSync WRITE setThemeAutoSync NOTIFY themeAutoSyncChanged)

public:
    explicit Settings(QObject* parent = nullptr);

    QString pairedDeviceId() const;
    QString pairedDeviceName() const;
    QString pairedServerUrl() const;
    QString pairingToken() const;
    bool isPaired() const;

    Q_INVOKABLE void setPairedDevice(const QString& deviceId, const QString& name,
                                      const QString& serverUrl, const QString& pairingToken);
    Q_INVOKABLE void clearPairedDevice();

    QVariantMap themeColors() const;
    QVariantMap themeFonts() const;
    void setThemeData(const QVariantMap& colors, const QVariantMap& fonts);

    bool themeAutoSync() const;
    void setThemeAutoSync(bool enabled);

    QString sessionCookie() const;
    void setSessionCookie(const QString& cookie);

signals:
    void pairedDeviceChanged();
    void themeChanged();
    void themeAutoSyncChanged();

private:
    QSettings m_settings;
};
