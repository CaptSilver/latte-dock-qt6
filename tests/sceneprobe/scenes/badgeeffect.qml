// Renders org.kde.graphicaleffects BadgeEffect — the masked-overlay ShaderEffect Latte
// uses for task badges (TaskIcon.qml) and Add-Widgets preview badges (AppletDelegate.qml).
// Unlike ShadowedItem/RectangularShadow (precompiled MultiEffect/Shapes), BadgeEffect is a
// real ShaderEffect that loads qrc:/shaders/badge.frag.qsb at runtime, so this is the path
// that surfaces a "Failed to deserialize QShader" / "shader preparation failed" on the
// Vulkan RHI. source + mask are ShaderEffectSources, exactly as the call sites wire them.
import QtQuick
import org.kde.graphicaleffects as KGraphicalEffects

Item {
    width: 256; height: 256

    Rectangle {
        id: iconWidget
        anchors.fill: parent
        color: "steelblue"
        Text { anchors.centerIn: parent; text: "app"; color: "white" }
    }

    Item {
        id: badgeMask
        anchors.fill: parent
        Rectangle {
            width: 40; height: 40; radius: 20
            x: parent.width - width; y: 0
            color: "white"
        }
    }

    KGraphicalEffects.BadgeEffect {
        anchors.fill: parent
        source: ShaderEffectSource {
            sourceItem: iconWidget
            hideSource: false
            live: false
        }
        mask: ShaderEffectSource {
            sourceItem: badgeMask
            hideSource: true
            live: false
        }
    }
}
