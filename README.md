# DecenzaPocket

Companion phone app for [Decenza](https://github.com/Kulitorum/Decenza) — a modern controller for Decent DE1 espresso machines.

![Platform](https://img.shields.io/badge/platform-iOS%20%7C%20Android-blue)
![Qt](https://img.shields.io/badge/Qt-6.10.2-green)
![License](https://img.shields.io/badge/license-GPL--3.0-orange)
![CI](https://img.shields.io/github/actions/workflow/status/Kulitorum/DecenzaPocket/android-release.yml?label=Android)
![CI](https://img.shields.io/github/actions/workflow/status/Kulitorum/DecenzaPocket/ios-release.yml?label=iOS)

## Overview

DecenzaPocket connects to a running Decenza instance over the network, giving you phone-sized controls for your espresso machine. It supports both local network discovery and remote access via AWS WebSocket relay.

## Features

- **Auto-discovery** of Decenza instances on the local network
- **Remote access** via AWS WebSocket relay when away from home
- **Start/stop shots** and monitor machine state
- **Real-time status** — pressure, temperature, flow, and shot progress
- **Power control** — wake and sleep your DE1 remotely

## Download

- **Android**: Download the latest APK from [Releases](https://github.com/Kulitorum/DecenzaPocket/releases)
- **iOS**: Available via TestFlight (contact the developer for access)

## Building

Requires Qt 6.10.2 with the following modules: Core, Gui, Qml, Quick, QuickControls2, Network, WebSockets, Multimedia.

Open `CMakeLists.txt` in Qt Creator and build for your target platform (iOS or Android).

## CI/CD

Automated builds via GitHub Actions:
- **Android**: Builds signed APK, uploads to GitHub Releases on version tags
- **iOS**: Builds IPA, uploads to App Store Connect / TestFlight on version tags

## Related Projects

- [Decenza](https://github.com/Kulitorum/Decenza) — the full tablet/desktop controller app
- [decenza.coffee](https://github.com/Kulitorum/decenza.coffee) — AWS backend for remote relay

## License

This project is licensed under the GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.
