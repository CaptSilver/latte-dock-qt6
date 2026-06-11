// Renders QtQuick.Effects.RectangularShadow, the Qt6 replacement for the rounded-rect
// shadow under the ComboBox popup card and ExternalShadow edge strip in Latte.
import QtQuick
import QtQuick.Effects

Item {
    width: 256; height: 256

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 160; height: 80
        radius: 6
        color: "#f5f5f5"

        RectangularShadow {
            anchors.fill: parent
            z: -1
            blur: 8
            spread: 0
            radius: parent.radius
            color: Qt.rgba(0, 0, 0, 0.35)
            offset: Qt.point(0, 3)
        }
    }
}
