// Renders Latte's real org.kde.latte.components.ShadowedItem (a preconfigured MultiEffect
// drop shadow). Resolved from the freshly staged source modules that run.sh puts on the
// import path.
import QtQuick
import org.kde.latte.components 1.0 as LatteComponents

Item {
    width: 256; height: 256
    Rectangle { id: src; anchors.centerIn: parent; width: 110; height: 60; radius: 8; color: "white" }
    LatteComponents.ShadowedItem {
        anchors.fill: src
        source: src
        shadowColor: "black"
        shadowSizePx: 16
    }
}
