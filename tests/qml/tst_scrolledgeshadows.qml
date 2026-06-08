/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Instantiates ScrollEdgeShadows through a stub host and asserts the two edge
// gradients paint with QtQuick.Shapes (two Shape descendants) and evaluate their
// bindings without referencing the removed Qt5Compat LinearGradient.

import QtQuick
import QtTest
import QtQuick.Shapes

TestCase {
    id: testCase
    name: "ScrollEdgeShadows"
    when: windowShown
    visible: true
    width: 300
    height: 200

    readonly property url harnessUrl: Qt.resolvedUrl("EdgeShadowsHarness.qml")

    function countShapes(item) {
        var n = 0;
        if (item instanceof Shape) n++;
        if (item.children)
            for (var i = 0; i < item.children.length; i++) n += countShapes(item.children[i]);
        return n;
    }

    function makeHost() {
        const component = Qt.createComponent(harnessUrl);
        verify(component.status === Component.Ready, "harness failed to compile: " + component.errorString());
        const host = createTemporaryObject(component, testCase);
        verify(host, "harness failed to instantiate");
        verify(host.edgeItem, "ScrollEdgeShadows failed to load");
        return host;
    }

    function test_edgeShadowsPaintWithShapes() {
        failOnWarning(/Error|is not defined|Cannot read/);
        const host = makeHost();
        compare(countShapes(host.edgeItem), 2);
    }
}
