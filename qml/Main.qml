import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: 800
    height: 400
    title: "Decenza Pocket"
    color: Theme.backgroundColor

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: Settings.isPaired ? statusView : pairingView
    }

    // Drop the relay connection while backgrounded so the tablet stops
    // streaming screen tiles (and we stop burning battery decoding them).
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive) {
                if (Settings.isPaired) Connection.start()
            } else {
                Connection.sendCommand("stop_remote")
                Connection.disconnect()
            }
        }
    }

    Component {
        id: pairingView
        PairingPage {
            onPairingComplete: {
                stackView.replace(statusView)
            }
        }
    }

    Component {
        id: statusView
        Rectangle {
            color: Theme.backgroundColor

            property bool landscape: width > height

            // Portrait layout
            ColumnLayout {
                visible: !parent.landscape
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: statusBottomBar.top
                anchors.bottomMargin: Theme.spacingMedium
                anchors.topMargin: Theme.spacingMedium
                anchors.leftMargin: Theme.spacingMedium
                anchors.rightMargin: Theme.spacingMedium
                spacing: Theme.spacingMedium

                Text {
                    text: "Decenza Pocket"
                    color: Theme.textColor
                    font.pixelSize: Theme.headingSize
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacingMedium
                }

                StatusCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200
                    connected: Connection.mode !== "disconnected"
                    machineState: Connection.machineState
                    machinePhase: Connection.machinePhase
                    temperature: Connection.temperature
                    waterLevelMl: Connection.waterLevelMl
                    isHeating: Connection.isHeating
                    isReady: Connection.isReady
                }

                Item { Layout.fillHeight: true }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    enabled: Connection.mode !== "disconnected"
                    onClicked: {
                        Connection.sendCommand("start_remote")
                        stackView.push(remoteControlView)
                    }
                    background: Rectangle {
                        color: parent.enabled ? Theme.surfaceColor : Theme.surfaceColor
                        radius: Theme.cardRadius / 2
                        border.color: parent.enabled ? Theme.primaryColor : Theme.borderColor
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "Remote Control"
                        color: parent.enabled ? Theme.primaryColor : Theme.textSecondaryColor
                        font.pixelSize: Theme.bodySize
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                PowerButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    isAwake: Connection.isAwake
                    connected: Connection.mode !== "disconnected"
                    onToggled: {
                        if (Connection.isAwake)
                            Connection.sleep()
                        else
                            Connection.wake()
                    }
                }

                ConnectionIndicator {
                    mode: Connection.mode
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // Landscape layout — two columns
            RowLayout {
                visible: parent.landscape
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: statusBottomBar.top
                anchors.bottomMargin: Theme.spacingMedium
                anchors.topMargin: Theme.spacingMedium
                anchors.leftMargin: Theme.spacingMedium
                anchors.rightMargin: Theme.spacingMedium
                spacing: Theme.spacingLarge

                // Left column: status
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.spacingMedium

                    Text {
                        text: "Decenza Pocket"
                        color: Theme.textColor
                        font.pixelSize: Theme.headingSize
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }

                    StatusCard {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        connected: Connection.mode !== "disconnected"
                        machineState: Connection.machineState
                        machinePhase: Connection.machinePhase
                        temperature: Connection.temperature
                        waterLevelMl: Connection.waterLevelMl
                        isHeating: Connection.isHeating
                        isReady: Connection.isReady
                    }

                    ConnectionIndicator {
                        mode: Connection.mode
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                // Right column: controls
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.spacingMedium

                    Item { Layout.fillHeight: true }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        enabled: Connection.mode !== "disconnected"
                        onClicked: {
                            Connection.sendCommand("start_remote")
                            stackView.push(remoteControlView)
                        }
                        background: Rectangle {
                            color: parent.enabled ? Theme.surfaceColor : Theme.surfaceColor
                            radius: Theme.cardRadius / 2
                            border.color: parent.enabled ? Theme.primaryColor : Theme.borderColor
                            border.width: 1
                        }
                        contentItem: Text {
                            text: "Remote Control"
                            color: parent.enabled ? Theme.primaryColor : Theme.textSecondaryColor
                            font.pixelSize: Theme.bodySize
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    PowerButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        isAwake: Connection.isAwake
                        connected: Connection.mode !== "disconnected"
                        onToggled: {
                            if (Connection.isAwake)
                                Connection.sleep()
                            else
                                Connection.wake()
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            BottomBar {
                id: statusBottomBar
                title: "Decenza Pocket"
                iconSource: "qrc:/resources/icons/settings.svg"
                onClicked: stackView.push(settingsView)
            }

            Component.onCompleted: {
                if (Settings.isPaired) {
                    Connection.start()
                }
            }
        }
    }

    Component {
        id: remoteControlView
        RemoteControlPage {
            onBack: {
                Connection.sendCommand("stop_remote")
                Connection.disconnect()
                stackView.pop()
                if (Settings.isPaired) Connection.start()
            }
        }
    }

    Component {
        id: settingsView
        SettingsPage {
            onBack: stackView.pop()
            onUnpaired: stackView.replace(pairingView)
        }
    }
}
