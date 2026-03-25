import QtQuick
import QtQuick.Layouts

RowLayout {
    property string mode: "disconnected" // "local", "remote", "disconnected"

    spacing: Theme.spacingSmall

    Rectangle {
        width: 8; height: 8; radius: 4
        color: {
            if (mode === "local") return Theme.successColor
            if (mode === "remote") return Theme.primaryColor
            return Theme.textSecondaryColor
        }
    }

    Text {
        text: {
            if (mode === "local") return "Connected over WiFi"
            if (mode === "remote") return "Connected over internet"
            return "Disconnected"
        }
        color: Theme.textSecondaryColor
        font.pixelSize: Theme.labelSize
    }
}
