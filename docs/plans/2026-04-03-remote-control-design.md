# Remote Control Design

## Overview

Stream the Decenza tablet's screen to the Pocket phone app and send touch events back, enabling remote control over the internet via the existing WebSocket relay.

## Architecture

Three components change:

1. **Tablet (Decenza)** — new `ScreenCaptureService` captures the QML window on change, diffs against the previous frame using a tile grid, and sends changed tiles as binary WebSocket frames through the existing `RelayClient`.

2. **Relay server (decenza.coffee)** — add binary frame passthrough. Binary frames are routed by connection role (device ↔ controller) without parsing.

3. **Phone (DecenzaPocket)** — new `RemoteControlPage` with a canvas that composites incoming tiles. Touch events are normalized and sent back as binary frames.

## Session Lifecycle

1. Phone sends JSON: `{"action": "command", "command": "start_remote", "scale": 0.5}`
2. Tablet starts `ScreenCaptureService`, sends initial full frame as tiles
3. Tablet monitors window for changes, sends only changed tiles
4. Phone sends touch events as binary frames
5. Phone sends `{"action": "command", "command": "stop_remote"}` to end
6. If controller disconnects, tablet auto-stops capturing

## Tile-Based Delta System

- Fixed tile size: 64x64 pixels
- Tablet captures window via `QQuickWindow::grabWindow()`, scales by configurable factor (default 0.5)
- Each tile compared pixel-by-pixel against previous frame
- Only changed tiles are WebP-encoded and sent
- Edge tiles may be smaller (partial tiles at right/bottom)
- Multiple controllers can view simultaneously (relay fans out)

## Binary Protocol

### Screen tiles (tablet → phone): type 0x01

```
[1 byte]  message type = 0x01
[2 bytes] frame width (uint16 BE)
[2 bytes] frame height (uint16 BE)
[1 byte]  tile size (pixels, e.g., 64)
[1 byte]  tile count in this message
Per tile:
  [1 byte]  tile X index
  [1 byte]  tile Y index
  [2 bytes] tile data length (uint16 BE)
  [N bytes] WebP-encoded tile data
```

### Touch events (phone → tablet): type 0x02

```
[1 byte]  message type = 0x02
[1 byte]  touch type (0=press, 1=move, 2=release)
[2 bytes] X (uint16 BE, 0–65535 normalized)
[2 bytes] Y (uint16 BE, 0–65535 normalized)
[1 byte]  touch point ID (for multitouch)
```

## Image Compression

WebP encoding. The Decenza UI is mostly flat colors which compress extremely well with WebP — typically 2-10KB per 64x64 tile.

## Resolution Independence

- Tablet captures at its actual window size, scaled by the scale factor
- Binary header includes actual pixel dimensions
- Phone scales the composited image to fit its screen
- Touch coordinates normalized as fractions (0–65535 maps to 0.0–1.0) so they are resolution-independent

## Component Design

### Tablet: ScreenCaptureService

- Created when `start_remote` command arrives, destroyed on `stop_remote` or disconnect
- Connects to `QQuickWindow::afterRendering()` to detect screen changes
- Captures via `grabWindow()`, scales down, diffs, encodes, sends
- Receives touch binary frames, injects via `QWindowSystemInterface::handleTouchEvent()`
- Sends heartbeat tile every 30s if nothing changed

### Relay: Binary Frame Handler

- Detect binary frames (API Gateway sets `isBase64Encoded = true`)
- Look up sender's connection in DynamoDB for device_id and role
- Forward raw bytes to all connections with same device_id and opposite role
- No parsing or validation of binary content

New commands in JSON validation schema:
- `command: "start_remote"` (with optional `scale` float)
- `command: "stop_remote"`

### Phone: RemoteControlPage + RemoteControlClient

- `RemoteControlPage.qml` — pushed from "Remote Control" button, full-screen canvas
- `RemoteControlClient` (C++) — parses binary frames, composites tiles into QImage, exposes as QML image provider
- Captures touch events, normalizes coordinates, sends as binary frames

## Bandwidth Protection

- Tablet tracks bytes/sec, throttles if exceeding 500KB/s
- Large updates (full page transitions) split across multiple messages
- Each message stays under 128KB (AWS API Gateway limit)

## Multi-Client

- Multiple phones can view simultaneously (relay fans out binary to all controllers)
- All controllers can send touch events (no locking)

## Error Handling

- Heartbeat tile every 30s even if nothing changed
- Phone shows "Connection lost" if no data for 60s
- Touch events ignored if no remote session active
