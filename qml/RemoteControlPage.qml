import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "black"

    signal back()

    property bool connected: RemoteControl.active

    Image {
        id: remoteScreen
        anchors.centerIn: parent
        width: {
            if (RemoteControl.frameWidth === 0) return 0
            var scaleX = parent.width / RemoteControl.frameWidth
            var scaleY = parent.height / RemoteControl.frameHeight
            var s = Math.min(scaleX, scaleY)
            return RemoteControl.frameWidth * s
        }
        height: {
            if (RemoteControl.frameHeight === 0) return 0
            var scaleX = parent.width / RemoteControl.frameWidth
            var scaleY = parent.height / RemoteControl.frameHeight
            var s = Math.min(scaleX, scaleY)
            return RemoteControl.frameHeight * s
        }
        cache: false
        source: "image://remoteframe/frame"

        property int frameCounter: 0
        Connections {
            target: RemoteControl
            function onFrameUpdated() {
                remoteScreen.frameCounter++
                remoteScreen.source = "image://remoteframe/frame?" + remoteScreen.frameCounter
            }
        }

        MultiPointTouchArea {
            anchors.fill: parent
            mouseEnabled: true
            minimumTouchPoints: 1
            maximumTouchPoints: 5

            onPressed: function(touchPoints) {
                for (var i = 0; i < touchPoints.length; i++) {
                    var tp = touchPoints[i]
                    RemoteControl.sendTouch(0, tp.x / width, tp.y / height, tp.pointId)
                }
            }
            onUpdated: function(touchPoints) {
                for (var i = 0; i < touchPoints.length; i++) {
                    var tp = touchPoints[i]
                    RemoteControl.sendTouch(1, tp.x / width, tp.y / height, tp.pointId)
                }
            }
            onReleased: function(touchPoints) {
                for (var i = 0; i < touchPoints.length; i++) {
                    var tp = touchPoints[i]
                    RemoteControl.sendTouch(2, tp.x / width, tp.y / height, tp.pointId)
                }
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        visible: !root.connected
        spacing: Theme.spacingMedium

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: !root.connected
            palette.dark: Theme.primaryColor
        }
        Text {
            text: "Connecting to tablet..."
            color: Theme.textSecondaryColor
            font.pixelSize: Theme.bodySize
            Layout.alignment: Qt.AlignHCenter
        }
    }

    // Debug overlay
    Text {
        id: debugText
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        color: "lime"
        font.pixelSize: 12
        wrapMode: Text.Wrap
        property string fullText: "active: " + RemoteControl.active
              + " | frames: " + remoteScreen.frameCounter
              + " | size: " + RemoteControl.frameWidth + "x" + RemoteControl.frameHeight
              + "\n" + RemoteControl.debugInfo
        text: fullText

        MouseArea {
            anchors.fill: parent
            onClicked: {
                // Copy to clipboard
                clipHelper.text = debugText.fullText
                clipHelper.selectAll()
                clipHelper.copy()
                debugText.color = "yellow"
                clipTimer.start()
            }
        }
    }
    TextEdit { id: clipHelper; visible: false }
    Timer { id: clipTimer; interval: 500; onTriggered: debugText.color = "lime" }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Theme.spacingMedium
        width: 40; height: 40
        radius: 20
        color: Theme.surfaceColor
        opacity: 0.7

        Text {
            anchors.centerIn: parent
            text: "\u2190"
            color: Theme.textColor
            font.pixelSize: Theme.titleSize
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.back()
        }
    }
}
