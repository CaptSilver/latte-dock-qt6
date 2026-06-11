// Exercises the shadow render path used by Latte's ShadowedItem. ShadowedItem is itself a thin
// MultiEffect wrapper, so this inlines MultiEffect with the same props to avoid depending on a
// freshly-installed org.kde.latte.components module. Importing the real component works once that
// module is installed up to date; staging the current source modules onto LATTE_QML_IMPORT_PATH
// (ahead of the system copy) is the way to test Latte's own component here later.
// The shadow formula is shadowBlur = min(1.0, sizePx / blurMaxPx).
import QtQuick
import QtQuick.Effects

Item {
    width: 256; height: 256

    Rectangle {
        id: iconSource
        anchors.centerIn: parent
        width: 48; height: 48
        radius: 8
        color: "#5294e2"
    }

    // Equivalent to: LatteComponents.ShadowedItem { source: iconSource; shadowSizePx: 12 }
    MultiEffect {
        source: iconSource
        anchors.centerIn: parent
        autoPaddingEnabled: true
        shadowEnabled: true
        shadowColor: "#80000000"
        shadowOpacity: 1.0
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 2
        blurMax: 256
        shadowBlur: Math.min(1.0, 12 / 256)   // shadowBlurFor(sizePx=12, blurMaxPx=256)
    }
}
