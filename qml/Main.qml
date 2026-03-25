import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: 400
    height: 700
    title: "Decenza Pocket"
    color: Theme.backgroundColor

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: Settings.isPaired ? statusView : pairingView
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

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMedium
                spacing: Theme.spacingMedium

                // Header
                Text {
                    text: Settings.pairedDeviceName || "Decenza"
                    color: Theme.textColor
                    font.pixelSize: Theme.headingSize
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacingMedium
                }

                // Status card
                StatusCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200
                    machineState: Client.machineState
                    machinePhase: Client.machinePhase
                    temperature: Client.temperature
                    waterLevelMl: Client.waterLevelMl
                    isHeating: Client.isHeating
                    isReady: Client.isReady
                }

                Item { Layout.fillHeight: true }

                // Power button
                PowerButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    isAwake: Client.isAwake
                    onToggled: {
                        if (Client.isAwake)
                            Client.sleep()
                        else
                            Client.wake()
                    }
                }

                Item { Layout.preferredHeight: Theme.spacingSmall }

                // Connection indicator
                ConnectionIndicator {
                    mode: Client.connected ? "local" : "disconnected"
                    Layout.alignment: Qt.AlignHCenter
                }

                // Settings link
                Text {
                    text: "Settings"
                    color: Theme.primaryColor
                    font.pixelSize: Theme.bodySize
                    font.underline: true
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: Theme.spacingMedium

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            // TODO: navigate to settings page (Task 14)
                        }
                    }
                }
            }

            // Auto-connect on load
            Component.onCompleted: {
                if (Settings.isPaired) {
                    Client.connectToServer(Settings.pairedServerUrl)
                }
            }
        }
    }

    // Handle login required (session expired)
    Connections {
        target: Client
        function onLoginRequired() {
            stackView.replace(pairingView)
        }
    }
}
