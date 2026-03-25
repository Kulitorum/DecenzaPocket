import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.backgroundColor

    signal pairingComplete()

    property bool isSearching: false
    property bool deviceFound: false
    property string foundDeviceName: ""
    property string foundServerUrl: ""
    property string errorMessage: ""

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

        // Found - enter TOTP
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

            Text {
                text: "Enter the TOTP code from your Decenza settings"
                color: Theme.textSecondaryColor
                font.pixelSize: Theme.bodySize
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            TextField {
                id: totpField
                placeholderText: "TOTP Code"
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
                onAccepted: loginButton.clicked()
            }

            Button {
                id: loginButton
                text: "Connect"
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                enabled: totpField.text.length >= 6
                onClicked: {
                    root.errorMessage = ""
                    Client.connectToServer(root.foundServerUrl)
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
            root.errorMessage = "No Decenza tablet found on this network"
        }
    }

    // Client connections
    Connections {
        target: Client
        function onLoginSuccess() {
            root.pairingComplete()
        }
        function onLoginFailed(error) {
            root.errorMessage = error || "Login failed"
        }
    }

    Component.onCompleted: {
        // Auto-start search
        Discovery.startSearch()
    }
}
