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
    @autoreleasepool {
        NSString* wsUrl = @"wss://ws.decenza.coffee";
        NSString* devId = deviceId.toNSString();
        NSString* pairToken = pairingToken.toNSString();

        // Get FCM token - for now, use APNs device token directly
        // Firebase SDK integration requires CocoaPods/SPM in the CI build
        // This will be wired up when GoogleService-Info.plist is available

        // For initial implementation: register using the relay WebSocket
        NSURL* url = [NSURL URLWithString:wsUrl];
        NSURLSession* session = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration]];
        NSURLSessionWebSocketTask* task = [session webSocketTaskWithURL:url];
        [task resume];

        // Register as controller
        NSDictionary* registerMsg = @{
            @"action": @"register",
            @"device_id": devId,
            @"role": @"controller",
            @"pairing_token": pairToken
        };
        NSData* registerData = [NSJSONSerialization dataWithJSONObject:registerMsg options:0 error:nil];
        NSString* registerStr = [[NSString alloc] initWithData:registerData encoding:NSUTF8StringEncoding];

        [task sendMessage:[[NSURLSessionWebSocketMessage alloc] initWithString:registerStr]
            completionHandler:^(NSError* err) {
            if (err) {
                qDebug() << "NativeBridge: FCM register WebSocket send error";
                [task cancel];
                return;
            }

            [task receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage* msg, NSError* recvErr) {
                if (recvErr) {
                    qDebug() << "NativeBridge: FCM register WebSocket receive error";
                    [task cancel];
                    return;
                }

                // TODO: Once Firebase SDK is integrated, get FCM token here
                // For now, skip the actual token registration
                // The widget can still function with manual state sync
                qDebug() << "NativeBridge: iOS FCM token registration pending Firebase SDK integration";
                [task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure reason:nil];
            }];
        }];
    }
}

}
