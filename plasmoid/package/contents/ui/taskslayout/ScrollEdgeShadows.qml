/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.7
import QtQuick.Shapes

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
    id: shadowsContainer
    opacity: 0.4

    readonly property int gradientLength: appletAbilities.metrics.iconSize / 3
    readonly property int thickness: appletAbilities.metrics.backgroundThickness
    readonly property color appliedColor: appletAbilities.myView.itemShadow.shadowSolidColor

    property Item flickable

    Shape {
        id: firstGradient
        width: !root.vertical ? gradientLength : shadowsContainer.thickness
        height: !root.vertical ? shadowsContainer.thickness : gradientLength
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            //! Horizontal fill on a horizontal dock, vertical on a vertical one — the old end-point flip.
            fillGradient: LinearGradient {
                x1: 0; y1: 0
                x2: !root.vertical ? firstGradient.width : 0
                y2: !root.vertical ? 0 : firstGradient.height
                GradientStop { position: 0.0; color: (scrollableList.currentPos > scrollableList.scrollFirstPos ? appliedColor : "transparent") }
                GradientStop { position: 1.0; color: "transparent" }
            }
            startX: 0; startY: 0
            PathLine { x: firstGradient.width; y: 0 }
            PathLine { x: firstGradient.width; y: firstGradient.height }
            PathLine { x: 0; y: firstGradient.height }
        }
    }

    Shape {
        id: lastGradient
        width: firstGradient.width
        height: firstGradient.height
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: 0
                x2: !root.vertical ? lastGradient.width : 0
                y2: !root.vertical ? 0 : lastGradient.height
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: (scrollableList.currentPos < scrollableList.scrollLastPos ? appliedColor : "transparent") }
            }
            startX: 0; startY: 0
            PathLine { x: lastGradient.width; y: 0 }
            PathLine { x: lastGradient.width; y: lastGradient.height }
            PathLine { x: 0; y: lastGradient.height }
        }
    }

    states: [
        State {
            name: "bottom"
            when: root.location === PlasmaCore.Types.BottomEdge

            AnchorChanges {
                target: firstGradient
                anchors{ top:undefined; bottom:parent.bottom; left:parent.left; right:undefined;
                    horizontalCenter:undefined; verticalCenter:undefined}
            }
            AnchorChanges {
                target: lastGradient
                anchors{ top:undefined; bottom:parent.bottom; left:undefined; right:parent.right;
                    horizontalCenter:undefined; verticalCenter:undefined}
            }
            AnchorChanges {
                target: shadowsContainer
                anchors{ top:undefined; bottom:flickable.bottom; left:undefined; right:undefined;
                    horizontalCenter:flickable.horizontalCenter; verticalCenter:undefined}
            }
            PropertyChanges {
                target: shadowsContainer
                anchors.leftMargin: 0;    anchors.rightMargin:0;     anchors.topMargin: 0;    anchors.bottomMargin: appletAbilities.metrics.margin.screenEdge;
                anchors.horizontalCenterOffset: 0; anchors.verticalCenterOffset: 0;
            }
        },
        State {
            name: "top"
            when: root.location === PlasmaCore.Types.TopEdge

            AnchorChanges {
                target: firstGradient
                anchors{ top:parent.top; bottom:undefined; left:parent.left; right:undefined;
                    horizontalCenter:undefined; verticalCenter:undefined}
            }
            AnchorChanges {
                target: lastGradient
                anchors{ top:parent.top; bottom:undefined; left:undefined; right:parent.right;
                    horizontalCenter:undefined; verticalCenter:undefined}
            }
            AnchorChanges {
                target: shadowsContainer
                anchors{ top:flickable.top; bottom:undefined; left:undefined; right:undefined;
                    horizontalCenter:flickable.horizontalCenter; verticalCenter:undefined}
            }
            PropertyChanges {
                target: shadowsContainer
                anchors.leftMargin: 0;    anchors.rightMargin:0;     anchors.topMargin: appletAbilities.metrics.margin.screenEdge;    anchors.bottomMargin:0;
                anchors.horizontalCenterOffset: 0; anchors.verticalCenterOffset: 0;
            }
        },
        State {
            name: "left"
            when: root.location === PlasmaCore.Types.LeftEdge

            AnchorChanges {
                target: firstGradient
                anchors{ top:parent.top; bottom:undefined; left:parent.left; right:undefined;
                    horizontalCenter:undefined; verticalCenter:undefined}
            }
            AnchorChanges {
                target: lastGradient
                anchors{ top:undefined; bottom:parent.bottom; left:parent.left; right:undefined;
                    horizontalCenter:undefined; verticalCenter:undefined}
            }
            AnchorChanges {
                target: shadowsContainer
                anchors{ top:undefined; bottom:undefined; left:flickable.left; right:undefined;
                    horizontalCenter:undefined; verticalCenter:flickable.verticalCenter}
            }
            PropertyChanges {
                target: shadowsContainer
                anchors.leftMargin: appletAbilities.metrics.margin.screenEdge;    anchors.rightMargin:0;     anchors.topMargin: 0;    anchors.bottomMargin:0;
                anchors.horizontalCenterOffset: 0; anchors.verticalCenterOffset: 0;
            }
        },
        State {
            name: "right"
            when: root.location === PlasmaCore.Types.RightEdge

            AnchorChanges {
                target: firstGradient
                anchors{ top:parent.top; bottom:undefined; left:undefined; right:parent.right;
                    horizontalCenter:undefined; verticalCenter:undefined}
            }
            AnchorChanges {
                target: lastGradient
                anchors{ top:undefined; bottom:parent.bottom; left:undefined; right:parent.right;
                    horizontalCenter:undefined; verticalCenter:undefined}
            }
            AnchorChanges {
                target: shadowsContainer
                anchors{ top:undefined; bottom:undefined; left:undefined; right:flickable.right;
                    horizontalCenter:undefined; verticalCenter:flickable.verticalCenter}
            }
            PropertyChanges {
                target: shadowsContainer
                anchors.leftMargin: 0;    anchors.rightMargin: appletAbilities.metrics.margin.screenEdge;     anchors.topMargin: 0;    anchors.bottomMargin:0;
                anchors.horizontalCenterOffset: 0; anchors.verticalCenterOffset: 0;
            }
        }
    ]
}
