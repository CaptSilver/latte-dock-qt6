// Kirigami.ShadowedRectangle — Latte's custom dock-background panel (colorizer/
// KirigamiShadowedRectangle.qml, used whenever the Kirigami library is found). It is a
// QSGMaterial-backed primitive that loads its rounded-rect + soft-shadow shaders at runtime,
// so it exercises a shader path separate from MultiEffect/BadgeEffect and is drawn in nearly
// every live dock. Mirrors the real call site: transparent fill, coloured soft shadow.
import QtQuick
import org.kde.kirigami as Kirigami

Item {
    width: 256; height: 256
    Kirigami.ShadowedRectangle {
        anchors.centerIn: parent
        width: 180; height: 120
        radius: 16
        color: "transparent"
        border.width: 2
        border.color: "#3daee9"
        shadow.size: 24
        shadow.color: Qt.rgba(0, 0, 0, 0.6)
    }
}
