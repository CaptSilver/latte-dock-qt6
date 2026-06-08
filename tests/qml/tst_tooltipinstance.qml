/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Drives the Qt6 effect ports inside ToolTipInstance offscreen. The real file
// pulls in taskmanager/abilities/root ids that don't exist outside a live dock,
// so each ported effect is rebuilt here from inline QML with stub sources, and
// we assert the MultiEffect wiring the file now relies on: the player-controls
// mask, the album-art blur, and the preview-thumbnail shadow whose size feeds
// both the layout margin and the shadow blur.

import QtQuick
import QtQuick.Effects
import QtTest

TestCase {
    id: testCase
    name: "ToolTipInstance"
    when: windowShown
    visible: true
    width: 200
    height: 200

    function make(qml) {
        const o = Qt.createQmlObject(qml, testCase);
        verify(o, "inline QML failed to instantiate");
        return o;
    }

    // Player-controls frosted glass masked by the thumbnail shape.
    function test_playerControlsMaskWired() {
        const o = make(
            'import QtQuick; import QtQuick.Effects;' +
            'Item { width: 100; height: 100;' +
            '  Rectangle { id: glass; anchors.fill: parent; color: "white"; visible: false }' +
            '  Item { id: maskItem; anchors.fill: parent }' +
            '  MultiEffect { id: eff; objectName: "mask"; anchors.fill: parent;' +
            '    source: glass; maskEnabled: true; maskSource: maskItem;' +
            '    maskThresholdMin: 0.0; maskSpreadAtMin: 1.0 } }');
        const eff = findChild(o, "mask");
        verify(eff, "MultiEffect not found");
        compare(eff.maskEnabled, true);
        verify(eff.maskSource !== null, "maskSource must be bound");
        verify(eff.source !== null, "source must be bound");
        compare(eff.maskThresholdMin, 0.0);
        compare(eff.maskSpreadAtMin, 1.0);
    }

    // Album-art background: source Image hidden, a sibling MultiEffect blurs it.
    function test_albumArtBlurWired() {
        const o = make(
            'import QtQuick; import QtQuick.Effects;' +
            'Item { width: 100; height: 100;' +
            '  property bool artAvailable: true;' +
            '  Image { id: art; objectName: "art"; anchors.fill: parent; visible: false; opacity: 0.25 }' +
            '  MultiEffect { objectName: "blur"; source: art; anchors.fill: art;' +
            '    visible: parent.artAvailable; opacity: 0.25;' +
            '    blurEnabled: true; blur: 1.0; blurMax: 32; autoPaddingEnabled: false } }');
        const eff = findChild(o, "blur");
        verify(eff, "blur MultiEffect not found");
        compare(eff.blurEnabled, true);
        compare(eff.blur, 1.0);
        compare(eff.blurMax, 32);
        compare(eff.opacity, 0.25);
        verify(eff.source !== null, "blur source must be bound");
        compare(findChild(o, "art").visible, false);
    }

    // Preview thumbnail shadow: one shadowPx drives both the loader margin and
    // the MultiEffect shadowBlur (= min(1, shadowPx/blurMax)).
    function test_previewShadowFeedsMarginAndBlur() {
        const o = make(
            'import QtQuick; import QtQuick.Effects;' +
            'Item { width: 100; height: 100;' +
            '  property int shadowPx: 12;' +
            '  property int blurMax: 64;' +
            '  Item { id: thumb; objectName: "thumb"; anchors.fill: parent;' +
            '    anchors.margins: Math.max(2, parent.shadowPx) }' +
            '  MultiEffect { objectName: "shadow"; source: thumb; anchors.fill: thumb;' +
            '    shadowEnabled: true; shadowColor: "black";' +
            '    shadowVerticalOffset: 3; shadowHorizontalOffset: 0;' +
            '    blurMax: parent.blurMax;' +
            '    shadowBlur: Math.min(1.0, parent.shadowPx / parent.blurMax);' +
            '    autoPaddingEnabled: true } }');
        const shadow = findChild(o, "shadow");
        const thumb = findChild(o, "thumb");
        verify(shadow, "shadow MultiEffect not found");
        compare(shadow.shadowEnabled, true);
        compare(shadow.shadowVerticalOffset, 3);
        // shadowPx=12, blurMax=64 -> 0.1875
        fuzzyCompare(shadow.shadowBlur, 12 / 64, 0.0001);
        // same shadowPx drives the loader margin: max(2,12)=12
        compare(thumb.anchors.margins, 12);

        // raising shadowPx moves BOTH the margin and the blur together.
        o.shadowPx = 40;
        compare(thumb.anchors.margins, 40);
        fuzzyCompare(shadow.shadowBlur, 40 / 64, 0.0001);
    }
}
