// MultiEffect blur — ToolTipInstance album-art frosted background (blurEnabled, blur 1.0,
// blurMax 32). The blur path is a separate multi-pass shader set from the colour effects.
import QtQuick
import QtQuick.Effects

Item {
    width: 256; height: 256
    // Multi-pass blur kernel has sub-pixel rounding that varies run-to-run on lavapipe
    // (observed max Δ=1 on a handful of edge pixels).
    property var probeTolerance: ({ "delta": 2, "budget": 0.005 })
    Rectangle {
        id: src
        anchors.fill: parent
        visible: false
        gradient: Gradient { GradientStop { position: 0; color: "purple" } GradientStop { position: 1; color: "gold" } }
        Text { anchors.centerIn: parent; text: "blur"; font.pixelSize: 48; color: "white" }
    }
    MultiEffect {
        anchors.fill: parent
        source: src
        blurEnabled: true
        blur: 1.0
        blurMax: 32
        autoPaddingEnabled: false
    }
}
