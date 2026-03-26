import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.backgroundColor

    signal pairingComplete()

    property bool isSearching: false
    property bool deviceFound: false
    property bool needsLogin: false   // true if server has security enabled
    property bool connecting: false
    property string foundDeviceName: ""
    property string foundServerUrl: ""
    property string errorMessage: ""

    function generateUUID() {
        return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
            var r = Math.random() * 16 | 0
            var v = c === 'x' ? r : (r & 0x3 | 0x8)
            return v.toString(16)
        })
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width * 0.8
        spacing: Theme.spacingLarge

        // Title
        Text {
            text: "Decenza Pocket"
            color: Theme.textColor
            font.pixelSize: Theme.headingSize
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            text: "Connect to your Decenza tablet"
            color: Theme.textSecondaryColor
            font.pixelSize: Theme.bodySize
            Layout.alignment: Qt.AlignHCenter
        }

        Item { Layout.preferredHeight: Theme.spacingLarge }

        // Searching state
        ColumnLayout {
            visible: root.isSearching && !root.deviceFound
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingMedium

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: root.isSearching
                palette.dark: Theme.primaryColor
            }

            Text {
                text: "Searching for Decenza..."
                color: Theme.textSecondaryColor
                font.pixelSize: Theme.bodySize
                Layout.alignment: Qt.AlignHCenter
            }
        }

        // Found - show connect button, or connecting/login
        ColumnLayout {
            visible: root.deviceFound
            spacing: Theme.spacingMedium

            Text {
                text: "Found: " + root.foundDeviceName
                color: Theme.successColor
                font.pixelSize: Theme.subtitleSize
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            // Connect button (shown before connecting)
            Button {
                visible: !root.connecting && !root.needsLogin
                text: "Connect"
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                onClicked: {
                    root.errorMessage = ""
                    root.connecting = true
                    Client.connectToServer(root.foundServerUrl)
                }
                background: Rectangle {
                    color: Theme.primaryColor
                    radius: Theme.cardRadius / 2
                }
                contentItem: Text {
                    text: "Connect"
                    color: Theme.primaryContrastColor
                    font.pixelSize: Theme.bodySize
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // Connecting spinner
            ColumnLayout {
                visible: root.connecting && !root.needsLogin
                Layout.alignment: Qt.AlignHCenter
                spacing: Theme.spacingSmall

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: root.connecting
                    palette.dark: Theme.primaryColor
                }
                Text {
                    text: "Connecting..."
                    color: Theme.textSecondaryColor
                    font.pixelSize: Theme.bodySize
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // TOTP login form (shown only when server requires auth)
            ColumnLayout {
                visible: root.needsLogin
                spacing: Theme.spacingMedium

                Text {
                    text: "Enter your 6-digit authenticator code"
                    color: Theme.textColor
                    font.pixelSize: Theme.bodySize
                    font.bold: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }

                Text {
                    text: "On your Decenza tablet, go to:\nSettings → Data → Web Security\n\nOpen your authenticator app (Google Authenticator, Authy, etc.) and enter the 6-digit code shown for Decenza."
                    color: Theme.textSecondaryColor
                    font.pixelSize: Theme.labelSize
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.3
                    Layout.fillWidth: true
                }

                TextField {
                    id: totpField
                    placeholderText: "000000"
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.titleSize
                    inputMethodHints: Qt.ImhDigitsOnly
                    maximumLength: 6
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    color: Theme.textColor
                    placeholderTextColor: Theme.textSecondaryColor
                    background: Rectangle {
                        color: Theme.surfaceColor
                        radius: Theme.cardRadius / 2
                        border.color: totpField.activeFocus ? Theme.primaryColor : Theme.borderColor
                        border.width: 1
                    }
                    onAccepted: if (totpField.text.length >= 6) loginButton.clicked()
                }

                Button {
                    id: loginButton
                    text: "Connect"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    enabled: totpField.text.length >= 6
                    onClicked: {
                        root.errorMessage = ""
                        Client.login(totpField.text)
                    }
                    background: Rectangle {
                        color: loginButton.enabled ? Theme.primaryColor : Theme.surfaceColor
                        radius: Theme.cardRadius / 2
                    }
                    contentItem: Text {
                        text: loginButton.text
                        color: loginButton.enabled ? Theme.primaryContrastColor : Theme.textSecondaryColor
                        font.pixelSize: Theme.bodySize
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // Error
        Text {
            visible: root.errorMessage.length > 0
            text: root.errorMessage
            color: Theme.errorColor
            font.pixelSize: Theme.bodySize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        // Not searching, not found - show search button
        Button {
            visible: !root.isSearching && !root.deviceFound
            text: "Search for Decenza"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.6
            Layout.preferredHeight: 48
            onClicked: {
                root.errorMessage = ""
                Discovery.startSearch()
            }
            background: Rectangle {
                color: Theme.primaryColor
                radius: Theme.cardRadius / 2
            }
            contentItem: Text {
                text: "Search for Decenza"
                color: Theme.primaryContrastColor
                font.pixelSize: Theme.bodySize
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // Discovery connections
    Connections {
        target: Discovery
        function onSearchingChanged() { root.isSearching = Discovery.searching }
        function onDeviceFound(deviceName, serverUrl, port, secure) {
            root.deviceFound = true
            root.foundDeviceName = deviceName
            root.foundServerUrl = serverUrl
        }
        function onSearchFailed() {
            root.errorMessage = "No Decenza tablet found on this network.\nMake sure your phone is on the same WiFi as the tablet."
        }
    }

    // Client connections
    Connections {
        target: Client
        function onLoginRequired() {
            // Server has security enabled - show TOTP form
            root.connecting = false
            root.needsLogin = true
        }
        function onLoginSuccess() {
            // Generate a pairing token and send to tablet
            root.connecting = false
            var token = generateUUID()
            Client.pair(token)
        }
        function onLoginFailed(error) {
            root.errorMessage = error || "Invalid code. Check your authenticator app and try again."
        }
        function onPairingComplete(deviceId, deviceName) {
            // Override the deviceName with the discovered name (more reliable)
            if (root.foundDeviceName) {
                Settings.setPairedDevice(deviceId, root.foundDeviceName,
                    Settings.pairedServerUrl, Settings.pairingToken)
            }
            root.pairingComplete()
        }
        function onConnectionError(error) {
            root.connecting = false
            root.errorMessage = error
        }
    }

    Component.onCompleted: {
        // Auto-start search
        Discovery.startSearch()
    }
}
