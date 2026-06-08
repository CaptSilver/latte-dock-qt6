/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Hosts ScrollOpacityMask with the outer ids it reads (root, scrollableList,
// appletAbilities) so the mask can be instantiated and used as a maskSource
// offscreen, without a live dock.

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
        readonly property int thickness: 30
        readonly property real currentPos: 0
        readonly property real scrollFirstPos: 0
        readonly property real scrollLastPos: 100
    }

    QtObject {
        id: appletAbilities
        readonly property QtObject metrics: QtObject { readonly property int iconSize: 48 }
    }

    property alias maskItem: maskLoader.item

    Loader {
        id: maskLoader
        anchors.fill: parent
        source: Qt.resolvedUrl("../../plasmoid/package/contents/ui/taskslayout/ScrollOpacityMask.qml")
    }
}
