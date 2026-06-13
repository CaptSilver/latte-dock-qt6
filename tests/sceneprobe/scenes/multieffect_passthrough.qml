// MultiEffect with no effect enabled — the bare copy path (CompactApplet, TaskIcon
// clickedAnimation effect). Loads the base MultiEffect shader variant.
import QtQuick
import QtQuick.Effects

Item {
    width: 256; height: 256
    // Passthrough MultiEffect has minor sub-pixel variance on lavapipe (observed max Δ=4
    // on a handful of pixels around the text rendering).
    property var probeTolerance: ({ "delta": 8, "budget": 0.005 })
    Rectangle {
        id: src
        anchors.fill: parent
        visible: false
        color: "slategray"
        Text { anchors.centerIn: parent; text: "copy"; font.pixelSize: 48; color: "white" }
    }
    MultiEffect {
        anchors.fill: parent
        source: src
    }
}
