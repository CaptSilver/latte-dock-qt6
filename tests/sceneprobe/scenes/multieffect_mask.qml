// MultiEffect mask — the layer.effect opacity mask the plasmoid uses to fade scrolled tasks
// (main.qml, ScrollOpacityMask) and the tooltip player-controls mask. maskSource is a
// layer-backed texture provider, exactly as the call sites wire it.
import QtQuick
import QtQuick.Effects

Item {
    width: 256; height: 256
    // Layer-backed mask edges aren't bit-reproducible run-to-run (AA varies ~Δ10 on a
    // handful of edge pixels), so this scene compares with a small tolerance instead of
    // the exact lavapipe tier.
    property var probeTolerance: ({ "delta": 16, "budget": 0.01 })
    Rectangle {
        id: src
        anchors.fill: parent
        visible: false
        color: "teal"
        Text { anchors.centerIn: parent; text: "mask"; font.pixelSize: 48; color: "white" }
    }
    Item {
        id: maskSrc
        anchors.fill: parent
        layer.enabled: true
        visible: false
        Rectangle { anchors.centerIn: parent; width: 160; height: 160; radius: 80; color: "white" }
    }
    MultiEffect {
        anchors.fill: parent
        source: src
        maskEnabled: true
        maskSource: maskSrc
        maskThresholdMin: 0.0
        maskSpreadAtMin: 1.0
        autoPaddingEnabled: false
    }
}
