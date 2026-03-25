import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.backgroundColor

    signal back()
    signal unpaired()

    ColumnLayout {
        anchors.fill: parent
        anchors.bottomMargin: 56  // Leave room for bottom bar
        anchors.topMargin: Theme.spacingMedium
        anchors.leftMargin: Theme.spacingMedium
        anchors.rightMargin: Theme.spacingMedium
        spacing: Theme.spacingLarge

        // Paired device
        Rectangle {
            Layout.fillWidth: true
            height: deviceColumn.implicitHeight + Theme.spacingLarge * 2
            color: Theme.surfaceColor
            radius: Theme.cardRadius

            ColumnLayout {
                id: deviceColumn
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingSmall

                Text {
                    text: "Paired Device"
                    color: Theme.textSecondaryColor
                    font.pixelSize: Theme.labelSize
                }

                Text {
                    text: Settings.pairedDeviceName || "Not paired"
                    color: Theme.textColor
                    font.pixelSize: Theme.bodySize
                    font.bold: true
                }

                Text {
                    text: Settings.pairedDeviceId
                    color: Theme.textSecondaryColor
                    font.pixelSize: Theme.labelSize
                    visible: Settings.isPaired
                }
            }
        }

        // Notification toggle
        Rectangle {
            Layout.fillWidth: true
            height: 56
            color: Theme.surfaceColor
            radius: Theme.cardRadius

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge

                Text {
                    text: "Ready notification"
                    color: Theme.textColor
                    font.pixelSize: Theme.bodySize
                    Layout.fillWidth: true
                }

                Switch {
                    checked: Notifications.enabled
                    onToggled: Notifications.enabled = checked
                }
            }
        }

        // Theme sync
        Rectangle {
            Layout.fillWidth: true
            height: themeColumn.implicitHeight + Theme.spacingLarge * 2
            color: Theme.surfaceColor
            radius: Theme.cardRadius

            ColumnLayout {
                id: themeColumn
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingSmall

                Text {
                    text: "Theme"
                    color: Theme.textSecondaryColor
                    font.pixelSize: Theme.labelSize
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Sync with Decenza automatically"
                        color: Theme.textColor
                        font.pixelSize: Theme.bodySize
                        Layout.fillWidth: true
                    }
                    Switch {
                        checked: Settings.themeAutoSync
                        onToggled: Settings.themeAutoSync = checked
                    }
                }

                Button {
                    text: syncTimer.running ? "Syncing..." : "Sync theme now"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    enabled: Connection.mode !== "disconnected" && !syncTimer.running
                    onClicked: {
                        Client.fetchTheme()
                        syncTimer.start()
                    }
                    background: Rectangle {
                        color: parent.enabled ? Theme.primaryColor : Theme.surfaceColor
                        radius: Theme.cardRadius / 2
                        border.color: parent.enabled ? "transparent" : Theme.borderColor
                        border.width: parent.enabled ? 0 : 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? Theme.primaryContrastColor : Theme.textSecondaryColor
                        font.pixelSize: Theme.bodySize
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    Timer {
                        id: syncTimer
                        interval: 1500
                        onTriggered: running = false
                    }
                }
            }
        }

        // Connection mode
        Rectangle {
            Layout.fillWidth: true
            height: connectionColumn.implicitHeight + Theme.spacingLarge * 2
            color: Theme.surfaceColor
            radius: Theme.cardRadius

            ColumnLayout {
                id: connectionColumn
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingSmall

                Text {
                    text: "Connection"
                    color: Theme.textSecondaryColor
                    font.pixelSize: Theme.labelSize
                }

                ConnectionIndicator {
                    mode: Connection.mode
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Unpair button
        Button {
            text: "Unpair Device"
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            visible: Settings.isPaired
            onClicked: {
                Connection.disconnect()
                Settings.clearPairedDevice()
                root.unpaired()
            }
            background: Rectangle {
                color: Theme.surfaceColor
                radius: Theme.cardRadius / 2
                border.color: Theme.errorColor
                border.width: 1
            }
            contentItem: Text {
                text: "Unpair Device"
                color: Theme.errorColor
                font.pixelSize: Theme.bodySize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        // Version
        Text {
            text: "DecenzaPocket v0.1.0"
            color: Theme.textSecondaryColor
            font.pixelSize: Theme.labelSize
            Layout.alignment: Qt.AlignHCenter
        }
    }

    // Bottom bar with back button
    BottomBar {
        title: "Settings"
        showBackButton: true
        onBackClicked: root.back()
    }
}
