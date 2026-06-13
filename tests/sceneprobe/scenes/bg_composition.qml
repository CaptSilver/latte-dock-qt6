// Background composition — the stacked render path the dock colorizer walks:
// Kirigami.ShadowedRectangle panel background with a border + a second inset card,
// then a MultiEffect colorization pass over both (the tint the colorizer applet applies).
// Avoids KSvg.FrameSvgItem (needs a Plasma theme SVG); exercises the QSGMaterial shader
// path plus the MultiEffect colourizer in one capture.
import QtQuick
import QtQuick.Effects
import org.kde.kirigami as Kirigami

Item {
    width: 256; height: 256
    property var probeExpect: [ { "minOpaqueFraction": 0.10 } ]

    Item {
        id: stack
        anchors.fill: parent

        Kirigami.ShadowedRectangle {
            anchors.centerIn: parent
            width: 200; height: 120
            radius: 16
            color: "#202a3a"
            border.width: 2
            border.color: "#3daee9"
            shadow.size: 20
            shadow.color: Qt.rgba(0, 0, 0, 0.6)
        }

        Kirigami.ShadowedRectangle {
            anchors.centerIn: parent
            width: 140; height: 70
            radius: 8
            color: "#2e3a50"
            shadow.size: 8
            shadow.color: Qt.rgba(0, 0, 0, 0.4)
        }
    }

    MultiEffect {
        anchors.fill: stack
        source: stack
        colorizationColor: "#3daee9"
        colorization: 0.35
    }
}
