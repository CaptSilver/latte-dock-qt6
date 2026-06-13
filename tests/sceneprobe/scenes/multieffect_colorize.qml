// MultiEffect colorization — the textColor tint used by TaskIcon badges, ParabolicItem
// monochromizer, and the containment colorizer applet.
import QtQuick
import QtQuick.Effects

Item {
    width: 256; height: 256
    Rectangle {
        id: src
        anchors.fill: parent
        visible: false
        color: "darkorange"
        Text { anchors.centerIn: parent; text: "X"; font.pixelSize: 64; color: "white" }
    }
    MultiEffect {
        anchors.fill: parent
        source: src
        colorizationColor: "#3daee9"
        colorization: 0.8
    }
}
