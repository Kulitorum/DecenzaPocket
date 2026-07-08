import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.backgroundColor

    signal back()
    signal unpaired()

    component DeviceCard: Rectangle {
        implicitHeight: col.implicitHeight + Theme.spacingLarge * 2
        color: Theme.surfaceColor
        radius: Theme.cardRadius

        ColumnLayout {
            id: col
            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: Theme.spacingSmall

            Text { text: "Paired Device"; color: Theme.textSecondaryColor; font.pixelSize: Theme.labelSize }
            Text {
                text: Settings.pairedDeviceName || "Not paired"
                color: Theme.textColor; font.pixelSize: Theme.bodySize; font.bold: true
                Layout.fillWidth: true; elide: Text.ElideRight
            }
            Text {
                text: Settings.pairedDeviceId
                color: Theme.textSecondaryColor; font.pixelSize: Theme.labelSize
                visible: Settings.isPaired
                Layout.fillWidth: true; elide: Text.ElideMiddle
            }
        }
    }

    component NotificationCard: Rectangle {
        implicitHeight: 56
        color: Theme.surfaceColor
        radius: Theme.cardRadius

        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            Text { text: "Ready notification"; color: Theme.textColor; font.pixelSize: Theme.bodySize; Layout.fillWidth: true }
            Switch { checked: Notifications.enabled; onToggled: Notifications.enabled = checked }
        }
    }

    component ThemeCard: Rectangle {
        implicitHeight: col.implicitHeight + Theme.spacingLarge * 2
        color: Theme.surfaceColor
        radius: Theme.cardRadius

        ColumnLayout {
            id: col
            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: Theme.spacingSmall

            Text { text: "Theme"; color: Theme.textSecondaryColor; font.pixelSize: Theme.labelSize }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Sync with Decenza automatically"
                    color: Theme.textColor; font.pixelSize: Theme.bodySize
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                }
                Switch { checked: Settings.themeAutoSync; onToggled: Settings.themeAutoSync = checked }
            }

            Button {
                text: syncTimer.running ? "Syncing..." : "Sync theme now"
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                enabled: Connection.mode !== "disconnected" && !syncTimer.running
                onClicked: { Client.fetchTheme(); syncTimer.start() }
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
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                Timer { id: syncTimer; interval: 1500; onTriggered: running = false }
            }
        }
    }

    component ConnectionCard: Rectangle {
        implicitHeight: col.implicitHeight + Theme.spacingLarge * 2
        color: Theme.surfaceColor
        radius: Theme.cardRadius

        ColumnLayout {
            id: col
            anchors.fill: parent
            anchors.margins: Theme.spacingLarge
            spacing: Theme.spacingSmall

            Text { text: "Connection"; color: Theme.textSecondaryColor; font.pixelSize: Theme.labelSize }
            ConnectionIndicator { mode: Connection.mode }
        }
    }

    component UnpairButton: Button {
        text: "Unpair Device"
        implicitHeight: 48
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
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
    }

    component VersionLabel: Text {
        text: "DecenzaPocket v" + AppVersion
        color: Theme.textSecondaryColor
        font.pixelSize: Theme.labelSize
    }

    readonly property bool landscape: width > height

    // Portrait: single column
    ColumnLayout {
        visible: !root.landscape
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: bottomBar.top
        anchors.topMargin: Theme.spacingMedium
        anchors.leftMargin: Theme.spacingMedium
        anchors.rightMargin: Theme.spacingMedium
        anchors.bottomMargin: Theme.spacingMedium
        spacing: Theme.spacingLarge

        DeviceCard       { Layout.fillWidth: true }
        NotificationCard { Layout.fillWidth: true }
        ThemeCard        { Layout.fillWidth: true }
        ConnectionCard   { Layout.fillWidth: true }

        Item { Layout.fillHeight: true }

        UnpairButton { Layout.fillWidth: true }
        VersionLabel { Layout.alignment: Qt.AlignHCenter }
    }

    // Landscape: two columns + footer
    ColumnLayout {
        visible: root.landscape
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: bottomBar.top
        anchors.topMargin: Theme.spacingMedium
        anchors.leftMargin: Theme.spacingMedium
        anchors.rightMargin: Theme.spacingMedium
        anchors.bottomMargin: Theme.spacingMedium
        spacing: Theme.spacingMedium

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingLarge

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                spacing: Theme.spacingMedium

                DeviceCard       { Layout.fillWidth: true }
                NotificationCard { Layout.fillWidth: true }
                ConnectionCard   { Layout.fillWidth: true }
                Item { Layout.fillHeight: true }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                spacing: Theme.spacingMedium

                ThemeCard { Layout.fillWidth: true }
                Item { Layout.fillHeight: true }
                UnpairButton { Layout.fillWidth: true }
            }
        }

        VersionLabel { Layout.alignment: Qt.AlignHCenter }
    }

    BottomBar {
        id: bottomBar
        title: "Settings"
        showBackButton: true
        onBackClicked: root.back()
    }
}
