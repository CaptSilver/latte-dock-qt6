// Degenerate MultiEffect parameters — shadowBlur animates from 0.0 to 0.8 across the
// 5 rendered frames. This exercises the edge where the blur shader transitions out of
// its zero/disabled state; any transient NaN or shader compilation failure on an
// intermediate frame trips the gate. The final frame is non-blank (shadowEnabled draws
// the spread even at shadowBlur=0), so the output floor also validates.
import QtQuick
import QtQuick.Effects

Item {
    width: 256; height: 256
    property var probeExpect: [ { "minOpaqueFraction": 0.05 } ]

    Rectangle {
        id: src
        anchors.centerIn: parent
        width: 120; height: 120
        radius: 16
        color: "seagreen"
        Text {
            anchors.centerIn: parent
            text: "Z"
            font.pixelSize: 48
            color: "white"
        }
    }

    MultiEffect {
        anchors.fill: src
        source: src
        autoPaddingEnabled: true
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, 0.8)
        blurMax: 32
        NumberAnimation on shadowBlur {
            from: 0.0; to: 0.8
            duration: 80
            running: true
        }
    }
}
