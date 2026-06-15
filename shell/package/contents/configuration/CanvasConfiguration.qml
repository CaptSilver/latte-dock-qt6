/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.8
import QtQuick.Controls 2.15 as QQC2

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.kirigami 2.20 as Kirigami

import org.kde.latte.private.app 0.1 as LatteApp
import org.kde.latte.core 0.2 as LatteCore
import org.kde.latte.private.containment 0.1 as LatteContainment

import "canvas" as CanvasComponent

Loader {
    id: canvasRoot
    active: plasmoid && plasmoid.configuration && latteView

    //! Exposed to CanvasConfigView (C++): the rearrange/exit toggle's rect in canvas-local coords. In
    //! configure-applets mode the canvas is carved click-through EXCEPT this rect, so the toggle stays
    //! clickable (to leave rearrange) while every widget interaction falls through to the dock.
    property rect rearrangeToggleRect: item ? item.rearrangeToggleRect : Qt.rect(-1, -1, 0, 0)

    sourceComponent: Item{
        id: root
        readonly property bool isVertical: plasmoid.formFactor === PlasmaCore.Types.Vertical
        readonly property bool isHorizontal: !isVertical
        readonly property rect rearrangeToggleRect: {
            var b = settingsOverlay.rearrangeToggle;
            if (!b || !b.visible) {
                return Qt.rect(-1, -1, 0, 0);
            }
            //! reference the reactive geometry so the binding re-evaluates when the toggle moves/resizes
            var reactive = b.x + b.y + b.width + b.height + root.width + root.height;
            return b.mapToItem(root, 0, 0, b.width, b.height);
        }

        property int animationSpeed: LatteCore.WindowSystem.compositingActive ? 500 : 0
        property int panelAlignment: plasmoid.configuration.alignment

        readonly property real appliedOpacity: imageTiler.opacity
        readonly property real maxOpacity: {
            //! Rearranging widgets: the canvas overlays the dock and its input region is carved away
            //! over the widgets (CanvasConfigView::updateInputRegion), so the blueprint must be
            //! transparent too -- otherwise it paints over the very widgets you are trying to see and move.
            if (universalSettings.inConfigureAppletsMode) {
                return 0;
            }

            //! No compositor -> no real transparency, so show the blueprint solid.
            if (!LatteCore.WindowSystem.compositingActive) {
                return 1;
            }

            //! Track the dock's real background opacity (what the edit-mode wheel now changes) so the
            //! blueprint preview matches what you scroll. -1 means the theme default (treat as opaque).
            return plasmoid.configuration.panelTransparency === -1 ? 1 : plasmoid.configuration.panelTransparency / 100;
        }

        property real offset: {
            if (root.isHorizontal) {
                return width * (plasmoid.configuration.offset/100);
            } else {
                return height * (plasmoid.configuration.offset/100)
            }
        }

        property string appChosenShadowColor: {
            if (plasmoid.configuration.shadowColorType === LatteContainment.Types.ThemeColorShadow) {
                var strC = String(Kirigami.Theme.textColor);
                return strC.indexOf("#") === 0 ? strC.substr(1) : strC;
            } else if (plasmoid.configuration.shadowColorType === LatteContainment.Types.UserColorShadow) {
                return plasmoid.configuration.shadowColor;
            }

            // default shadow color
            return "080808";
        }

        property string appShadowColorSolid: "#" + appChosenShadowColor

        //// BEGIN OF Behaviors
        Behavior on offset {
            NumberAnimation {
                id: offsetAnimation
                duration: animationSpeed
                easing.type: Easing.OutCubic
            }
        }
        //// END OF Behaviors

        Item {
            id: graphicsSystem
            readonly property bool isAccelerated: (GraphicsInfo.api !== GraphicsInfo.Software)
                                                  && (GraphicsInfo.api !== GraphicsInfo.Unknown)
        }

        Image{
            id: imageTiler
            anchors.fill: parent
            opacity: root.maxOpacity
            fillMode: Image.Tile
            source: latteView.layout ? latteView.layout.background : "../images/canvas/blueprint.jpg"

            Behavior on opacity {
                NumberAnimation {
                    duration: 0.8 * root.animationSpeed
                    easing.type: Easing.OutCubic
                }
            }
        }

        LatteApp.ContextMenuLayer {
            id: contextMenuLayer
            anchors.fill: parent
            view: latteView
        }

        MouseArea {
            id: editBackMouseArea
            anchors.fill: imageTiler
            visible: !universalSettings.inConfigureAppletsMode
            hoverEnabled: true

            property bool wheelIsBlocked: false;
            readonly property int opacityStep: 5
            readonly property string tooltip: i18nc("opacity for the dock background under edit mode, %1 is opacity percentage",
                                                    "You can use mouse wheel to change the dock background opacity of %1%", plasmoid.configuration.panelTransparency === -1 ? 100 : plasmoid.configuration.panelTransparency)

            onWheel: (wheel) => {
                processWheel(wheel);
            }


            function processWheel(wheel) {
                if (wheelIsBlocked) {
                    return;
                }

                wheelIsBlocked = true;
                scrollDelayer.start();

                var angle = wheel.angleDelta.y / 8;

                //! -1 = theme default (treated as fully opaque); start from 100 so the first scroll steps down.
                var current = plasmoid.configuration.panelTransparency === -1 ? 100 : plasmoid.configuration.panelTransparency;

                if (angle > 10) {
                    plasmoid.configuration.panelTransparency = Math.min(100, current + opacityStep);
                } else if (angle < -10) {
                    plasmoid.configuration.panelTransparency = Math.max(0, current - opacityStep);
                }
            }

            //! A timer is needed in order to handle also touchpads that probably
            //! send too many signals very fast. This way the signals per sec are limited.
            //! The user needs to have a steady normal scroll in order to not
            //! notice a annoying delay
            Timer{
                id: scrollDelayer

                interval: 80
                onTriggered: editBackMouseArea.wheelIsBlocked = false;
            }
        }

        PlasmaComponents.Button {
            anchors.fill: editBackMouseArea
            opacity: 0

            QQC2.ToolTip.text: editBackMouseArea.tooltip
            QQC2.ToolTip.visible: hovered && editBackMouseArea.tooltip.length > 0
        }

        //! Settings Overlay
        CanvasComponent.SettingsOverlay {
            id: settingsOverlay
            anchors.fill: parent
        }
    }
}
