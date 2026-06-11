// Renders Latte's real org.kde.latte.components.GlowPoint.
import QtQuick
import org.kde.latte.components 1.0 as LatteComponents

Item {
    width: 256; height: 256
    LatteComponents.GlowPoint {
        anchors.centerIn: parent
        width: 48; height: 48
        size: 24
        basicColor: "cyan"
        showGlow: true
    }
}
