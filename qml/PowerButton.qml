import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    radius: Theme.cardRadius
    color: !root.connected ? Theme.surfaceColor
         : root.isAwake ? Theme.surfaceColor
         : Theme.primaryColor
    border.width: root.isAwake || !root.connected ? 1 : 0
    border.color: Theme.borderColor
    opacity: root.connected ? 1.0 : 0.6

    property bool isAwake: false
    property bool connected: false
    signal toggled()

    // Press feedback
    scale: mouseArea.pressed && root.connected ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: 100 } }

    Row {
        anchors.centerIn: parent
        spacing: Theme.spacingSmall

        BusyIndicator {
            visible: !root.connected
            running: visible
            width: 24; height: 24
            anchors.verticalCenter: parent.verticalCenter
            palette.dark: Theme.textSecondaryColor
        }

        Text {
            text: !root.connected ? "Connecting..."
                : root.isAwake ? "Turn Off"
                : "Turn On"
            color: !root.connected ? Theme.textSecondaryColor
                 : root.isAwake ? Theme.textColor
                 : Theme.primaryContrastColor
            font.pixelSize: Theme.titleSize
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.connected
        onClicked: root.toggled()
    }
}
