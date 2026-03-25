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

    function stateColor() {
        if (machineState === "Sleep" || machineState === "Disconnected" || machineState === "Unknown")
            return Theme.textSecondaryColor
        if (isHeating) return Theme.warningColor
        if (isReady) return Theme.successColor
        return Theme.primaryColor
    }

    function displayState() {
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
