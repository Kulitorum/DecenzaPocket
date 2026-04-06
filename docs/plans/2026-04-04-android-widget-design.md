# Android Power Widget Design

## Goal

A home screen widget that toggles the espresso machine on/off with a single tap and keeps its icon in sync with the actual machine state.

## Behavior

- **Tap**: Sends wake or sleep command via short-lived WebSocket to `wss://ws.decenza.coffee`
- **Icon**: Green power button (ON) / Red power button (OFF), updated in real-time via FCM
- **Failure**: If relay unreachable or not paired, opens the app instead
- **State source**: FCM push on state change + cached last-known state in DynamoDB

## Architecture

```
Machine state changes
        |
        v
Relay Lambda (status_push handler)
        |
        +-- broadcasts relay_status to connected controllers (existing)
        +-- if isAwake changed: cache in DynamoDB + send FCM data message (new)
                |
                v
        Android FCM Service
                |
                v
        Update SharedPreferences + refresh widget icon
```

### Widget tap flow

```
Widget tap
    |
    v
Open WebSocket to wss://ws.decenza.coffee
    |
    v
Register as controller, send wake/sleep command
    |
    v
Wait for relay_response (success/fail)
    |
    +-- success: update SharedPreferences + refresh icon
    +-- fail/timeout: launch app via Intent
    |
    v
Close WebSocket
```

## Components

### Backend (decenza.coffee)

1. **DynamoDB: DeviceState table** — caches `{device_id, isAwake, last_updated}` per device
2. **DynamoDB: FcmTokens table** — stores `{device_id, fcm_token, platform, last_updated}` per controller
3. **wsMessage Lambda changes**:
   - On `status_push`: compare `isAwake` with cached state. If changed, update cache + send FCM to all registered tokens for that device_id
   - On new action `register_fcm_token`: store/update token in FcmTokens table
4. **New action `get_device_state`**: returns cached isAwake for a device_id (for widget init after reboot)
5. **HTTPS endpoint** for `get_device_state` (simple API Gateway HTTP route + Lambda) so the widget doesn't need a full WebSocket handshake just to query state

### Android (DecenzaPocket)

1. **`PowerWidgetProvider`** (Java/Kotlin) — `AppWidgetProvider` subclass
   - `onUpdate`: reads SharedPreferences, sets icon + click PendingIntent
   - `onReceive`: handles custom broadcast from FCM service to refresh
2. **`PowerWidgetService`** (Java/Kotlin) — `JobIntentService` for tap handling
   - Opens WebSocket, registers, sends command, waits for response, updates widget
   - On failure, launches app Activity
3. **`DecenzaFcmService`** (Java/Kotlin) — `FirebaseMessagingService`
   - `onMessageReceived`: extracts `isAwake` from data payload, updates SharedPreferences, broadcasts to widget
   - `onNewToken`: sends new token to relay via HTTPS
4. **Widget layout XML** — `RemoteViews` with single ImageView
5. **Widget metadata XML** — 1x1 size, resizable, update period 0 (FCM-driven)
6. **Icons**: `DecenzaPocketIcon.png` (ON), `DecenzaPocketIcon_OFF.png` (OFF) scaled to widget drawable densities

### Gradle changes

- Add Firebase BOM + `firebase-messaging` dependency
- Add `google-services` plugin
- `google-services.json` from Firebase console (user provides)

### AndroidManifest additions

- `PowerWidgetProvider` receiver with `APPWIDGET_UPDATE` intent filter
- `DecenzaFcmService` service
- `PowerWidgetService` service
- Widget metadata XML reference

## Icons

- ON: `icon/DecenzaPocketIcon.png` (green power button)
- OFF: `icon/DecenzaPocketIcon_OFF.png` (red power button)
- Need density-scaled versions for widget drawables (mdpi through xxxhdpi)

## Status

Implemented and shipped in v0.3.0. iOS widget also implemented (see `2026-04-04-ios-widget-design.md`).

## Out of scope

- Local network discovery from widget
- Showing temperature or other status in widget
- Multiple machine support (single paired device)
