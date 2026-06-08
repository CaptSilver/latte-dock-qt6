/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.7
import QtQuick.Shapes

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore

Item {

    readonly property int gradientLength: appletAbilities.metrics.iconSize / 3

    Shape {
        id: firstGradient
        width: !root.vertical ? gradientLength : scrollableList.thickness
        height: !root.vertical ? scrollableList.thickness : gradientLength
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            //! White-where-opaque mask edge; direction flips with the dock orientation.
            fillGradient: LinearGradient {
                x1: 0; y1: 0
                x2: !root.vertical ? gradientLength : 0
                y2: !root.vertical ? 0 : gradientLength
                GradientStop { position: 0.0; color: scrollableList.currentPos <= scrollableList.scrollFirstPos ? "white" : "transparent" }
                GradientStop { position: 1.0; color: "white" }
            }
            startX: 0; startY: 0
            PathLine { x: firstGradient.width; y: 0 }
            PathLine { x: firstGradient.width; y: firstGradient.height }
            PathLine { x: 0; y: firstGradient.height }
        }
    }

    Rectangle {
        id: centralRectangle
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: !root.vertical ? length : firstGradient.width
        height: !root.vertical ? firstGradient.height : length
        color: "white"

        property int length: !root.vertical ? parent.width - 2*gradientLength : parent.height - 2*gradientLength
    }

    Shape {
        id: lastGradient
        anchors.top: parent.top
        anchors.right: parent.right
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
                GradientStop { position: 0.0; color: "white" }
                GradientStop { position: 1.0; color: (scrollableList.currentPos >= scrollableList.scrollLastPos ? "white" : "transparent") }
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
                target: centralRectangle
                anchors{ top:undefined; bottom:parent.bottom; left:undefined; right:undefined;
                    horizontalCenter:parent.horizontalCenter; verticalCenter:undefined}
            }
            AnchorChanges {
                target: lastGradient
                anchors{ top:undefined; bottom:parent.bottom; left:undefined; right:parent.right;
                    horizontalCenter:undefined; verticalCenter:undefined}
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
                target: centralRectangle
                anchors{ top:parent.top; bottom:undefined; left:undefined; right:undefined;
                    horizontalCenter:parent.horizontalCenter; verticalCenter:undefined}
            }
            AnchorChanges {
                target: lastGradient
                anchors{ top:parent.top; bottom:undefined; left:undefined; right:parent.right;
                    horizontalCenter:undefined; verticalCenter:undefined}
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
                target: centralRectangle
                anchors{ top:undefined; bottom:undefined; left:parent.left; right:undefined;
                    horizontalCenter:undefined; verticalCenter:parent.verticalCenter}
            }
            AnchorChanges {
                target: lastGradient
                anchors{ top:undefined; bottom:parent.bottom; left:parent.left; right:undefined;
                    horizontalCenter:undefined; verticalCenter:undefined}
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
                target: centralRectangle
                anchors{ top:undefined; bottom:undefined; left:undefined; right:parent.right;
                    horizontalCenter:undefined; verticalCenter:parent.verticalCenter}
            }
            AnchorChanges {
                target: lastGradient
                anchors{ top:undefined; bottom:parent.bottom; left:undefined; right:parent.right;
                    horizontalCenter:undefined; verticalCenter:undefined}
            }
        }
    ]
}
