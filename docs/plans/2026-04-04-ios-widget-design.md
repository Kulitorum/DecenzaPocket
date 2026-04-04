# iOS Power Widget Design

## Goal

An iOS home screen widget that toggles the espresso machine on/off with a single tap and stays in sync with the real machine state via FCM push notifications. Identical behavior to the Android widget.

## Behavior

- **Tap**: Sends wake or sleep command via short-lived WebSocket to `wss://ws.decenza.coffee`
- **Icon**: Green power button (ON) / Red power button (OFF), updated in real-time via FCM
- **Failure**: If relay unreachable or not paired, opens the app
- **State source**: FCM push on state change + cached last-known state via shared UserDefaults

## Architecture

### Widget Extension (standalone Xcode project)

```
ios/PowerWidget/
  PowerWidget.xcodeproj
  PowerWidgetBundle.swift          (WidgetKit entry point)
  PowerWidget.swift                (widget view + toggle intent)
  WebSocketCommand.swift           (URLSessionWebSocketTask for wake/sleep)
  SharedState.swift                (App Group UserDefaults read/write)
  Info.plist                       (NSExtension config)
  Assets.xcassets/                 (widget_on / widget_off images)
```

### Data sharing via App Groups

Main app and widget extension share data through `UserDefaults(suiteName: "group.io.github.kulitorum.decenzapocket")`:
- `device_id` (String)
- `pairingToken` (String)
- `isAwake` (Bool)

### FCM token registration from Qt app

Qt C++ app registers the FCM token on startup via Objective-C bridge:
- Objective-C++ file calls Firebase iOS SDK to get token
- Sends `register_fcm_token` to relay via existing WebSocket connection
- Writes pairing data to App Group UserDefaults (like Android SharedPreferences sync)

### CI build integration

- Widget extension is a standalone Xcode project in `ios/PowerWidget/`
- CI builds it separately after the main app
- Embeds the `.appex` into the main app archive before export

## Backend

No changes needed. The existing FCM module, DynamoDB tables, and relay logic support iOS tokens already (`platform: 'ios'` in the schema).

## Firebase iOS setup

1. Register iOS app in existing Firebase project (`io.github.kulitorum.decenzapocket`)
2. Download `GoogleService-Info.plist` into `ios/`
3. Enable Push Notifications capability on the App ID (Apple Developer portal)
4. Create APNs key and upload to Firebase console
5. Add Firebase iOS SDK to widget extension via SPM

## Key differences from Android

| Aspect | Android | iOS |
|--------|---------|-----|
| Widget framework | AppWidgetProvider (Java) | WidgetKit (Swift) |
| WebSocket client | OkHttp | URLSessionWebSocketTask (built-in) |
| Shared state | SharedPreferences via JNI | App Group UserDefaults |
| FCM bridge | Java DecenzaFcmService | Objective-C++ Firebase bridge |
| Build integration | Gradle dependencies | Separate Xcode project, embedded .appex |
| Push entitlement | Automatic with FCM | Requires APNs key + entitlement |

## Out of scope

- Lock Screen controls (iOS 18 Control Center widgets — future work)
- Complications for Apple Watch
- Multiple machine support
