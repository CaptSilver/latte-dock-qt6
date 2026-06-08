/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The parabolic icon shadow binds transformOrigin/opacity/rotation/scale onto the
// shadow every animation frame. ShadowedItem (a MultiEffect) must accept those
// plain-Item props alongside the shadow source — this pins that contract, which
// the shadowBlurFor and ShadowedItem unit tests don't exercise.

import QtQuick
import QtTest

TestCase {
    id: testCase
    name: "ShadowedItemTransforms"
    when: windowShown
    visible: true
    width: 200
    height: 200

    Item {
        id: stubSource
        width: 48
        height: 48
        Rectangle { anchors.fill: parent; color: "red" }
    }

    readonly property url shadowedItemUrl: Qt.resolvedUrl("../../declarativeimports/components/ShadowedItem.qml")

    function test_acceptsSourceAndPerFrameTransforms() {
        const component = Qt.createComponent(shadowedItemUrl);
        verify(component.status === Component.Ready, component.errorString());
        // The same plain-Item props ParabolicItem binds every parabolic frame.
        const item = createTemporaryObject(component, testCase, {
            "source": stubSource,
            "shadowColor": "black",
            "shadowSizePx": 24,
            "shadowVerticalOffset": 2,
            "transformOrigin": Item.Bottom,
            "opacity": 0.8,
            "rotation": 0,
            "scale": 1.3
        });
        verify(item, "ShadowedItem failed to instantiate with a source");
        compare(item.source, stubSource);
        compare(item.scale, 1.3);
        compare(item.transformOrigin, Item.Bottom);
        fuzzyCompare(item.opacity, 0.8, 0.0001);
    }
}
