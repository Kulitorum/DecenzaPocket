# DecenzaPocket Design

## Overview

A minimal phone companion app (iOS + Android) for the Decenza espresso machine controller. Connects to the Decenza tablet on the local network for direct control, or through `api.decenza.coffee` when remote. Same Qt/C++/QML stack as Decenza.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  DecenzaPocket (Phone)               │
├─────────────────────────────────────────────────────┤
│  QML UI          │  Network Layer                    │
│  - StatusView    │  - Discovery (UDP 8889)           │
│  - PowerToggle   │  - ShotServer client (HTTPS)      │
│  - ReadyAlert    │  - AWS WebSocket relay client      │
│  - ThemeCache    │  - Theme sync                      │
└────────┬────────────────────┬───────────────────────┘
         │ Local (same WiFi)  │ Remote (anywhere)
         ▼                    ▼
┌─────────────────┐  ┌──────────────────────────┐
│ Decenza Tablet  │  │ wss://ws.decenza.coffee  │
│ ShotServer:8888 │  │ (API Gateway WebSocket)  │
│ UDP:8889        │  │         │                │
└─────────────────┘  │    ┌────▼─────┐          │
                     │    │ Lambda   │          │
                     │    │ relay    │          │
                     │    └────┬─────┘          │
                     │         │ WS forward     │
                     │         ▼                │
                     │  Decenza Tablet          │
                     │  (also connected to WS)  │
                     └──────────────────────────┘
```

## Connection Modes

### Local mode (same network)

1. App broadcasts `DECENZA_DISCOVER` on UDP 8889
2. Decenza responds with `{deviceName, serverUrl, port, secure}`
3. App connects to ShotServer HTTPS, authenticates via TOTP
4. Polls `/api/telemetry` + `/api/state` every 2-3 seconds
5. Sends `/api/power/wake` or `/api/power/sleep` for on/off

### Remote mode (different network)

1. Both Decenza and DecenzaPocket connect to `wss://ws.decenza.coffee`
2. Decenza identifies as `{action: "register", deviceId, role: "device", pairingToken}`
3. DecenzaPocket sends `{action: "command", deviceId, pairingToken, command: "wake"}`
4. Lambda looks up device's WebSocket connection by deviceId, forwards command
5. Decenza processes locally, sends response back through WebSocket
6. DecenzaPocket also receives periodic status updates this way

## Pairing (one-time, local network)

1. User opens DecenzaPocket, it discovers Decenza on the network
2. User enters TOTP code (same as ShotServer web UI auth)
3. App receives: `deviceId`, `deviceName`, session cookie
4. App generates a `pairingToken` (random 256-bit secret)
5. App sends token to Decenza via `POST /api/pocket/pair` (new endpoint)
6. Both store: `deviceId` + `pairingToken` persistently
7. Token is used for AWS relay authentication (no TOTP needed remotely)

## Multi-user Support

Each Decenza tablet has its own `deviceId` (UUID from QSettings). The relay routes by deviceId, so multiple users with different machines work independently. Multiple pocket apps can pair with the same tablet (family sharing) since the `pairingToken` is per-tablet, not per-phone.

## Decenza-side Changes

### ShotServer new endpoints

- `GET /api/theme/export` - Returns full theme as JSON (colors, fonts, spacing)
- `POST /api/pocket/pair` - Accept and store pairing token from pocket app
- `GET /api/pocket/status` - Lightweight status bundle (state + temp + water + heating progress)

### New: AWS relay client (opt-in, enabled after pairing)

- Connects to `wss://ws.decenza.coffee` with `deviceId` + `pairingToken`
- Listens for relay commands, executes them against local ShotServer logic
- Pushes status updates every 5 seconds (only when state changes, to save cost)

## AWS Backend Changes (decenza.coffee)

### Extend WsConnections table

- Add `deviceId` attribute + GSI for lookup
- Add `role` attribute (`device` | `controller`)
- Add `pairingToken` (hashed) for auth

### Extend wsConnect.ts

- Accept `deviceId` and `role` as query params on connect
- Store in DynamoDB with deviceId GSI

### Extend wsMessage.ts

- New action `register` - device registers with deviceId + pairingToken
- New action `command` - controller sends command to device (wake/sleep/status)
- New action `status` - device pushes status update to paired controllers
- Validate pairingToken before forwarding

### New REST endpoint (convenience)

- `POST /v1/relay/{deviceId}/command` - For one-off commands without maintaining a WebSocket

### Cost estimate (per user, 24/7)

| Component | Usage | Monthly cost |
|-----------|-------|-------------|
| WebSocket connection minutes | ~43,200 min | $0.01 |
| WebSocket messages | ~30,000 | $0.03 |
| Lambda invocations | ~60,000 | $0.00 (free tier) |
| DynamoDB reads/writes | ~60,000 | $0.01 |
| **Total** | | **~$0.05/month** |

## Phone UI (single screen)

```
┌──────────────────────────┐
│     Decenza Pocket       │
│                          │
│   ┌──────────────────┐   │
│   │   Machine        │   │
│   │                  │   │
│   │   ● Ready        │   │  ← State (color-coded)
│   │   93.2°C         │   │  ← Group head temp
│   │   Water: 78%     │   │  ← Water level
│   │                  │   │
│   └──────────────────┘   │
│                          │
│   ┌──────────────────┐   │
│   │                  │   │
│   │   (  Power  )    │   │  ← Big toggle: Wake/Sleep
│   │                  │   │
│   └──────────────────┘   │
│                          │
│   Local connection       │  ← Connection indicator
│   Settings               │
└──────────────────────────┘
```

### Ready notification

- When machine transitions from Heating to Ready, play a notification sound
- Works in both local and remote modes
- Uses system notification so it works even if app is backgrounded
  - Android: foreground service
  - iOS: local notification (or push via AWS if app is suspended)

## Theme Sync

- On local connection, fetch `GET /api/theme/export` and cache to disk
- Theme includes: background color, card color, text colors, accent color, font family, font sizes, spacing, radius values
- App uses cached theme, falls back to a dark default before first sync
- Re-syncs whenever on local network (compare ETag/hash to avoid unnecessary downloads)

## Technology Stack

Same as Decenza:
- Qt 6.10 / C++17 / QML
- CMake build system
- Android + iOS targets only (no desktop)
- Qt Network (HTTPS client, UDP discovery, WebSocket)
- No BLE needed (talks to Decenza, not directly to DE1)

## What We Don't Build

- No shot graphing, no profile editor, no scale support
- No BLE - all communication goes through Decenza's ShotServer
- No desktop build
- No AI, no screensaver, no media
- No MQTT - the AWS WebSocket relay replaces this need
