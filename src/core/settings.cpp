#include "settings.h"
#include "nativebridge.h"
#include <QDebug>

Settings::Settings(QObject* parent)
    : QObject(parent)
    , m_settings("Decenza", "DecenzaPocket")
{
}

QString Settings::pairedDeviceId() const { return m_settings.value("device/id").toString(); }
QString Settings::pairedDeviceName() const { return m_settings.value("device/name").toString(); }
QString Settings::pairedServerUrl() const { return m_settings.value("device/serverUrl").toString(); }
QString Settings::pairingToken() const { return m_settings.value("device/pairingToken").toString(); }
bool Settings::isPaired() const { return !pairedDeviceId().isEmpty(); }

void Settings::setPairedDevice(const QString& deviceId, const QString& name,
                                const QString& serverUrl, const QString& pairingToken)
{
    m_settings.setValue("device/id", deviceId);
    m_settings.setValue("device/name", name);
    m_settings.setValue("device/serverUrl", serverUrl);
    m_settings.setValue("device/pairingToken", pairingToken);
    NativeBridge::syncPairingData(deviceId, pairingToken);
    emit pairedDeviceChanged();
}

void Settings::clearPairedDevice()
{
    m_settings.remove("device/id");
    m_settings.remove("device/name");
    m_settings.remove("device/serverUrl");
    m_settings.remove("device/pairingToken");
    m_settings.remove("device/sessionCookie");
    m_settings.sync();
    NativeBridge::syncPairingData({}, {});
    qDebug() << "Settings: cleared pairedDevice, isPaired:" << isPaired();
    emit pairedDeviceChanged();
}

QVariantMap Settings::themeColors() const { return m_settings.value("theme/colors").toMap(); }
QVariantMap Settings::themeFonts() const { return m_settings.value("theme/fonts").toMap(); }

void Settings::setThemeData(const QVariantMap& colors, const QVariantMap& fonts)
{
    m_settings.setValue("theme/colors", colors);
    m_settings.setValue("theme/fonts", fonts);
    emit themeChanged();
}

bool Settings::themeAutoSync() const { return m_settings.value("theme/autoSync", true).toBool(); }
void Settings::setThemeAutoSync(bool enabled)
{
    if (themeAutoSync() != enabled) {
        m_settings.setValue("theme/autoSync", enabled);
        emit themeAutoSyncChanged();
    }
}

QString Settings::sessionCookie() const { return m_settings.value("device/sessionCookie").toString(); }
void Settings::setSessionCookie(const QString& cookie)
{
    m_settings.setValue("device/sessionCookie", cookie);
}
