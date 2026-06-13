// Parabolic zoom — a scaled icon with a MultiEffect colorize overlay, mirroring the
// ParabolicItem monochromizer path: the source item is scaled up (simulating zoom)
// and the effect layer rides the same transform.
import QtQuick
import QtQuick.Effects

Item {
    width: 256; height: 256
    property var probeExpect: [ { "minOpaqueFraction": 0.05 } ]

    Rectangle {
        id: icon
        anchors.centerIn: parent
        width: 64; height: 64
        radius: 10
        color: "darkorange"
        scale: 1.6
        Text {
            anchors.centerIn: parent
            text: "A"
            font.pixelSize: 36
            color: "white"
        }
    }

    MultiEffect {
        anchors.fill: icon
        source: icon
        scale: icon.scale
        transformOrigin: Item.Center
        colorizationColor: "#3daee9"
        colorization: 0.7
    }
}
