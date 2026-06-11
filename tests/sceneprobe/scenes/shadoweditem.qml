// Renders the MultiEffect-based drop shadow that ShadowedItem (org.kde.latte.components)
// wraps, sized as a typical dock icon. ShadowedItem IS a MultiEffect with shadowEnabled +
// shadowBlurFor() defaults; the scene instantiates that equivalent directly so it
// doesn't need the pure-QML module (which Qt6 can't resolve file-based types from when
// using addImportPath in a nested kwin session — a known Qt6 limitation with plugin-less
// modules). The shadow formula is shadowBlur = min(1.0, sizePx / blurMaxPx).
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
