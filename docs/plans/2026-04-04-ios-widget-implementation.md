# iOS Power Widget Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add an iOS home screen widget that toggles the espresso machine on/off and stays in sync via FCM push notifications, identical to the Android widget.

**Architecture:** Standalone WidgetKit extension (Swift) built as a separate Xcode project. Main Qt app syncs pairing data via App Group UserDefaults and registers FCM token via Objective-C++ bridge. CI builds extension separately and embeds `.appex` into the main app archive.

**Tech Stack:** Swift/WidgetKit (widget), Objective-C++ (Firebase bridge in Qt app), Firebase iOS SDK (FCM), URLSessionWebSocketTask (WebSocket)

---

### Task 1: Create widget extension Xcode project structure

**Files:**
- Create: `ios/PowerWidget/PowerWidget.xcodeproj` (via Xcode CLI or manual)
- Create: `ios/PowerWidget/PowerWidgetBundle.swift`
- Create: `ios/PowerWidget/Info.plist`
- Create: `ios/PowerWidget/Assets.xcassets/` (with widget icons)

**Step 1: Create directory structure**

```bash
mkdir -p ios/PowerWidget/Assets.xcassets
```

**Step 2: Create the WidgetKit bundle entry point**

Create `ios/PowerWidget/PowerWidgetBundle.swift`:

```swift
import WidgetKit
import SwiftUI

@main
struct PowerWidgetBundle: WidgetBundle {
    var body: some Widget {
        PowerWidget()
    }
}
```

**Step 3: Create Info.plist for the extension**

Create `ios/PowerWidget/Info.plist`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDisplayName</key>
    <string>Decenza Power</string>
    <key>CFBundleIdentifier</key>
    <string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>
    <key>CFBundleExecutable</key>
    <string>$(EXECUTABLE_NAME)</string>
    <key>CFBundleName</key>
    <string>$(PRODUCT_NAME)</string>
    <key>CFBundlePackageType</key>
    <string>$(PRODUCT_BUNDLE_PACKAGE_TYPE)</string>
    <key>CFBundleShortVersionString</key>
    <string>$(MARKETING_VERSION)</string>
    <key>CFBundleVersion</key>
    <string>$(CURRENT_PROJECT_VERSION)</string>
    <key>NSExtension</key>
    <dict>
        <key>NSExtensionPointIdentifier</key>
        <string>com.apple.widgetkit-extension</string>
    </dict>
    <key>MinimumOSVersion</key>
    <string>17.0</string>
</dict>
</plist>
```

**Step 4: Scale widget icons into asset catalog**

Create `ios/PowerWidget/Assets.xcassets/WidgetOn.imageset/Contents.json`:

```json
{
  "images": [
    { "filename": "widget_on@2x.png", "idiom": "universal", "scale": "2x" },
    { "filename": "widget_on@3x.png", "idiom": "universal", "scale": "3x" }
  ],
  "info": { "version": 1, "author": "xcode" }
}
```

Create `ios/PowerWidget/Assets.xcassets/WidgetOff.imageset/Contents.json`:

```json
{
  "images": [
    { "filename": "widget_off@2x.png", "idiom": "universal", "scale": "2x" },
    { "filename": "widget_off@3x.png", "idiom": "universal", "scale": "3x" }
  ],
  "info": { "version": 1, "author": "xcode" }
}
```

Scale the source icons using ImageMagick:

```bash
# ON icon (transparent background version)
magick icon/DecenzaPocketIcon.png -fuzz 15% -transparent white -resize 120x120 ios/PowerWidget/Assets.xcassets/WidgetOn.imageset/widget_on@2x.png
magick icon/DecenzaPocketIcon.png -fuzz 15% -transparent white -resize 180x180 ios/PowerWidget/Assets.xcassets/WidgetOn.imageset/widget_on@3x.png

# OFF icon
magick icon/DecenzaPocketIcon_OFF.png -resize 120x120 ios/PowerWidget/Assets.xcassets/WidgetOff.imageset/widget_off@2x.png
magick icon/DecenzaPocketIcon_OFF.png -resize 180x180 ios/PowerWidget/Assets.xcassets/WidgetOff.imageset/widget_off@3x.png
```

**Step 5: Commit**

```bash
git add ios/PowerWidget/
git commit -m "feat: add iOS widget extension project structure and assets"
```

---

### Task 2: Create shared state helper (App Group UserDefaults)

**Files:**
- Create: `ios/PowerWidget/SharedState.swift`

**Step 1: Create SharedState.swift**

```swift
import Foundation

struct SharedState {
    static let suiteName = "group.io.github.kulitorum.decenzapocket"

    private static var defaults: UserDefaults? {
        UserDefaults(suiteName: suiteName)
    }

    static var deviceId: String {
        get { defaults?.string(forKey: "device_id") ?? "" }
        set { defaults?.set(newValue, forKey: "device_id") }
    }

    static var pairingToken: String {
        get { defaults?.string(forKey: "pairingToken") ?? "" }
        set { defaults?.set(newValue, forKey: "pairingToken") }
    }

    static var isAwake: Bool {
        get { defaults?.bool(forKey: "isAwake") ?? false }
        set { defaults?.set(newValue, forKey: "isAwake") }
    }

    static var isPaired: Bool {
        !deviceId.isEmpty && !pairingToken.isEmpty
    }
}
```

**Step 2: Commit**

```bash
git add ios/PowerWidget/SharedState.swift
git commit -m "feat: add App Group shared state for widget/app communication"
```

---

### Task 3: Create WebSocket command helper

**Files:**
- Create: `ios/PowerWidget/WebSocketCommand.swift`

**Step 1: Create WebSocketCommand.swift**

Uses `URLSessionWebSocketTask` (built into iOS, no dependencies):

```swift
import Foundation

enum PowerCommand: String {
    case wake, sleep
}

struct WebSocketCommand {
    static let wsURL = URL(string: "wss://ws.decenza.coffee")!

    static func send(_ command: PowerCommand, completion: @escaping (Bool) -> Void) {
        guard SharedState.isPaired else {
            completion(false)
            return
        }

        let deviceId = SharedState.deviceId
        let pairingToken = SharedState.pairingToken
        let session = URLSession(configuration: .default)
        let task = session.webSocketTask(with: wsURL)
        task.resume()

        // Register as controller
        let register: [String: Any] = [
            "action": "register",
            "device_id": deviceId,
            "role": "controller",
            "pairing_token": pairingToken
        ]

        guard let registerData = try? JSONSerialization.data(withJSONObject: register),
              let registerStr = String(data: registerData, encoding: .utf8) else {
            completion(false)
            return
        }

        task.send(.string(registerStr)) { error in
            if error != nil {
                completion(false)
                return
            }

            // Wait for "registered" response, then send command
            task.receive { result in
                guard case .success(let message) = result,
                      case .string(let text) = message,
                      let json = try? JSONSerialization.jsonObject(with: Data(text.utf8)) as? [String: Any],
                      json["type"] as? String == "registered" else {
                    task.cancel(with: .normalClosure, reason: nil)
                    completion(false)
                    return
                }

                // Send the command
                let cmd: [String: Any] = [
                    "action": "command",
                    "device_id": deviceId,
                    "pairing_token": pairingToken,
                    "command": command.rawValue
                ]

                guard let cmdData = try? JSONSerialization.data(withJSONObject: cmd),
                      let cmdStr = String(data: cmdData, encoding: .utf8) else {
                    task.cancel(with: .normalClosure, reason: nil)
                    completion(false)
                    return
                }

                task.send(.string(cmdStr)) { error in
                    if error != nil {
                        task.cancel(with: .normalClosure, reason: nil)
                        completion(false)
                        return
                    }

                    // Wait for command_sent
                    task.receive { result in
                        var success = false
                        if case .success(let msg) = result,
                           case .string(let txt) = msg,
                           let js = try? JSONSerialization.jsonObject(with: Data(txt.utf8)) as? [String: Any],
                           js["type"] as? String == "command_sent" {
                            success = true
                        }
                        task.cancel(with: .normalClosure, reason: nil)
                        completion(success)
                    }
                }
            }
        }
    }
}
```

**Step 2: Commit**

```bash
git add ios/PowerWidget/WebSocketCommand.swift
git commit -m "feat: add WebSocket command helper for widget power toggle"
```

---

### Task 4: Create the WidgetKit widget with interactive toggle

**Files:**
- Create: `ios/PowerWidget/PowerWidget.swift`

**Step 1: Create PowerWidget.swift**

iOS 17+ supports interactive widgets with `AppIntent`:

```swift
import WidgetKit
import SwiftUI
import AppIntents

// MARK: - Toggle Intent

struct TogglePowerIntent: AppIntent {
    static var title: LocalizedStringResource = "Toggle Decenza Power"
    static var description = IntentDescription("Toggles the espresso machine on or off")

    func perform() async throws -> some IntentResult {
        guard SharedState.isPaired else {
            return .result()
        }

        let currentState = SharedState.isAwake
        let command: PowerCommand = currentState ? .sleep : .wake

        await withCheckedContinuation { continuation in
            WebSocketCommand.send(command) { success in
                if success {
                    SharedState.isAwake = !currentState
                }
                continuation.resume()
            }
        }

        return .result()
    }
}

// MARK: - Timeline Provider

struct PowerTimelineProvider: TimelineProvider {
    func placeholder(in context: Context) -> PowerEntry {
        PowerEntry(date: Date(), isAwake: false)
    }

    func getSnapshot(in context: Context, completion: @escaping (PowerEntry) -> Void) {
        completion(PowerEntry(date: Date(), isAwake: SharedState.isAwake))
    }

    func getTimeline(in context: Context, completion: @escaping (Timeline<PowerEntry>) -> Void) {
        let entry = PowerEntry(date: Date(), isAwake: SharedState.isAwake)
        let timeline = Timeline(entries: [entry], policy: .never)
        completion(timeline)
    }
}

// MARK: - Timeline Entry

struct PowerEntry: TimelineEntry {
    let date: Date
    let isAwake: Bool
}

// MARK: - Widget View

struct PowerWidgetView: View {
    var entry: PowerEntry

    var body: some View {
        Button(intent: TogglePowerIntent()) {
            Image(entry.isAwake ? "WidgetOn" : "WidgetOff")
                .resizable()
                .aspectRatio(contentMode: .fit)
        }
        .buttonStyle(.plain)
    }
}

// MARK: - Widget Configuration

struct PowerWidget: Widget {
    let kind = "PowerWidget"

    var body: some WidgetConfiguration {
        StaticConfiguration(kind: kind, provider: PowerTimelineProvider()) { entry in
            PowerWidgetView(entry: entry)
                .containerBackground(.clear, for: .widget)
        }
        .configurationDisplayName("Decenza Power")
        .description("Toggle your espresso machine on or off")
        .supportedFamilies([.systemSmall])
    }
}
```

**Step 2: Commit**

```bash
git add ios/PowerWidget/PowerWidget.swift
git commit -m "feat: add WidgetKit power widget with interactive toggle"
```

---

### Task 5: Create the Xcode project file for the widget extension

This task creates a minimal `.xcodeproj` that can be built with `xcodebuild`. The project has a single target: the widget extension.

**Files:**
- Create: `ios/PowerWidget/PowerWidget.xcodeproj/project.pbxproj`

**Step 1: Create the Xcode project**

This is best done with a script or using `xcodegen`. Create `ios/PowerWidget/project.yml` for XcodeGen:

```yaml
name: PowerWidget
options:
  bundleIdPrefix: io.github.kulitorum.decenzapocket
  deploymentTarget:
    iOS: "17.0"
  xcodeVersion: "16.0"
targets:
  PowerWidgetExtension:
    type: app-extension
    platform: iOS
    productName: PowerWidgetExtension
    sources:
      - path: .
        excludes:
          - "project.yml"
          - "PowerWidget.xcodeproj"
    settings:
      base:
        PRODUCT_BUNDLE_IDENTIFIER: io.github.kulitorum.decenzapocket.PowerWidget
        SWIFT_VERSION: "5.9"
        GENERATE_INFOPLIST_FILE: NO
        INFOPLIST_FILE: Info.plist
        CODE_SIGN_ENTITLEMENTS: PowerWidgetExtension.entitlements
        MARKETING_VERSION: "$(MARKETING_VERSION)"
        CURRENT_PROJECT_VERSION: "$(CURRENT_PROJECT_VERSION)"
    entitlements:
      path: PowerWidgetExtension.entitlements
```

**Step 2: Create entitlements file**

Create `ios/PowerWidget/PowerWidgetExtension.entitlements`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.application-groups</key>
    <array>
        <string>group.io.github.kulitorum.decenzapocket</string>
    </array>
</dict>
</plist>
```

**Step 3: Generate the Xcode project**

In CI, install `xcodegen` and run:

```bash
cd ios/PowerWidget && xcodegen generate
```

Or commit the generated `.xcodeproj` directly.

**Step 4: Commit**

```bash
git add ios/PowerWidget/
git commit -m "feat: add widget extension Xcode project and entitlements"
```

---

### Task 6: Qt app — Sync pairing data to App Group UserDefaults (iOS)

**Files:**
- Modify: `src/core/settings.h` (rename `syncToAndroidPrefs` to `syncToNativePrefs`)
- Modify: `src/core/settings.cpp` (add iOS App Group UserDefaults sync)

**Step 1: Rename and extend the sync method**

In `src/core/settings.h`, rename:
```cpp
void syncToNativePrefs(const QString& deviceId, const QString& pairingToken);
```

In `src/core/settings.cpp`, rename `syncToAndroidPrefs` to `syncToNativePrefs` and add an `#ifdef Q_OS_IOS` block:

```cpp
void Settings::syncToNativePrefs(const QString& deviceId, const QString& pairingToken)
{
#ifdef Q_OS_ANDROID
    // ... existing Android JNI code (unchanged) ...
#endif

#ifdef Q_OS_IOS
    @autoreleasepool {
        NSString* suiteName = @"group.io.github.kulitorum.decenzapocket";
        NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:suiteName];
        if (defaults) {
            [defaults setObject:deviceId.toNSString() forKey:@"device_id"];
            [defaults setObject:pairingToken.toNSString() forKey:@"pairingToken"];
            [defaults synchronize];
            qDebug() << "Settings: synced pairing to iOS App Group UserDefaults";
        }
    }
#endif
}
```

Note: This file must be compiled as Objective-C++ on iOS. Rename `settings.cpp` to `settings.mm` or add the iOS-specific code in a separate `.mm` file.

**Better approach — create a separate platform bridge file:**

Create `src/core/nativebridge.h`:

```cpp
#pragma once
#include <QString>

namespace NativeBridge {
    void syncPairingData(const QString& deviceId, const QString& pairingToken);
    void registerFcmToken(const QString& deviceId, const QString& pairingToken);
}
```

Create `src/core/nativebridge_ios.mm`:

```objc
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
    // Will be implemented in Task 7 with Firebase
    Q_UNUSED(deviceId);
    Q_UNUSED(pairingToken);
}

}
```

Create `src/core/nativebridge_android.cpp` (move existing JNI code):

```cpp
#include "nativebridge.h"
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif
#include <QDebug>

namespace NativeBridge {

void syncPairingData(const QString& deviceId, const QString& pairingToken)
{
#ifdef Q_OS_ANDROID
    // ... existing JNI SharedPreferences code from settings.cpp ...
#endif
}

void registerFcmToken(const QString& deviceId, const QString& pairingToken)
{
    // Android handles this from Java (DecenzaApplication.java)
    Q_UNUSED(deviceId);
    Q_UNUSED(pairingToken);
}

}
```

Create `src/core/nativebridge_stub.cpp` (for desktop builds):

```cpp
#include "nativebridge.h"
#include <QDebug>

namespace NativeBridge {
void syncPairingData(const QString& deviceId, const QString& pairingToken) {
    Q_UNUSED(deviceId); Q_UNUSED(pairingToken);
}
void registerFcmToken(const QString& deviceId, const QString& pairingToken) {
    Q_UNUSED(deviceId); Q_UNUSED(pairingToken);
}
}
```

**Step 2: Update CMakeLists.txt**

Add the platform-specific bridge files:

```cmake
if(ANDROID)
    list(APPEND SOURCES src/core/nativebridge_android.cpp)
elseif(IOS)
    list(APPEND SOURCES src/core/nativebridge_ios.mm)
else()
    list(APPEND SOURCES src/core/nativebridge_stub.cpp)
endif()
```

**Step 3: Update settings.cpp**

Replace the `syncToAndroidPrefs` calls with `NativeBridge::syncPairingData`:

```cpp
#include "core/nativebridge.h"

// In setPairedDevice():
NativeBridge::syncPairingData(deviceId, pairingToken);

// In clearPairedDevice():
NativeBridge::syncPairingData({}, {});
```

Remove the old `syncToAndroidPrefs` method and the `#ifdef Q_OS_ANDROID` / `QJniObject` includes from settings.cpp.

**Step 4: Commit**

```bash
git add src/core/nativebridge.h src/core/nativebridge_ios.mm src/core/nativebridge_android.cpp src/core/nativebridge_stub.cpp src/core/settings.cpp src/core/settings.h CMakeLists.txt
git commit -m "feat: add NativeBridge for cross-platform pairing data sync"
```

---

### Task 7: Qt app — Add Firebase iOS SDK and FCM token registration

**Files:**
- Modify: `ios/PowerWidget/project.yml` or `CMakeLists.txt` (add Firebase SPM)
- Modify: `src/core/nativebridge_ios.mm` (implement FCM token registration)
- Create: `ios/GoogleService-Info.plist` (user provides, gitignore it)
- Modify: `.gitignore` (add GoogleService-Info.plist)

**Step 1: Add GoogleService-Info.plist to gitignore**

Append to `.gitignore`:

```
ios/GoogleService-Info.plist
```

**Step 2: Update nativebridge_ios.mm with Firebase FCM registration**

```objc
#include "nativebridge.h"
#import <Foundation/Foundation.h>
@import FirebaseCore;
@import FirebaseMessaging;
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
        // Ensure Firebase is configured
        if (![FIRApp defaultApp]) {
            [FIRApp configure];
        }

        [[FIRMessaging messaging] tokenWithCompletion:^(NSString* token, NSError* error) {
            if (error) {
                qDebug() << "NativeBridge: FCM token error:" << QString::fromNSString(error.localizedDescription);
                return;
            }
            qDebug() << "NativeBridge: got FCM token, registering...";

            // Register token with relay via WebSocket
            // Reuse the same WebSocket pattern as the widget
            NSString* wsUrl = @"wss://ws.decenza.coffee";
            NSURL* url = [NSURL URLWithString:wsUrl];
            NSURLSession* session = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration]];
            NSURLSessionWebSocketTask* task = [session webSocketTaskWithURL:url];
            [task resume];

            // Register as controller
            NSDictionary* registerMsg = @{
                @"action": @"register",
                @"device_id": deviceId.toNSString(),
                @"role": @"controller",
                @"pairing_token": pairingToken.toNSString()
            };
            NSData* registerData = [NSJSONSerialization dataWithJSONObject:registerMsg options:0 error:nil];
            NSString* registerStr = [[NSString alloc] initWithData:registerData encoding:NSUTF8StringEncoding];

            [task sendMessage:[[NSURLSessionWebSocketMessage alloc] initWithString:registerStr]
                completionHandler:^(NSError* err) {
                if (err) { [task cancel]; return; }

                [task receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage* msg, NSError* recvErr) {
                    if (recvErr) { [task cancel]; return; }

                    // Send register_fcm_token
                    NSDictionary* fcmMsg = @{
                        @"action": @"register_fcm_token",
                        @"device_id": deviceId.toNSString(),
                        @"pairing_token": pairingToken.toNSString(),
                        @"fcm_token": token,
                        @"platform": @"ios"
                    };
                    NSData* fcmData = [NSJSONSerialization dataWithJSONObject:fcmMsg options:0 error:nil];
                    NSString* fcmStr = [[NSString alloc] initWithData:fcmData encoding:NSUTF8StringEncoding];

                    [task sendMessage:[[NSURLSessionWebSocketMessage alloc] initWithString:fcmStr]
                        completionHandler:^(NSError* sendErr) {
                        if (sendErr) { [task cancel]; return; }

                        [task receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage* resp, NSError* respErr) {
                            if (!respErr) {
                                qDebug() << "NativeBridge: FCM token registered with relay";
                            }
                            [task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure reason:nil];
                        }];
                    }];
                }];
            }];
        }];
    }
}

}
```

**Step 3: Call registerFcmToken on app startup**

In `src/main.cpp`, after `connection.start()` or at the end of initialization:

```cpp
#include "core/nativebridge.h"

// After Settings is created and before app.exec():
if (settings.isPaired()) {
    NativeBridge::syncPairingData(settings.pairedDeviceId(), settings.pairingToken());
    NativeBridge::registerFcmToken(settings.pairedDeviceId(), settings.pairingToken());
}
```

**Step 4: Add Firebase SDK to the iOS build**

In `CMakeLists.txt` iOS section, add:

```cmake
    # Firebase for FCM (push notifications for widget)
    # Firebase SDK is added via Swift Package Manager in the CI build step
```

The Firebase SDK will be integrated via SPM in the CI workflow rather than in CMake.

**Step 5: Commit**

```bash
git add src/core/nativebridge_ios.mm src/main.cpp CMakeLists.txt .gitignore
git commit -m "feat: add FCM token registration via Objective-C++ bridge on iOS"
```

---

### Task 8: Add App Group entitlement to main app

**Files:**
- Modify: `ios/Info.plist` or create `ios/DecenzaPocket.entitlements`

**Step 1: Create main app entitlements**

Create `ios/DecenzaPocket.entitlements`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.application-groups</key>
    <array>
        <string>group.io.github.kulitorum.decenzapocket</string>
    </array>
    <key>aps-environment</key>
    <string>production</string>
</dict>
</plist>
```

**Step 2: Add entitlements to CMakeLists.txt**

In the `if(IOS)` block:

```cmake
    set_target_properties(DecenzaPocket PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/ios/DecenzaPocket.entitlements"
    )
```

**Step 3: Commit**

```bash
git add ios/DecenzaPocket.entitlements CMakeLists.txt
git commit -m "feat: add App Group and push notification entitlements"
```

---

### Task 9: Update CI workflow to build and embed widget extension

**Files:**
- Modify: `.github/workflows/ios-release.yml`

**Step 1: Add widget build steps**

After the "Configure CMake" step and before "Build and Archive", add:

```yaml
      - name: Install XcodeGen
        run: brew install xcodegen

      - name: Build Widget Extension
        env:
          TEAM_ID: ${{ secrets.APPLE_TEAM_ID }}
        run: |
          cd ios/PowerWidget
          xcodegen generate

          xcodebuild build \
            -project PowerWidget.xcodeproj \
            -scheme PowerWidgetExtension \
            -configuration Release \
            -destination "generic/platform=iOS" \
            -derivedDataPath $RUNNER_TEMP/widget-build \
            CODE_SIGN_STYLE=Manual \
            DEVELOPMENT_TEAM=$TEAM_ID \
            CODE_SIGN_IDENTITY="iPhone Distribution" \
            PROVISIONING_PROFILE_SPECIFIER="${{ secrets.WIDGET_PROVISIONING_PROFILE_NAME }}" \
            MARKETING_VERSION="${{ env.APP_VERSION }}" \
            CURRENT_PROJECT_VERSION="${{ steps.bump.outputs.build_code }}"
```

**Step 2: Embed the widget in the archive**

After the "Build and Archive" step, add:

```yaml
      - name: Embed Widget Extension
        run: |
          ARCHIVE=$RUNNER_TEMP/DecenzaPocket.xcarchive
          WIDGET_APPEX=$(find $RUNNER_TEMP/widget-build -name "PowerWidgetExtension.appex" -type d | head -1)

          if [ -n "$WIDGET_APPEX" ]; then
            PLUGINS_DIR="$ARCHIVE/Products/Applications/DecenzaPocket.app/PlugIns"
            mkdir -p "$PLUGINS_DIR"
            cp -R "$WIDGET_APPEX" "$PLUGINS_DIR/"
            echo "Embedded widget extension"
          else
            echo "WARNING: Widget extension not found"
          fi
```

**Step 3: Update ExportOptions.plist to include widget provisioning**

Add the widget bundle ID to the `provisioningProfiles` dict:

```xml
              <key>io.github.kulitorum.decenzapocket.PowerWidget</key>
              <string>${{ secrets.WIDGET_PROVISIONING_PROFILE_NAME }}</string>
```

**Step 4: Commit**

```bash
git add .github/workflows/ios-release.yml
git commit -m "feat: build and embed iOS widget extension in CI"
```

---

### Task 10: Apple Developer Portal setup (manual steps)

These are manual steps, not code:

1. **Enable App Groups** on the main app ID (`io.github.kulitorum.decenzapocket`):
   - Apple Developer Portal -> Identifiers -> select app ID -> App Groups -> add `group.io.github.kulitorum.decenzapocket`

2. **Create widget extension App ID** (`io.github.kulitorum.decenzapocket.PowerWidget`):
   - Identifiers -> Register new -> App IDs -> App Extensions
   - Enable App Groups with same group

3. **Enable Push Notifications** on the main app ID

4. **Create APNs key** (or certificate):
   - Keys -> Create -> Apple Push Notifications service (APNs)
   - Download the `.p8` key file

5. **Upload APNs key to Firebase**:
   - Firebase Console -> Project Settings -> Cloud Messaging -> iOS
   - Upload the `.p8` key, enter Key ID and Team ID

6. **Register iOS app in Firebase** (if not done):
   - Firebase Console -> Add app -> iOS -> bundle ID `io.github.kulitorum.decenzapocket`
   - Download `GoogleService-Info.plist` into `ios/`

7. **Create provisioning profiles**:
   - One for the main app (update existing to include App Groups + Push)
   - One for the widget extension
   - Add `WIDGET_PROVISIONING_PROFILE_NAME` and `WIDGET_PROVISIONING_PROFILE_BASE64` secrets to GitHub

8. **Regenerate main app provisioning profile** with new capabilities and update GitHub secret

---

### Summary of all files

**New files:**
- `ios/PowerWidget/PowerWidgetBundle.swift`
- `ios/PowerWidget/PowerWidget.swift`
- `ios/PowerWidget/SharedState.swift`
- `ios/PowerWidget/WebSocketCommand.swift`
- `ios/PowerWidget/Info.plist`
- `ios/PowerWidget/PowerWidgetExtension.entitlements`
- `ios/PowerWidget/project.yml` (XcodeGen)
- `ios/PowerWidget/Assets.xcassets/` (widget icons)
- `ios/DecenzaPocket.entitlements`
- `src/core/nativebridge.h`
- `src/core/nativebridge_ios.mm`
- `src/core/nativebridge_android.cpp`
- `src/core/nativebridge_stub.cpp`

**Modified files:**
- `src/core/settings.h` (remove `syncToAndroidPrefs`)
- `src/core/settings.cpp` (use `NativeBridge::syncPairingData` instead)
- `src/main.cpp` (add startup FCM registration call)
- `CMakeLists.txt` (platform-specific bridge files, entitlements)
- `.github/workflows/ios-release.yml` (widget build + embed)
- `.gitignore` (GoogleService-Info.plist)
