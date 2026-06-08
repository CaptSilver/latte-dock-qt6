/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Instantiates ShadowedItem offscreen against a stub source and checks the
// MultiEffect wiring the call sites depend on: the source is bound, the shadow
// is on, and shadowBlur stays inside MultiEffect's 0..1 range for sane and
// out-of-range pixel sizes.

import QtQuick
import QtTest

TestCase {
    id: testCase
    name: "ShadowedItem"
    when: windowShown
    visible: true
    width: 200
    height: 200

    Item { id: stubSource; width: 48; height: 48 }

    readonly property url u: Qt.resolvedUrl("../../declarativeimports/components/ShadowedItem.qml")

    function make(sizePx) {
        const c = Qt.createComponent(u);
        verify(c.status === Component.Ready, c.errorString());
        const o = createTemporaryObject(c, testCase, { "source": stubSource, "shadowSizePx": sizePx });
        verify(o);
        return o;
    }

    function test_sourceBoundAndShadowOn() {
        const e = make(8);
        compare(e.source, stubSource);
        compare(e.shadowEnabled, true);
        verify(e.autoPaddingEnabled);
    }
    function test_blurInRangeForTypicalSize() {
        const e = make(8);
        verify(e.shadowBlur >= 0.0 && e.shadowBlur <= 1.0);
    }
    function test_blurClampsForHugeSize() {
        const e = make(4096);
        compare(e.shadowBlur, 1.0);
    }
    function test_blurZeroForZeroSize() {
        const e = make(0);
        compare(e.shadowBlur, 0.0);
    }
    // The default blurMaxPx (256) sits above the 0.5*512 icon-size shadow cap, so a
    // mid-range size scales proportionally instead of clamping early (a 64 ceiling
    // would wrongly report 1.0 at 128px).
    function test_blurScalesToDefaultCeiling() {
        compare(make(128).shadowBlur, 0.5);
        compare(make(256).shadowBlur, 1.0);
    }
}
