/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.2
import QtQuick.Controls.Styles 1.2 as QtQuickControlStyle
import QtQuick.Controls 1.2
import org.kde.ksvg 1.0 as KSvg

QtQuickControlStyle.SwitchStyle {
    id: styleRoot
    padding { top: 0 ; left: 0 ; right: 0 ; bottom: 0 }

    KSvg.Svg {
        id: switchSvg
        imagePath: "widgets/switch"
        colorSet: KSvg.Svg.View
    }

    property bool themeHasSwitch: false

    handle: Item {
        implicitWidth: {
            if (fallbackHandle.visible) {
                return height;
            }

            return switchSvg.hasElement("hint-handle-size") ? switchSvg.elementSize("hint-handle-size").width : themeHandleItem.width;
        }
        implicitHeight: {
            if (fallbackHandle.visible) {
                return theme.mSize(theme.defaultFont).height
            }

            return switchSvg.hasElement("hint-handle-size") ? switchSvg.elementSize("hint-handle-size").height : themeHandleItem.height
        }

        Item {
            id: fallbackHandle
            anchors.fill: parent
            visible: !themeHandleItem.visible

            KSvg.FrameSvgItem {
                anchors.fill: parent
                opacity: control.enabled ? 1.0 : 0.6
                imagePath: "widgets/button"
                prefix: "shadow"
            }

            KSvg.FrameSvgItem {
                id: button
                anchors.fill: parent
                imagePath: "widgets/button"
                prefix: "normal"
            }
        }

        KSvg.SvgItem {
            id: themeHandleShadow
            anchors.fill: themeHandleItem
            opacity: control.enabled ? 1.0 : 0.6
            svg: switchSvg
            elementId: control.activeFocus ? "handle-focus" : (control.hovered ? "handle-hover" : "handle-shadow")
            visible: styleRoot.themeHasSwitch
        }

        KSvg.SvgItem {
            id: themeHandleItem
            anchors.centerIn: parent
            width: naturalSize.width
            height: naturalSize.height
            svg: switchSvg
            elementId: "handle"
            visible: styleRoot.themeHasSwitch
        }
    }

    groove: Item {
        width: theme.mSize(theme.defaultFont).height * 2
        height: themeGroove.visible ? themeGrooveItem.implicitHeight : theme.mSize(theme.defaultFont).height

        Item{
            id: fallbackGroove
            anchors.fill: parent
            visible: !themeGroove.visible

            KSvg.FrameSvgItem {
                id: fallbackGrooveItem
                anchors.fill: parent
                imagePath: "widgets/slider"
                prefix: "groove"
                opacity: control.checked ? 0 : 1
                visible: opacity > 0

                Behavior on opacity {
                    PropertyAnimation { duration: units.shortDuration * 2 }
                }
            }

            KSvg.FrameSvgItem {
                id: fallbackHighlight
                anchors.fill: fallbackGrooveItem
                imagePath: "widgets/slider"
                prefix: "groove-highlight"
                opacity: control.checked ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    PropertyAnimation { duration: units.shortDuration * 2 }
                }
            }
        }

        Item {
            id: themeGroove
            anchors.fill: parent
            visible: styleRoot.themeHasSwitch

            KSvg.FrameSvgItem {
                id: themeGrooveItem
                anchors.fill: parent
                imagePath: "widgets/switch"
                prefix: "groove"
                opacity: control.checked ? 0 : 1
                visible: opacity > 0

                Component.onCompleted: styleRoot.themeHasSwitch = fromCurrentTheme;
                onFromCurrentThemeChanged: styleRoot.themeHasSwitch = fromCurrentTheme;

                Behavior on opacity {
                    PropertyAnimation { duration: units.shortDuration * 2 }
                }
            }

            KSvg.FrameSvgItem {
                id: themeGrooveHighlight
                anchors.fill: parent
                imagePath: "widgets/switch"
                prefix: "groove-highlight"
                opacity: control.checked ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    PropertyAnimation { duration: units.shortDuration * 2 }
                }
            }
        }
    }
}
