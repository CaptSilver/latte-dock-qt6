/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Characterization test for insertIndexAt, the drop-index helper the tasks DropArea
// calls from onDragMove. That handler dereferences a DeclarativeDragDropEvent and is
// not reachable headlessly, so the only way to pin the arithmetic is to call the
// helper directly.
//
// tools.js is not a .pragma library, so it runs in the importing component's scope
// and reads root.vertical and appletAbilities.metrics.totals.length unqualified from
// there. Both are declared below, shaped like the plasmoid objects it really sees:
// main.qml declares vertical, and Totals.qml declares length as the full item
// length (icon plus edges), which is what the stripe arithmetic steps by.

import QtQuick
import QtTest
import "../../plasmoid/package/contents/code/tools.js" as TaskTools

TestCase {
    id: root
    name: "TaskTools"

    property bool vertical: false

    QtObject {
        id: appletAbilities

        readonly property QtObject metrics: QtObject {
            readonly property QtObject totals: QtObject {
                readonly property int length: 40
            }
        }
    }

    function init() {
        root.vertical = false;
    }

    // A drop over an existing task takes that task's slot, whatever the coordinates say.
    function test_hoveredItemWins() {
        compare(TaskTools.insertIndexAt({itemIndex: 3}, 999, 999), 3);
        compare(TaskTools.insertIndexAt({itemIndex: 7}, 0, 0), 7);
    }

    // Known quirk, pinned rather than endorsed: the hover branch tests itemIndex for
    // truthiness, so a drop over the FIRST task reads as no hover and falls through to
    // the stripe arithmetic. ceil(100 / 40) - 1 == 2, not 0.
    function test_hoverOverFirstTaskFallsThroughToStripes() {
        compare(TaskTools.insertIndexAt({itemIndex: 0}, 100, 100), 2);
    }

    // No hover: the distance along the layout decides, so a horizontal dock reads x
    // and ignores y.
    function test_horizontalUsesX() {
        compare(TaskTools.insertIndexAt(null, 100, 999), 2);
        compare(TaskTools.insertIndexAt(undefined, 100, 999), 2);
    }

    function test_verticalUsesY() {
        root.vertical = true;
        compare(TaskTools.insertIndexAt(null, 999, 100), 2);
    }

    // Exactly one item length in is the boundary between the first and second slot.
    function test_stripeBoundary() {
        compare(TaskTools.insertIndexAt(null, 40, 0), 0);
        compare(TaskTools.insertIndexAt(null, 41, 0), 1);
    }

    // A drop at the very edge lands before the first task.
    function test_dropAtOrigin() {
        compare(TaskTools.insertIndexAt(null, 0, 0), -1);
    }
}
