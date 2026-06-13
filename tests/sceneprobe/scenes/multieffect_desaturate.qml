// MultiEffect saturation:-1 — the grayscale path (TaskIcon stateColorizer / badge desaturate,
// RemoveWindowFromGroupAnimation). Loads the saturation shader variant.
import QtQuick
import QtQuick.Effects

Item {
    width: 256; height: 256
    Rectangle {
        id: src
        anchors.fill: parent
        visible: false
        gradient: Gradient { GradientStop { position: 0; color: "tomato" } GradientStop { position: 1; color: "royalblue" } }
        Text { anchors.centerIn: parent; text: "X"; font.pixelSize: 64; color: "white" }
    }
    MultiEffect {
        anchors.fill: parent
        source: src
        saturation: -1
    }
}
