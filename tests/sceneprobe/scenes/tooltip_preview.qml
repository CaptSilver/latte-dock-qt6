// Tooltip preview composition: blurred album-art background (ToolTipInstance frosted
// glass) + an opaque card over it + a ShadowedItem drop shadow behind the card.
// ShadowedItem IS a MultiEffect (its root type), so it takes source + shadowSizePx,
// placed as a sibling behind the card so the card itself stays visible.
import QtQuick
import QtQuick.Effects
import org.kde.latte.components 1.0 as LatteComponents

Item {
    width: 256; height: 256
    property var probeExpect: [ { "minOpaqueFraction": 0.05 } ]

    // Frosted album-art background — same blur settings as ToolTipInstance.
    Rectangle {
        id: art
        anchors.fill: parent
        visible: false
        gradient: Gradient {
            GradientStop { position: 0; color: "purple" }
            GradientStop { position: 1; color: "gold" }
        }
    }
    MultiEffect {
        anchors.fill: parent
        source: art
        blurEnabled: true
        blur: 1.0
        blurMax: 32
        autoPaddingEnabled: false
    }

    // Shadow behind the card.
    LatteComponents.ShadowedItem {
        id: cardShadow
        anchors.centerIn: parent
        width: 160; height: 90
        source: card
        shadowSizePx: 12
        shadowColor: Qt.rgba(0, 0, 0, 0.7)
    }

    // Visible tooltip card.
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 160; height: 90
        radius: 8
        color: "#dd1b2a3c"
    }
}
