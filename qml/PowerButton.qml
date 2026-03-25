import QtQuick

Rectangle {
    id: root
    radius: Theme.cardRadius
    color: root.isAwake ? Theme.surfaceColor : Theme.primaryColor
    border.width: root.isAwake ? 1 : 0
    border.color: Theme.borderColor

    property bool isAwake: false
    property bool enabled: true
    signal toggled()

    // Press feedback
    scale: mouseArea.pressed ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: 100 } }

    Text {
        anchors.centerIn: parent
        text: root.isAwake ? "Turn Off" : "Turn On"
        color: root.isAwake ? Theme.textColor : Theme.primaryContrastColor
        font.pixelSize: Theme.titleSize
        font.bold: true
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.enabled
        onClicked: root.toggled()
    }
}
