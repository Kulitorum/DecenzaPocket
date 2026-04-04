#include "nativebridge.h"
#import <Foundation/Foundation.h>
#include <QDebug>

namespace NativeBridge {

void syncPairingData(const QString& deviceId, const QString& pairingToken)
{
    @autoreleasepool {
        NSString* suiteName = @"group.io.github.kulitorum.decenzapocket";
        NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:suiteName];
        if (defaults) {
            [defaults setObject:deviceId.toNSString() forKey:@"device_id"];
            [defaults setObject:pairingToken.toNSString() forKey:@"pairingToken"];
            [defaults synchronize];
            qDebug() << "NativeBridge: synced pairing to App Group UserDefaults";
        }
    }
}

void registerFcmToken(const QString& deviceId, const QString& pairingToken)
{
    // Will be implemented with Firebase in a later task
    Q_UNUSED(deviceId);
    Q_UNUSED(pairingToken);
}

}
