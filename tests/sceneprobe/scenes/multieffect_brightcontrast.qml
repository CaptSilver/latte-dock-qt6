// MultiEffect brightness+contrast — TaskIcon hover highlight (brightness 0.30, contrast 0.1).
import QtQuick
import QtQuick.Effects

Item {
    width: 256; height: 256
    Rectangle {
        id: src
        anchors.fill: parent
        visible: false
        color: "seagreen"
        Text { anchors.centerIn: parent; text: "X"; font.pixelSize: 64; color: "white" }
    }
    MultiEffect {
        anchors.fill: parent
        source: src
        brightness: 0.30
        contrast: 0.1
    }
}
