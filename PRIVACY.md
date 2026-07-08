# Privacy Policy for Decenza Pocket

**Last updated: July 8, 2026**

## Overview

Decenza Pocket is a companion app that lets you monitor and control your Decent
Espresso DE1 machine remotely, through the Decenza tablet app. We are committed to
protecting your privacy. **We do not collect, sell, or share any personal data, and
the app contains no advertising or third-party analytics or tracking.**

## What the app communicates with

To do its job, Decenza Pocket communicates only with:

- **Your Decenza tablet**, relayed through the Decenza connection server
  (`wss://ws.decenza.coffee`). This relay passes power on/off commands, machine
  status (temperature, water level, heating state), and — when you open Remote
  Control — screen images and your touch input between your phone and the tablet.
- **Your local network**, to discover your Decenza tablet for pairing. No data
  leaves your network during discovery.
- **Firebase Cloud Messaging** (a Google service), used only to deliver push
  notifications — the "machine ready" alert and home-screen widget state updates.

## Permissions

- **Local Network** — to find your Decenza tablet on your Wi‑Fi for pairing.
- **Internet** — to reach the relay server and receive push notifications.
- **Notifications** — to alert you when your machine is ready.

The app does **not** use Bluetooth, location, the camera, the microphone, your
contacts, or any advertising identifier.

## Data storage

- **On your device:** your paired-device identifier, its pairing token, and a
  cached copy of your tablet's theme are stored locally so the app can reconnect.
  If your Decenza tablet has web security enabled, you enter a one-time
  authenticator code during pairing; that code is used only to authenticate and is
  not stored by us.
- **On the connection server:** to keep your home-screen widget in sync, the server
  caches your machine's current on/off (awake) state. No shot data, no account, and
  no personal content is stored.

## Third-party services

- **Amazon Web Services (AWS)** hosts the Decenza connection relay described above.
- **Google Firebase Cloud Messaging** delivers push notifications. A device push
  token is registered to route notifications to your device. See Google's privacy
  policy at https://firebase.google.com/support/privacy and
  https://policies.google.com/privacy.

## Children's privacy

Decenza Pocket is not directed to children and does not knowingly collect data from
anyone.

## Open source

Decenza Pocket is open source. You can review the complete source code at:
https://github.com/Kulitorum/DecenzaPocket

## Contact

For questions about this privacy policy, please open an issue on GitHub:
https://github.com/Kulitorum/DecenzaPocket/issues

## Changes

Any changes to this privacy policy will be posted in this file and in the app's
repository.
