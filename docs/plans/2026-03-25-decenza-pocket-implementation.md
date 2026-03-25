# DecenzaPocket Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a phone companion app (iOS + Android) that shows DE1 machine status and controls power, communicating through Decenza's ShotServer locally and via AWS WebSocket relay remotely.

**Architecture:** Three codebases are involved: (1) DecenzaPocket Qt app at `C:\CODE\DecenzaPocket`, (2) Decenza tablet app at `C:\CODE\Decenza` for new ShotServer endpoints, (3) AWS backend at `C:\CODE\decenza.coffee` for WebSocket relay. The pocket app talks to Decenza's ShotServer (HTTPS port 8888) on the local network and to `wss://ws.decenza.coffee` when remote.

**Tech Stack:** Qt 6.10 / C++17 / QML (pocket app), TypeScript / Lambda / API Gateway / DynamoDB (AWS backend)

**Design doc:** `C:\CODE\DecenzaPocket\docs\plans\2026-03-25-decenza-pocket-design.md`

---

## Phase 1: DecenzaPocket App Skeleton

### Task 1: CMake project setup

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `src/version.h.in`
- Create: `versioncode.txt`
- Create: `qml/Main.qml`
- Create: `android/AndroidManifest.xml.in`
- Create: `CLAUDE.md`

**Step 1: Create `CMakeLists.txt`**

Model it on Decenza's CMakeLists.txt but stripped down. Only needs: Core, Gui, Qml, Quick, QuickControls2, Network, WebSockets, Multimedia (for notification sound).

```cmake
cmake_minimum_required(VERSION 3.21)
project(DecenzaPocket VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VERSION_CODE_FILE "${CMAKE_SOURCE_DIR}/versioncode.txt")
set(VERSION_HEADER "${CMAKE_BINARY_DIR}/version.h")

file(READ "${VERSION_CODE_FILE}" CURRENT_VERSION_CODE)
string(STRIP "${CURRENT_VERSION_CODE}" CURRENT_VERSION_CODE)

configure_file(
    "${CMAKE_SOURCE_DIR}/src/version.h.in"
    "${VERSION_HEADER}"
    @ONLY
)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2 Network WebSockets Multimedia
)

qt_standard_project_setup(REQUIRES 6.8)

set(SOURCES
    src/main.cpp
)

qt_add_executable(DecenzaPocket ${SOURCES})

qt_add_qml_module(DecenzaPocket
    URI DecenzaPocket
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_include_directories(DecenzaPocket PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(DecenzaPocket PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::Network Qt6::WebSockets Qt6::Multimedia
)

set_target_properties(DecenzaPocket PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
)

# Android
if(ANDROID)
    set(ANDROID_VERSION_NAME "${CMAKE_PROJECT_VERSION}")
    set(ANDROID_VERSION_CODE "${CURRENT_VERSION_CODE}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/android/AndroidManifest.xml.in"
        "${CMAKE_SOURCE_DIR}/android/AndroidManifest.xml"
        @ONLY
    )
endif()

# iOS
if(IOS)
    set_target_properties(DecenzaPocket PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.kulitorum.decenzapocket"
        MACOSX_BUNDLE_BUNDLE_VERSION "${CURRENT_VERSION_CODE}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CMAKE_PROJECT_VERSION}"
    )
endif()
```

**Step 2: Create `versioncode.txt`**

```
1
```

**Step 3: Create `src/version.h.in`**

```cpp
#pragma once
#define VERSION_STRING "@CMAKE_PROJECT_VERSION@"
#define VERSION_MAJOR @PROJECT_VERSION_MAJOR@
#define VERSION_MINOR @PROJECT_VERSION_MINOR@
#define VERSION_PATCH @PROJECT_VERSION_PATCH@
```

**Step 4: Create `src/main.cpp`**

Minimal entry point that loads QML.

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Decenza");
    app.setApplicationName("DecenzaPocket");

    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;
    engine.loadFromModule("DecenzaPocket", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
```

**Step 5: Create `qml/Main.qml`**

Placeholder that proves the app loads.

```qml
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    visible: true
    width: 400
    height: 700
    title: "Decenza Pocket"
    color: "#1a1a2e"

    Text {
        anchors.centerIn: parent
        text: "Decenza Pocket"
        color: "#ffffff"
        font.pixelSize: 24
    }
}
```

**Step 6: Create `android/AndroidManifest.xml.in`**

```xml
<?xml version="1.0"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="io.github.kulitorum.decenzapocket"
    android:installLocation="auto"
    android:versionCode="@ANDROID_VERSION_CODE@"
    android:versionName="@ANDROID_VERSION_NAME@">

    <uses-permission android:name="android.permission.INTERNET"/>
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE"/>
    <uses-permission android:name="android.permission.ACCESS_WIFI_STATE"/>
    <uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE"/>
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE"/>
    <uses-permission android:name="android.permission.POST_NOTIFICATIONS"/>
    <uses-permission android:name="android.permission.VIBRATE"/>

    <supports-screens
        android:anyDensity="true"
        android:largeScreens="true"
        android:normalScreens="true"
        android:smallScreens="true"/>

    <application
        android:name="org.qtproject.qt.android.bindings.QtApplication"
        android:label="Decenza Pocket"
        android:hardwareAccelerated="true"
        android:theme="@style/AppTheme">
        <activity
            android:name="org.qtproject.qt.android.bindings.QtActivity"
            android:configChanges="orientation|uiMode|screenLayout|screenSize|smallestScreenSize|layoutDirection|locale|fontScale|keyboard|keyboardHidden|navigation|mcc|mnc|density"
            android:screenOrientation="portrait"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>
    </application>
</manifest>
```

**Step 7: Create `CLAUDE.md`**

```markdown
# DecenzaPocket

Companion phone app for Decenza espresso machine controller. iOS + Android only.

## Development Environment

- **Qt version**: 6.10.2
- **Qt path (Windows)**: `C:/Qt/6.10.2/msvc2022_64`
- **C++ standard**: C++17
- **Related projects**:
  - Decenza (tablet app): `C:\CODE\Decenza`
  - AWS backend: `C:\CODE\decenza.coffee`
- **IMPORTANT**: Use relative paths (e.g., `src/main.cpp`) to avoid edit errors

## Build

Don't build automatically - let the user build in Qt Creator.

## Conventions

Same as Decenza:
- Classes: `PascalCase`, Methods/variables: `camelCase`, Members: `m_` prefix
- QML files: `PascalCase.qml`, IDs/properties: `camelCase`
- Never use timers as guards/workarounds
- Never run I/O on the main thread
```

**Step 8: Verify it builds**

Run: Open in Qt Creator and build, or:
```bash
cd C:/CODE/DecenzaPocket
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64"
cmake --build build
```

**Step 9: Commit**

```bash
git init
git add CMakeLists.txt src/ qml/ android/ versioncode.txt CLAUDE.md docs/
git commit -m "feat: initial DecenzaPocket project skeleton"
```

---

## Phase 2: Settings & Discovery

### Task 2: Settings class

**Files:**
- Create: `src/core/settings.h`
- Create: `src/core/settings.cpp`
- Modify: `CMakeLists.txt` (add to SOURCES)

**Step 1: Create settings class**

Stores paired device info and cached theme. Uses QSettings like Decenza.

```cpp
// src/core/settings.h
#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantMap>

class Settings : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString pairedDeviceId READ pairedDeviceId NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QString pairedDeviceName READ pairedDeviceName NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QString pairedServerUrl READ pairedServerUrl NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QString pairingToken READ pairingToken NOTIFY pairedDeviceChanged)
    Q_PROPERTY(bool isPaired READ isPaired NOTIFY pairedDeviceChanged)
    Q_PROPERTY(QVariantMap themeColors READ themeColors NOTIFY themeChanged)
    Q_PROPERTY(QVariantMap themeFonts READ themeFonts NOTIFY themeChanged)

public:
    explicit Settings(QObject* parent = nullptr);

    // Pairing
    QString pairedDeviceId() const;
    QString pairedDeviceName() const;
    QString pairedServerUrl() const;
    QString pairingToken() const;
    bool isPaired() const;

    Q_INVOKABLE void setPairedDevice(const QString& deviceId, const QString& name,
                                      const QString& serverUrl, const QString& pairingToken);
    Q_INVOKABLE void clearPairedDevice();

    // Theme cache
    QVariantMap themeColors() const;
    QVariantMap themeFonts() const;
    void setThemeData(const QVariantMap& colors, const QVariantMap& fonts);

    // Session
    QString sessionCookie() const;
    void setSessionCookie(const QString& cookie);

signals:
    void pairedDeviceChanged();
    void themeChanged();

private:
    QSettings m_settings;
};
```

```cpp
// src/core/settings.cpp
#include "settings.h"

Settings::Settings(QObject* parent)
    : QObject(parent)
    , m_settings("Decenza", "DecenzaPocket")
{
}

QString Settings::pairedDeviceId() const { return m_settings.value("device/id").toString(); }
QString Settings::pairedDeviceName() const { return m_settings.value("device/name").toString(); }
QString Settings::pairedServerUrl() const { return m_settings.value("device/serverUrl").toString(); }
QString Settings::pairingToken() const { return m_settings.value("device/pairingToken").toString(); }
bool Settings::isPaired() const { return !pairedDeviceId().isEmpty(); }

void Settings::setPairedDevice(const QString& deviceId, const QString& name,
                                const QString& serverUrl, const QString& pairingToken)
{
    m_settings.setValue("device/id", deviceId);
    m_settings.setValue("device/name", name);
    m_settings.setValue("device/serverUrl", serverUrl);
    m_settings.setValue("device/pairingToken", pairingToken);
    emit pairedDeviceChanged();
}

void Settings::clearPairedDevice()
{
    m_settings.remove("device");
    emit pairedDeviceChanged();
}

QVariantMap Settings::themeColors() const { return m_settings.value("theme/colors").toMap(); }
QVariantMap Settings::themeFonts() const { return m_settings.value("theme/fonts").toMap(); }

void Settings::setThemeData(const QVariantMap& colors, const QVariantMap& fonts)
{
    m_settings.setValue("theme/colors", colors);
    m_settings.setValue("theme/fonts", fonts);
    emit themeChanged();
}

QString Settings::sessionCookie() const { return m_settings.value("device/sessionCookie").toString(); }
void Settings::setSessionCookie(const QString& cookie)
{
    m_settings.setValue("device/sessionCookie", cookie);
}
```

**Step 2: Add to CMakeLists.txt SOURCES**

Add `src/core/settings.h` and `src/core/settings.cpp` to SOURCES list.

**Step 3: Register in main.cpp**

Create Settings instance and expose to QML via `qmlRegisterSingletonInstance` or `setContextProperty`.

**Step 4: Commit**

```bash
git add src/core/settings.h src/core/settings.cpp CMakeLists.txt src/main.cpp
git commit -m "feat: add Settings class for pairing and theme cache"
```

### Task 3: Network discovery

**Files:**
- Create: `src/network/discovery.h`
- Create: `src/network/discovery.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Implement UDP discovery**

Sends `DECENZA_DISCOVER` broadcast on UDP port 8889, parses JSON response. Modeled on Decenza's `onDiscoveryDatagram()` (see `C:\CODE\Decenza\src\network\shotserver.cpp:872-911`).

```cpp
// src/network/discovery.h
#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QTimer>

class Discovery : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool searching READ isSearching NOTIFY searchingChanged)
    Q_PROPERTY(QString foundDeviceName READ foundDeviceName NOTIFY deviceFound)
    Q_PROPERTY(QString foundServerUrl READ foundServerUrl NOTIFY deviceFound)

public:
    explicit Discovery(QObject* parent = nullptr);

    bool isSearching() const { return m_searching; }
    QString foundDeviceName() const { return m_foundDeviceName; }
    QString foundServerUrl() const { return m_foundServerUrl; }

    Q_INVOKABLE void startSearch();
    Q_INVOKABLE void stopSearch();

signals:
    void searchingChanged();
    void deviceFound(const QString& deviceName, const QString& serverUrl,
                     int port, bool secure);
    void searchFailed();

private slots:
    void onReadyRead();
    void onSearchTimeout();
    void sendDiscoveryBroadcast();

private:
    QUdpSocket* m_socket = nullptr;
    QTimer m_broadcastTimer;
    QTimer m_timeoutTimer;
    bool m_searching = false;
    QString m_foundDeviceName;
    QString m_foundServerUrl;

    static constexpr quint16 DISCOVERY_PORT = 8889;
};
```

```cpp
// src/network/discovery.cpp
#include "discovery.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>

Discovery::Discovery(QObject* parent)
    : QObject(parent)
{
    m_broadcastTimer.setInterval(1000); // Broadcast every second
    connect(&m_broadcastTimer, &QTimer::timeout, this, &Discovery::sendDiscoveryBroadcast);

    m_timeoutTimer.setSingleShot(true);
    m_timeoutTimer.setInterval(10000); // 10 second total search
    connect(&m_timeoutTimer, &QTimer::timeout, this, &Discovery::onSearchTimeout);
}

void Discovery::startSearch()
{
    if (m_searching) return;

    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::Any, 0)) {
        qWarning() << "Discovery: Failed to bind UDP socket";
        delete m_socket;
        m_socket = nullptr;
        emit searchFailed();
        return;
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &Discovery::onReadyRead);

    m_searching = true;
    emit searchingChanged();

    sendDiscoveryBroadcast();
    m_broadcastTimer.start();
    m_timeoutTimer.start();
}

void Discovery::stopSearch()
{
    m_broadcastTimer.stop();
    m_timeoutTimer.stop();

    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    if (m_searching) {
        m_searching = false;
        emit searchingChanged();
    }
}

void Discovery::sendDiscoveryBroadcast()
{
    if (!m_socket) return;

    QByteArray datagram = "DECENZA_DISCOVER";

    // Broadcast on all interfaces
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
            iface.flags().testFlag(QNetworkInterface::IsRunning) &&
            !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            const auto entries = iface.addressEntries();
            for (const auto& entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    QHostAddress broadcast = entry.broadcast();
                    if (!broadcast.isNull()) {
                        m_socket->writeDatagram(datagram, broadcast, DISCOVERY_PORT);
                    }
                }
            }
        }
    }

    // Also try generic broadcast
    m_socket->writeDatagram(datagram, QHostAddress::Broadcast, DISCOVERY_PORT);
}

void Discovery::onReadyRead()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());
        QHostAddress sender;
        m_socket->readDatagram(datagram.data(), datagram.size(), &sender);

        QJsonDocument doc = QJsonDocument::fromJson(datagram);
        if (doc.isNull()) continue;

        QJsonObject obj = doc.object();
        if (obj["type"].toString() != "DECENZA_SERVER") continue;

        m_foundDeviceName = obj["deviceName"].toString();
        m_foundServerUrl = obj["serverUrl"].toString();
        int port = obj["port"].toInt(8888);
        bool secure = obj["secure"].toBool(false);

        qDebug() << "Discovery: Found" << m_foundDeviceName << "at" << m_foundServerUrl;

        stopSearch();
        emit deviceFound(m_foundDeviceName, m_foundServerUrl, port, secure);
        return;
    }
}

void Discovery::onSearchTimeout()
{
    qDebug() << "Discovery: Search timed out";
    stopSearch();
    emit searchFailed();
}
```

**Step 2: Add to CMakeLists.txt, register in main.cpp**

**Step 3: Commit**

```bash
git add src/network/discovery.h src/network/discovery.cpp CMakeLists.txt src/main.cpp
git commit -m "feat: add UDP network discovery for Decenza tablet"
```

### Task 4: ShotServer HTTP client

**Files:**
- Create: `src/network/decenzaclient.h`
- Create: `src/network/decenzaclient.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Implement the HTTP client**

Wraps all ShotServer API calls. Uses `QNetworkAccessManager`. Handles TOTP auth, session cookies, and SSL (self-signed cert acceptance). Key endpoints from `C:\CODE\Decenza\src\network\shotserver.cpp:1510-1618`:

- `GET /api/power/status` -> `{connected, state, substate, awake}`
- `GET /api/power/wake` -> `{success, action: "wake"}`
- `GET /api/power/sleep` -> `{success, action: "sleep"}`
- `GET /api/state` -> `{connected, state, substate, phase, isFlowing, isHeating, isReady}`
- `GET /api/telemetry` -> `{temperature, waterLevel, waterLevelMl, state, phase, ...}`
- `GET /api/theme` -> full theme JSON (already exists, `shotserver_theme.cpp:97`)
- `POST /api/auth/login` -> TOTP login (exists in `shotserver_auth.cpp`)

```cpp
// src/network/decenzaclient.h
#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>

class Settings;

class DecenzaClient : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(bool polling READ isPolling NOTIFY pollingChanged)
    Q_PROPERTY(QString machineState READ machineState NOTIFY statusUpdated)
    Q_PROPERTY(QString machinePhase READ machinePhase NOTIFY statusUpdated)
    Q_PROPERTY(double temperature READ temperature NOTIFY statusUpdated)
    Q_PROPERTY(double waterLevelMl READ waterLevelMl NOTIFY statusUpdated)
    Q_PROPERTY(bool isHeating READ isHeating NOTIFY statusUpdated)
    Q_PROPERTY(bool isReady READ isReady NOTIFY statusUpdated)
    Q_PROPERTY(bool isAwake READ isAwake NOTIFY statusUpdated)

public:
    explicit DecenzaClient(Settings* settings, QObject* parent = nullptr);

    bool isConnected() const { return m_connected; }
    bool isPolling() const { return m_pollTimer.isActive(); }

    QString machineState() const { return m_machineState; }
    QString machinePhase() const { return m_machinePhase; }
    double temperature() const { return m_temperature; }
    double waterLevelMl() const { return m_waterLevelMl; }
    bool isHeating() const { return m_isHeating; }
    bool isReady() const { return m_isReady; }
    bool isAwake() const { return m_isAwake; }

    Q_INVOKABLE void connectToServer(const QString& serverUrl);
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void login(const QString& totpCode);
    Q_INVOKABLE void wake();
    Q_INVOKABLE void sleep();
    Q_INVOKABLE void fetchTheme();

signals:
    void connectedChanged();
    void pollingChanged();
    void statusUpdated();
    void loginRequired();
    void loginSuccess();
    void loginFailed(const QString& error);
    void themeReceived(const QVariantMap& colors, const QVariantMap& fonts);
    void commandSent(const QString& command, bool success);
    void connectionError(const QString& error);
    void readyNotification(); // Machine transitioned to Ready

private slots:
    void pollStatus();

private:
    void get(const QString& path, std::function<void(const QJsonObject&)> callback);
    void post(const QString& path, const QByteArray& body,
              std::function<void(const QJsonObject&)> callback);

    Settings* m_settings;
    QNetworkAccessManager* m_nam;
    QTimer m_pollTimer;
    QString m_serverUrl;
    bool m_connected = false;

    // Cached status
    QString m_machineState;
    QString m_machinePhase;
    double m_temperature = 0;
    double m_waterLevelMl = 0;
    bool m_isHeating = false;
    bool m_isReady = false;
    bool m_isAwake = false;
    bool m_wasHeating = false; // For ready notification edge detection
};
```

The implementation should:
- Accept self-signed certs (like Decenza's ShotServer uses)
- Include session cookie in requests
- Start polling `/api/state` + `/api/telemetry` every 3 seconds on successful auth
- Detect Heating -> Ready transition and emit `readyNotification()`
- On 401 response, emit `loginRequired()`

**Step 2: Add to CMakeLists.txt, wire up in main.cpp**

**Step 3: Commit**

```bash
git add src/network/decenzaclient.h src/network/decenzaclient.cpp CMakeLists.txt src/main.cpp
git commit -m "feat: add DecenzaClient for ShotServer HTTP communication"
```

---

## Phase 3: QML UI

### Task 5: Theme singleton

**Files:**
- Create: `qml/Theme.qml`
- Modify: `CMakeLists.txt` (add to QML_FILES)

**Step 1: Create Theme.qml**

Reads cached theme from Settings with Decenza defaults as fallback. Reference: `C:\CODE\Decenza\qml\Theme.qml:61-88` for default color values.

```qml
pragma Singleton
import QtQuick

QtObject {
    // Scale for phone
    property real scale: 1.0

    // Colors - from cached theme or Decenza defaults
    property color backgroundColor: _color("backgroundColor", "#1a1a2e")
    property color surfaceColor: _color("surfaceColor", "#303048")
    property color primaryColor: _color("primaryColor", "#4e85f4")
    property color secondaryColor: _color("secondaryColor", "#c0c5e3")
    property color textColor: _color("textColor", "#ffffff")
    property color textSecondaryColor: _color("textSecondaryColor", "#a0a8b8")
    property color accentColor: _color("accentColor", "#e94560")
    property color successColor: _color("successColor", "#00cc6d")
    property color warningColor: _color("warningColor", "#ffaa00")
    property color errorColor: _color("errorColor", "#ff4444")
    property color borderColor: _color("borderColor", "#3a3a4e")
    property color primaryContrastColor: _color("primaryContrastColor", "#ffffff")

    // Font sizes
    property int headingSize: _font("headingSize", 28)
    property int titleSize: _font("titleSize", 22)
    property int subtitleSize: _font("subtitleSize", 16)
    property int bodySize: _font("bodySize", 16)
    property int labelSize: _font("labelSize", 13)
    property int valueSize: _font("valueSize", 48)

    // Spacing
    property int spacingSmall: 8
    property int spacingMedium: 16
    property int spacingLarge: 24

    // Radius
    property int cardRadius: 16

    function _color(name, fallback) {
        var cached = Settings.themeColors[name]
        return cached ? cached : fallback
    }

    function _font(name, fallback) {
        var cached = Settings.themeFonts[name]
        return cached > 0 ? cached : fallback
    }
}
```

**Step 2: Register as singleton in `qml/qmldir` or CMakeLists.txt**

**Step 3: Commit**

```bash
git add qml/Theme.qml CMakeLists.txt
git commit -m "feat: add Theme singleton with cached Decenza theme"
```

### Task 6: Main UI

**Files:**
- Modify: `qml/Main.qml`
- Create: `qml/StatusCard.qml`
- Create: `qml/PowerButton.qml`
- Create: `qml/ConnectionIndicator.qml`
- Create: `qml/PairingPage.qml`
- Modify: `CMakeLists.txt`

**Step 1: Create `StatusCard.qml`**

Shows machine state (color-coded), temperature, water level.

State color mapping:
- Sleep/Disconnected: `Theme.textSecondaryColor`
- Heating: `Theme.warningColor`
- Ready/Idle: `Theme.successColor`
- Espresso/Steam/HotWater/Flushing: `Theme.primaryColor`
- Error: `Theme.errorColor`

```qml
// qml/StatusCard.qml
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.surfaceColor
    radius: Theme.cardRadius

    property string machineState: "Disconnected"
    property string machinePhase: ""
    property double temperature: 0
    property double waterLevelMl: 0
    property bool isHeating: false
    property bool isReady: false

    function stateColor() {
        if (machineState === "Sleep" || machineState === "Disconnected")
            return Theme.textSecondaryColor
        if (isHeating) return Theme.warningColor
        if (isReady) return Theme.successColor
        return Theme.primaryColor
    }

    function displayState() {
        if (machinePhase && machinePhase !== "Disconnected")
            return machinePhase
        return machineState || "Disconnected"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingMedium

        // State indicator
        RowLayout {
            spacing: Theme.spacingSmall
            Rectangle {
                width: 12; height: 12; radius: 6
                color: root.stateColor()
            }
            Text {
                text: root.displayState()
                color: root.stateColor()
                font.pixelSize: Theme.titleSize
                font.bold: true
            }
        }

        // Temperature
        Text {
            text: root.temperature > 0
                  ? root.temperature.toFixed(1) + "°C"
                  : "--°C"
            color: Theme.textColor
            font.pixelSize: Theme.valueSize
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        // Water level
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingSmall
            Text {
                text: "Water"
                color: Theme.textSecondaryColor
                font.pixelSize: Theme.bodySize
            }
            Text {
                text: root.waterLevelMl > 0
                      ? Math.round(root.waterLevelMl) + " ml"
                      : "-- ml"
                color: Theme.textColor
                font.pixelSize: Theme.bodySize
                font.bold: true
            }
        }
    }
}
```

**Step 2: Create `PowerButton.qml`**

Big toggle button. Shows "Turn On" when asleep, "Turn Off" when awake.

```qml
// qml/PowerButton.qml
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    radius: Theme.cardRadius
    color: root.isAwake ? Theme.surfaceColor : Theme.primaryColor

    property bool isAwake: false
    property bool enabled: true
    signal toggled()

    Text {
        anchors.centerIn: parent
        text: root.isAwake ? "Turn Off" : "Turn On"
        color: root.isAwake ? Theme.textColor : Theme.primaryContrastColor
        font.pixelSize: Theme.titleSize
        font.bold: true
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        onClicked: root.toggled()
    }
}
```

**Step 3: Create `ConnectionIndicator.qml`**

Shows connection mode: local, remote, or disconnected.

**Step 4: Create `PairingPage.qml`**

Shown on first launch. Discovery + TOTP login flow:
1. "Searching for Decenza..." with spinner
2. Found -> show device name + TOTP input field
3. On login success -> save pairing, switch to main view

**Step 5: Update `Main.qml`**

Wire everything together with a StackView: PairingPage (if not paired) or main status view.

**Step 6: Commit**

```bash
git add qml/*.qml CMakeLists.txt
git commit -m "feat: add main UI - status card, power button, pairing flow"
```

---

## Phase 4: Ready Notification

### Task 7: Notification sound

**Files:**
- Create: `src/core/notificationmanager.h`
- Create: `src/core/notificationmanager.cpp`
- Create: `resources/sounds/ready.wav` (placeholder - use a short pleasant chime)
- Create: `resources/resources.qrc`
- Modify: `CMakeLists.txt`

**Step 1: Implement NotificationManager**

Uses `QSoundEffect` to play a sound when the machine transitions to Ready. Connects to `DecenzaClient::readyNotification()`.

```cpp
// src/core/notificationmanager.h
#pragma once

#include <QObject>
#include <QSoundEffect>

class NotificationManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit NotificationManager(QObject* parent = nullptr);

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    Q_INVOKABLE void playReadySound();

signals:
    void enabledChanged();

private:
    QSoundEffect m_sound;
    bool m_enabled = true;
};
```

The implementation loads `qrc:/sounds/ready.wav` and plays it. Also triggers a system notification on Android (via JNI) and iOS (via local notification) so it works when backgrounded.

**Step 2: Wire to DecenzaClient in main.cpp**

```cpp
connect(&client, &DecenzaClient::readyNotification,
        &notifications, &NotificationManager::playReadySound);
```

**Step 3: Commit**

```bash
git add src/core/notificationmanager.h src/core/notificationmanager.cpp resources/ CMakeLists.txt
git commit -m "feat: add ready notification sound when machine finishes heating"
```

---

## Phase 5: AWS WebSocket Relay (Backend)

### Task 8: Extend WsConnections DynamoDB table

**Files:**
- Modify: `C:\CODE\decenza.coffee\infra\dynamodb.tf` (add GSI)

**Step 1: Add deviceId GSI to ws_connections table**

In `C:\CODE\decenza.coffee\infra\dynamodb.tf`, add to the `ws_connections` resource:

```hcl
  attribute {
    name = "device_id"
    type = "S"
  }

  global_secondary_index {
    name            = "DeviceIdIndex"
    hash_key        = "device_id"
    projection_type = "ALL"
  }
```

**Step 2: Apply terraform**

```bash
cd C:/CODE/decenza.coffee/infra
terraform plan
terraform apply
```

**Step 3: Commit**

```bash
cd C:/CODE/decenza.coffee
git add infra/dynamodb.tf
git commit -m "feat: add deviceId GSI to WebSocket connections table for relay"
```

### Task 9: Extend WebSocket handlers for relay

**Files:**
- Modify: `C:\CODE\decenza.coffee\backend\shared\types.ts`
- Modify: `C:\CODE\decenza.coffee\backend\shared\validate.ts`
- Modify: `C:\CODE\decenza.coffee\backend\shared\dynamo.ts`
- Modify: `C:\CODE\decenza.coffee\backend\shared\broadcast.ts`
- Modify: `C:\CODE\decenza.coffee\backend\lambdas\wsConnect.ts`
- Modify: `C:\CODE\decenza.coffee\backend\lambdas\wsMessage.ts`

**Step 1: Extend types.ts**

Add relay types:

```typescript
// Add to types.ts

/** Extended WsConnection with relay fields */
export interface WsConnection {
  connection_id: string;
  connected_at: number;
  ttl: number;
  filters?: {
    country_code?: string;
  };
  // Relay fields (optional - only for relay connections)
  device_id?: string;
  role?: 'device' | 'controller';
  pairing_token_hash?: string;
}

/** Relay command from controller to device */
export interface RelayCommand {
  type: 'relay_command';
  command_id: string;
  command: 'wake' | 'sleep' | 'status';
}

/** Relay response from device to controller */
export interface RelayResponse {
  type: 'relay_response';
  command_id: string;
  success: boolean;
  data?: Record<string, unknown>;
}

/** Status push from device to controllers */
export interface RelayStatus {
  type: 'relay_status';
  device_id: string;
  state: string;
  phase: string;
  temperature: number;
  waterLevelMl: number;
  isHeating: boolean;
  isReady: boolean;
  isAwake: boolean;
  timestamp: string;
}
```

**Step 2: Extend validate.ts**

Add relay message validation:

```typescript
// Add to validate.ts

export const wsMessageSchema = z.discriminatedUnion('action', [
  z.object({
    action: z.literal('subscribe'),
    filters: z.object({
      country_code: z.string().length(2).toUpperCase().optional(),
    }).optional(),
  }),
  z.object({ action: z.literal('unsubscribe') }),
  z.object({ action: z.literal('ping') }),
  // Relay actions
  z.object({
    action: z.literal('register'),
    device_id: z.string().uuid(),
    role: z.enum(['device', 'controller']),
    pairing_token: z.string().min(1).max(128),
  }),
  z.object({
    action: z.literal('command'),
    device_id: z.string().uuid(),
    pairing_token: z.string().min(1).max(128),
    command: z.enum(['wake', 'sleep', 'status']),
  }),
  z.object({
    action: z.literal('status_push'),
    state: z.string(),
    phase: z.string(),
    temperature: z.number(),
    waterLevelMl: z.number(),
    isHeating: z.boolean(),
    isReady: z.boolean(),
    isAwake: z.boolean(),
  }),
  z.object({
    action: z.literal('command_response'),
    command_id: z.string(),
    success: z.boolean(),
    data: z.record(z.unknown()).optional(),
  }),
]);
```

**Step 3: Extend dynamo.ts**

Add relay-specific DynamoDB operations:

```typescript
// Add to dynamo.ts

export async function putConnectionWithDevice(
  connectionId: string,
  deviceId: string,
  role: 'device' | 'controller',
  pairingTokenHash: string,
  filters?: { country_code?: string }
): Promise<void> {
  const now = Date.now();
  const ttl = Math.floor(now / 1000) + CONNECTION_TTL_SECONDS;
  await docClient.send(new PutCommand({
    TableName: CONNECTIONS_TABLE,
    Item: {
      connection_id: connectionId,
      connected_at: now,
      ttl,
      filters,
      device_id: deviceId,
      role,
      pairing_token_hash: pairingTokenHash,
    },
  }));
}

export async function getConnectionsByDeviceId(
  deviceId: string,
  role?: string
): Promise<WsConnection[]> {
  const params: Record<string, unknown> = { ':deviceId': deviceId };
  let filterExpr: string | undefined;
  if (role) {
    filterExpr = '#role = :role';
    params[':role'] = role;
  }

  const response = await docClient.send(new QueryCommand({
    TableName: CONNECTIONS_TABLE,
    IndexName: 'DeviceIdIndex',
    KeyConditionExpression: 'device_id = :deviceId',
    FilterExpression: filterExpr,
    ExpressionAttributeValues: params,
    ...(filterExpr ? { ExpressionAttributeNames: { '#role': 'role' } } : {}),
  }));
  return (response.Items || []) as WsConnection[];
}
```

**Step 4: Extend wsConnect.ts**

Accept `deviceId` and `role` query params:

```typescript
// Updated wsConnect.ts
export async function handler(event: APIGatewayProxyWebsocketEventV2): Promise<APIGatewayProxyResultV2> {
  const connectionId = event.requestContext.connectionId;
  const queryParams = event.queryStringParameters || {};
  const deviceId = queryParams.device_id;
  const role = queryParams.role as 'device' | 'controller' | undefined;

  console.log(`WebSocket connect: ${connectionId} device=${deviceId} role=${role}`);

  try {
    if (deviceId && role) {
      // Relay connection - store with device info (token verified on first message)
      await putConnectionWithDevice(connectionId, deviceId, role, '');
    } else {
      // Regular shot map connection
      await putConnection(connectionId);
    }
    return { statusCode: 200, body: 'Connected' };
  } catch (error) {
    console.error('Failed to store connection:', error);
    return { statusCode: 500, body: 'Failed to connect' };
  }
}
```

**Step 5: Extend wsMessage.ts**

Add relay message handling:

```typescript
// Add relay cases to wsMessage.ts switch

case 'register': {
  // Device or controller registers with pairing token
  const tokenHash = createHash('sha256')
    .update(message.pairing_token).digest('hex');
  await putConnectionWithDevice(
    connectionId, message.device_id, message.role, tokenHash
  );
  await sendToConnection(connectionId, {
    type: 'registered',
    device_id: message.device_id,
    role: message.role,
  });
  break;
}

case 'command': {
  // Controller sends command to device
  const tokenHash = createHash('sha256')
    .update(message.pairing_token).digest('hex');

  // Find the device connection
  const devices = await getConnectionsByDeviceId(message.device_id, 'device');
  const device = devices.find(d => d.pairing_token_hash === tokenHash);

  if (!device) {
    await sendToConnection(connectionId, {
      type: 'error',
      error: 'Device not connected or invalid pairing token',
    });
    break;
  }

  // Forward command to device
  const commandId = randomUUID();
  await sendToConnection(device.connection_id, {
    type: 'relay_command',
    command_id: commandId,
    command: message.command,
    from_connection: connectionId,
  });

  await sendToConnection(connectionId, {
    type: 'command_sent',
    command_id: commandId,
  });
  break;
}

case 'command_response': {
  // Device sends response back - forward to all controllers for this device
  const conn = await getConnection(connectionId);
  if (!conn?.device_id) break;

  const controllers = await getConnectionsByDeviceId(conn.device_id, 'controller');
  for (const ctrl of controllers) {
    await sendToConnection(ctrl.connection_id, {
      type: 'relay_response',
      command_id: message.command_id,
      success: message.success,
      data: message.data,
    });
  }
  break;
}

case 'status_push': {
  // Device pushes status update - forward to all controllers
  const conn = await getConnection(connectionId);
  if (!conn?.device_id) break;

  const controllers = await getConnectionsByDeviceId(conn.device_id, 'controller');
  for (const ctrl of controllers) {
    await sendToConnection(ctrl.connection_id, {
      type: 'relay_status',
      device_id: conn.device_id,
      ...message,
    });
  }
  break;
}
```

**Step 6: Build and deploy**

```bash
cd C:/CODE/decenza.coffee/backend
npm run build
cd ../infra
terraform apply
```

**Step 7: Commit**

```bash
cd C:/CODE/decenza.coffee
git add backend/ infra/
git commit -m "feat: add WebSocket relay for DecenzaPocket remote control"
```

---

## Phase 6: AWS Relay Client in Decenza

### Task 10: Add relay client to Decenza tablet app

**Files:**
- Create: `C:\CODE\Decenza\src\network\relayclient.h`
- Create: `C:\CODE\Decenza\src\network\relayclient.cpp`
- Modify: `C:\CODE\Decenza\src\network\shotserver.h` (add pocket pair endpoint)
- Modify: `C:\CODE\Decenza\src\network\shotserver.cpp` (add pocket pair + theme export routes)
- Modify: `C:\CODE\Decenza\src\main.cpp` (wire relay client)
- Modify: `C:\CODE\Decenza\CMakeLists.txt` (add WebSockets module + new files)

**Step 1: Create RelayClient**

Maintains WebSocket to `wss://ws.decenza.coffee`. Registers with `deviceId` + `pairingToken`. Processes relay commands and pushes status updates.

```cpp
// C:\CODE\Decenza\src\network\relayclient.h
#pragma once

#include <QObject>
#include <QWebSocket>
#include <QTimer>

class DE1Device;
class MachineState;
class Settings;

class RelayClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit RelayClient(DE1Device* device, MachineState* machineState,
                         Settings* settings, QObject* parent = nullptr);

    bool isConnected() const;
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

signals:
    void connectedChanged();
    void enabledChanged();

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onStateChanged();
    void onReconnectTimer();
    void onPingTimer();

private:
    void connectToRelay();
    void handleCommand(const QString& commandId, const QString& command);
    void pushStatus();

    QWebSocket m_socket;
    QTimer m_reconnectTimer;
    QTimer m_pingTimer;
    QTimer m_statusPushTimer;
    DE1Device* m_device;
    MachineState* m_machineState;
    Settings* m_settings;
    bool m_enabled = false;
    int m_reconnectAttempts = 0;
};
```

Key behaviors:
- Connects to `wss://ws.decenza.coffee/prod?device_id={id}&role=device` (the `/prod` matches the WebSocket API Gateway stage name from terraform)
- Sends `register` message with pairing token
- On `relay_command` received, calls `m_device->wakeUp()` / `m_device->goToSleep()` / builds status JSON
- Sends `command_response` back
- Pushes `status_push` every 5 seconds when state has changed
- Reconnects with exponential backoff
- Sends `ping` every 5 minutes to keep connection alive (WebSocket idle timeout is 10 min on API Gateway)

**Step 2: Add pairing endpoint to ShotServer**

In `shotserver.cpp`, add route handling for `POST /api/pocket/pair`:

```cpp
else if (path == "/api/pocket/pair" && method == "POST") {
    qsizetype bodyStart = request.indexOf("\r\n\r\n");
    if (bodyStart >= 0) {
        QByteArray body = request.mid(bodyStart + 4);
        QJsonDocument doc = QJsonDocument::fromJson(body);
        QString pairingToken = doc.object()["pairingToken"].toString();

        if (pairingToken.isEmpty()) {
            sendResponse(socket, 400, "application/json",
                R"({"error":"Missing pairingToken"})");
            return;
        }

        // Store pairing token in settings
        m_settings->setPocketPairingToken(pairingToken);

        // Enable relay client
        if (m_relayClient) {
            m_relayClient->setEnabled(true);
        }

        QJsonObject result;
        result["success"] = true;
        result["deviceId"] = m_settings->deviceId();
        result["deviceName"] = QSysInfo::machineHostName();
        sendJson(socket, QJsonDocument(result).toJson(QJsonDocument::Compact));
    }
}
```

Also add `GET /api/pocket/status` as a lightweight status endpoint (combines state + telemetry for the pocket app):

```cpp
else if (path == "/api/pocket/status") {
    QJsonObject result;
    if (m_device) {
        result["connected"] = m_device->isConnected();
        result["state"] = m_device->stateString();
        result["temperature"] = m_device->temperature();
        result["waterLevelMl"] = m_device->waterLevelMl();
        bool awake = m_device->isConnected() &&
                     m_device->state() != DE1::State::Sleep &&
                     m_device->state() != DE1::State::GoingToSleep;
        result["isAwake"] = awake;
    }
    if (m_machineState) {
        result["phase"] = m_machineState->phaseString();
        result["isHeating"] = m_machineState->isHeating();
        result["isReady"] = m_machineState->isReady();
    }
    sendJson(socket, QJsonDocument(result).toJson(QJsonDocument::Compact));
}
```

**Step 3: Add `pocketPairingToken` to Decenza's Settings class**

In `C:\CODE\Decenza\src\core\settings.h` add:
```cpp
QString pocketPairingToken() const;
void setPocketPairingToken(const QString& token);
```

Implementation reads/writes `"pocket/pairingToken"` in QSettings.

**Step 4: Wire in main.cpp**

After ShotServer is created, create RelayClient and connect it. Only activate when pairing token exists.

**Step 5: Add Qt WebSockets to Decenza's CMakeLists.txt**

Decenza may already have Network but needs `find_package(Qt6 COMPONENTS ... WebSockets)` and `target_link_libraries(... Qt6::WebSockets)`.

**Step 6: Commit (in Decenza repo)**

```bash
cd C:/CODE/Decenza
git add src/network/relayclient.h src/network/relayclient.cpp
git add src/network/shotserver.h src/network/shotserver.cpp
git add src/core/settings.h src/core/settings.cpp
git add src/main.cpp CMakeLists.txt
git commit -m "feat: add pocket app pairing and AWS relay client"
```

---

## Phase 7: AWS Relay Client in DecenzaPocket

### Task 11: Add relay client to pocket app

**Files:**
- Create: `C:\CODE\DecenzaPocket\src\network\relayclient.h`
- Create: `C:\CODE\DecenzaPocket\src\network\relayclient.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`

**Step 1: Implement pocket-side relay client**

Connects to `wss://ws.decenza.coffee/prod?device_id={id}&role=controller`. Sends commands and receives status pushes. Falls back from local to remote automatically:

- On startup, try local discovery first
- If local discovery fails and we have a paired device, connect via relay
- If local connection is lost, switch to relay
- If local connection becomes available again, switch back

```cpp
// src/network/relayclient.h (pocket version)
#pragma once

#include <QObject>
#include <QWebSocket>
#include <QTimer>

class Settings;

class RelayClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)

public:
    explicit RelayClient(Settings* settings, QObject* parent = nullptr);

    bool isConnected() const;

    Q_INVOKABLE void connectToRelay();
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void sendCommand(const QString& command);

signals:
    void connectedChanged();
    void statusReceived(const QString& state, const QString& phase,
                        double temperature, double waterLevelMl,
                        bool isHeating, bool isReady, bool isAwake);
    void commandResult(const QString& commandId, bool success);
    void readyNotification(); // Machine transitioned to Ready
    void connectionError(const QString& error);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onReconnectTimer();
    void onPingTimer();

private:
    QWebSocket m_socket;
    QTimer m_reconnectTimer;
    QTimer m_pingTimer;
    Settings* m_settings;
    int m_reconnectAttempts = 0;
    bool m_wasHeating = false; // For ready notification edge detection
};
```

**Step 2: Create ConnectionManager**

Orchestrates local vs. remote:

```cpp
// src/network/connectionmanager.h
#pragma once

#include <QObject>

class Discovery;
class DecenzaClient;
class RelayClient;
class Settings;

class ConnectionManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged) // "local", "remote", "disconnected"
    Q_PROPERTY(QString machineState READ machineState NOTIFY statusChanged)
    Q_PROPERTY(QString machinePhase READ machinePhase NOTIFY statusChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY statusChanged)
    Q_PROPERTY(double waterLevelMl READ waterLevelMl NOTIFY statusChanged)
    Q_PROPERTY(bool isHeating READ isHeating NOTIFY statusChanged)
    Q_PROPERTY(bool isReady READ isReady NOTIFY statusChanged)
    Q_PROPERTY(bool isAwake READ isAwake NOTIFY statusChanged)

public:
    explicit ConnectionManager(Settings* settings, QObject* parent = nullptr);

    QString mode() const { return m_mode; }
    // ... property getters that delegate to active client

    Q_INVOKABLE void start(); // Begin discovery -> local -> remote fallback
    Q_INVOKABLE void wake();
    Q_INVOKABLE void sleep();

signals:
    void modeChanged();
    void statusChanged();
    void readyNotification();
    void loginRequired();

private:
    Settings* m_settings;
    Discovery* m_discovery;
    DecenzaClient* m_localClient;
    RelayClient* m_remoteClient;
    QString m_mode = "disconnected";
};
```

**Step 3: Wire in main.cpp, expose to QML**

**Step 4: Update QML to use ConnectionManager**

Replace direct DecenzaClient references with ConnectionManager properties. The UI doesn't care whether it's local or remote.

**Step 5: Commit**

```bash
git add src/network/relayclient.h src/network/relayclient.cpp
git add src/network/connectionmanager.h src/network/connectionmanager.cpp
git add CMakeLists.txt src/main.cpp qml/Main.qml
git commit -m "feat: add AWS relay client and connection manager with local/remote fallback"
```

---

## Phase 8: Integration & Polish

### Task 12: Theme sync on local connection

**Files:**
- Modify: `src/network/decenzaclient.cpp` (fetch theme on connect)
- Modify: `src/core/settings.cpp` (persist theme)

**Step 1: Auto-fetch theme**

After successful TOTP login in `DecenzaClient`, automatically call `GET /api/theme` (already exists on ShotServer at `C:\CODE\Decenza\src\network\shotserver_theme.cpp:97`). Parse the `colors` and `fonts` objects and save to Settings.

**Step 2: Commit**

```bash
git add src/network/decenzaclient.cpp src/core/settings.cpp
git commit -m "feat: auto-sync theme from Decenza on local connection"
```

### Task 13: Pairing flow integration

**Files:**
- Modify: `qml/PairingPage.qml`
- Modify: `src/network/decenzaclient.cpp`

**Step 1: Complete pairing flow**

After TOTP login succeeds:
1. Generate random pairing token: `QUuid::createUuid().toString(QUuid::WithoutBraces)`
2. POST to `/api/pocket/pair` with the token
3. Save `deviceId`, `deviceName`, `serverUrl`, `pairingToken` to Settings
4. Navigate to main status view

**Step 2: Commit**

```bash
git add qml/PairingPage.qml src/network/decenzaclient.cpp
git commit -m "feat: complete pairing flow with token exchange"
```

### Task 14: Settings page

**Files:**
- Create: `qml/SettingsPage.qml`
- Modify: `qml/Main.qml`

**Step 1: Create settings page**

Minimal settings:
- Paired device name + "Unpair" button
- Ready notification toggle
- Connection mode indicator (local/remote)
- App version

**Step 2: Commit**

```bash
git add qml/SettingsPage.qml qml/Main.qml
git commit -m "feat: add settings page with unpair and notification toggle"
```

---

## Summary of Cross-Project Changes

| Project | Changes |
|---------|---------|
| **DecenzaPocket** (`C:\CODE\DecenzaPocket`) | New Qt app: CMake, Settings, Discovery, DecenzaClient, RelayClient, ConnectionManager, QML UI, NotificationManager |
| **Decenza** (`C:\CODE\Decenza`) | New: `relayclient.h/cpp`, add `POST /api/pocket/pair` + `GET /api/pocket/status` to ShotServer, add `pocketPairingToken` to Settings, add WebSockets to CMake |
| **decenza.coffee** (`C:\CODE\decenza.coffee`) | Extend WsConnections GSI, extend wsConnect/wsMessage/types/validate/dynamo for relay routing |

## Execution Order

Tasks 1-7 (DecenzaPocket app) can be done first as they only need existing ShotServer endpoints.
Tasks 8-9 (AWS backend) and Task 10 (Decenza relay) can be done in parallel.
Task 11 (pocket relay) depends on 8-10.
Tasks 12-14 are polish and integration.
