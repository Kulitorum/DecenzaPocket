#pragma once
#include <QString>

namespace NativeBridge {
    void syncPairingData(const QString& deviceId, const QString& pairingToken);
    void registerFcmToken(const QString& deviceId, const QString& pairingToken);
}
