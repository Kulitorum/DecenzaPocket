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
                anchors.bottomMargin: 56  // Leave room for bottom bar
                anchors.topMargin: Theme.spacingMedium
                anchors.leftMargin: Theme.spacingMedium
                anchors.rightMargin: Theme.spacingMedium
                spacing: Theme.spacingMedium

                // Header
                Text {
                    text: "Decenza Pocket"
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
                    machineState: Connection.machineState
                    machinePhase: Connection.machinePhase
                    temperature: Connection.temperature
                    waterLevelMl: Connection.waterLevelMl
                    isHeating: Connection.isHeating
                    isReady: Connection.isReady
                }

                Item { Layout.fillHeight: true }

                // Power button
                PowerButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    isAwake: Connection.isAwake
                    onToggled: {
                        if (Connection.isAwake)
                            Connection.sleep()
                        else
                            Connection.wake()
                    }
                }

                // Connection indicator
                ConnectionIndicator {
                    mode: Connection.mode
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // Bottom bar with settings cog
            BottomBar {
                title: "Decenza Pocket"
                iconSource: "qrc:/resources/icons/settings.svg"
                onClicked: stackView.push(settingsView)
            }

            // Auto-connect on load via ConnectionManager
            Component.onCompleted: {
                if (Settings.isPaired) {
                    Connection.start()
                }
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

    // Handle login required (session expired)
    Connections {
        target: Connection
        function onLoginRequired() {
            stackView.replace(pairingView)
        }
    }
}
