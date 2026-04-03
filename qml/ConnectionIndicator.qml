import QtQuick
import QtQuick.Layouts

RowLayout {
    property string mode: "disconnected" // "local", "remote", "disconnected"

    spacing: Theme.spacingSmall

    Rectangle {
        width: 8; height: 8; radius: 4
        color: {
            if (mode === "remote") return Theme.successColor
            return Theme.textSecondaryColor
        }
    }

    Text {
        text: {
            if (mode === "remote") return "Connected"
            return "Connecting"
        }
        color: Theme.textSecondaryColor
        font.pixelSize: Theme.labelSize
    }
}
