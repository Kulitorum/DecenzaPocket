import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property bool showBackButton: false
    property string iconSource: ""

    signal clicked()
    signal backClicked()

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    height: 56
    color: Theme.primaryColor

    // Top border
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.borderColor
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 16
        spacing: Theme.spacingMedium

        // Back button
        Item {
            visible: root.showBackButton
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48

            Image {
                anchors.centerIn: parent
                source: "qrc:/icons/back.svg"
                width: 24; height: 24
                sourceSize: Qt.size(24, 24)
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.backClicked()
            }
        }

        // Title
        Text {
            visible: root.title !== ""
            text: root.title
            color: "#ffffff"
            font.pixelSize: 18
            font.bold: true
        }

        Item { Layout.fillWidth: true }

        // Right icon (e.g. settings cog)
        Item {
            visible: root.iconSource !== ""
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48

            Image {
                anchors.centerIn: parent
                source: root.iconSource
                width: 24; height: 24
                sourceSize: Qt.size(24, 24)
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.clicked()
            }
        }
    }
}
