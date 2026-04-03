import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.surfaceColor
    radius: Theme.cardRadius

    property string machineState: "Disconnected"
    property string machinePhase: ""
    property double temperature: 0
    property double waterLevelMl: 0
    property bool isHeating: false
    property bool isReady: false
    property bool connected: false

    function stateColor() {
        var display = displayState()
        if (display === "Connecting" || display === "Sleep" || display === "Disconnected" || display === "Unknown")
            return Theme.textSecondaryColor
        if (display === "Heating")
            return Theme.warningColor
        if (display === "Ready" || display === "Idle")
            return Theme.successColor
        return Theme.primaryColor
    }

    function displayState() {
        if (!connected) return "Connecting"
        // Prefer phase (Heating, Ready, etc.) over raw BLE state
        if (machinePhase && machinePhase !== "Disconnected")
            return machinePhase
        return machineState || "Disconnected"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingMedium

        // State indicator
        RowLayout {
            spacing: Theme.spacingSmall
            Layout.alignment: Qt.AlignHCenter
            Rectangle {
                width: 12; height: 12; radius: 6
                color: root.stateColor()
            }
            Text {
                text: root.displayState()
                color: root.stateColor()
                font.pixelSize: Theme.titleSize
                font.bold: true
            }
        }

        // Temperature
        Text {
            text: root.temperature > 0
                  ? root.temperature.toFixed(1) + "\u00B0C"
                  : "--\u00B0C"
            color: Theme.textColor
            font.pixelSize: Theme.valueSize
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        // Water level
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacingSmall
            Text {
                text: "Water"
                color: Theme.textSecondaryColor
                font.pixelSize: Theme.bodySize
            }
            Text {
                text: root.waterLevelMl > 0
                      ? Math.round(root.waterLevelMl) + " ml"
                      : "-- ml"
                color: Theme.textColor
                font.pixelSize: Theme.bodySize
                font.bold: true
            }
        }
    }
}
