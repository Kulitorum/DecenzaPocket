pragma Singleton
import QtQuick

QtObject {
    // Colors - from cached theme or Decenza defaults
    property color backgroundColor: _color("backgroundColor", "#1a1a2e")
    property color surfaceColor: _color("surfaceColor", "#303048")
    property color primaryColor: _color("primaryColor", "#4e85f4")
    property color secondaryColor: _color("secondaryColor", "#c0c5e3")
    property color textColor: _color("textColor", "#ffffff")
    property color textSecondaryColor: _color("textSecondaryColor", "#a0a8b8")
    property color accentColor: _color("accentColor", "#e94560")
    property color successColor: _color("successColor", "#00cc6d")
    property color warningColor: _color("warningColor", "#ffaa00")
    property color errorColor: _color("errorColor", "#ff4444")
    property color borderColor: _color("borderColor", "#3a3a4e")
    property color primaryContrastColor: _color("primaryContrastColor", "#ffffff")

    // Font sizes
    property int headingSize: _font("headingSize", 28)
    property int titleSize: _font("titleSize", 22)
    property int subtitleSize: _font("subtitleSize", 16)
    property int bodySize: _font("bodySize", 16)
    property int labelSize: _font("labelSize", 13)
    property int valueSize: _font("valueSize", 48)

    // Spacing
    property int spacingSmall: 8
    property int spacingMedium: 16
    property int spacingLarge: 24

    // Radius
    property int cardRadius: 16

    function _color(name, fallback) {
        var cached = Settings.themeColors[name]
        return cached ? cached : fallback
    }

    function _font(name, fallback) {
        var cached = Settings.themeFonts[name]
        return cached > 0 ? cached : fallback
    }
}
