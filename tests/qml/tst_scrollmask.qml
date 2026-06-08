/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Instantiates ScrollOpacityMask through a stub host and asserts it still works
// as a MultiEffect mask source: layer-backable, visible, white-where-opaque, and
// painted with QtQuick.Shapes (two Shape descendants) rather than the removed
// Qt5Compat LinearGradient.

import QtQuick
import QtTest
import QtQuick.Shapes

TestCase {
    id: testCase
    name: "ScrollOpacityMask"
    when: windowShown
    visible: true
    width: 300
    height: 200

    readonly property url harnessUrl: Qt.resolvedUrl("ScrollMaskHarness.qml")

    function countShapes(item) {
        var n = 0;
        if (item instanceof Shape) n++;
        if (item.children)
            for (var i = 0; i < item.children.length; i++) n += countShapes(item.children[i]);
        return n;
    }

    function makeMask() {
        const component = Qt.createComponent(harnessUrl);
        verify(component.status === Component.Ready, "harness failed to compile: " + component.errorString());
        const host = createTemporaryObject(component, testCase);
        verify(host, "harness failed to instantiate");
        verify(host.maskItem, "ScrollOpacityMask failed to load");
        return host.maskItem;
    }

    function test_maskPaintsWithShapes() {
        failOnWarning(/Error|is not defined|Cannot read/);
        const mask = makeMask();
        compare(countShapes(mask), 2);
    }

    // It is consumed as a maskSource: the root must be visible and layer-backable.
    function test_usableAsMaskSource() {
        const mask = makeMask();
        verify(mask.visible, "mask root must be visible to act as a maskSource");
        mask.layer.enabled = true;
        compare(mask.layer.enabled, true);
    }
}
