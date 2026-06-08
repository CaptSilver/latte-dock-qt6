/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Hosts ScrollEdgeShadows with the outer ids it reads (root, scrollableList,
// appletAbilities, flickable) so the gradient component can be instantiated and
// inspected offscreen, without a live dock.

import QtQuick
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
    id: root
    width: 200
    height: 60

    readonly property bool vertical: false
    readonly property int location: PlasmaCore.Types.BottomEdge

    QtObject {
        id: scrollableList
        readonly property real currentPos: 50
        readonly property real scrollFirstPos: 0
        readonly property real scrollLastPos: 100
    }

    QtObject {
        id: appletAbilities
        readonly property QtObject metrics: QtObject {
            readonly property int iconSize: 48
            readonly property int backgroundThickness: 30
            readonly property QtObject margin: QtObject { readonly property int screenEdge: 4 }
        }
        readonly property QtObject myView: QtObject {
            readonly property QtObject itemShadow: QtObject { readonly property color shadowSolidColor: "black" }
        }
    }

    property alias edgeItem: edgeLoader.item

    Item { id: flickable; anchors.fill: parent }

    Loader {
        id: edgeLoader
        anchors.fill: parent
        source: Qt.resolvedUrl("../../plasmoid/package/contents/ui/taskslayout/ScrollEdgeShadows.qml")
        onLoaded: item.flickable = flickable
    }
}
