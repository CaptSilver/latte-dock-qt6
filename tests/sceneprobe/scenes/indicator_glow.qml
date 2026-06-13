// Indicator active-glow: GlowPoint with the glow enabled, mirroring the default
// indicator's active-task dot. Uses the same properties as the existing glowpoint.qml
// scene (size, basicColor, showGlow) plus glow3D which the default indicator sets.
import QtQuick
import org.kde.latte.components 1.0 as LatteComponents

Item {
    width: 256; height: 256
    property var probeExpect: [ { "minOpaqueFraction": 0.02 } ]

    LatteComponents.GlowPoint {
        anchors.centerIn: parent
        width: 160; height: 24
        size: 16
        basicColor: "#3daee9"
        showGlow: true
        glow3D: true
    }
}
