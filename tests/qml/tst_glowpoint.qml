/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Drives GlowPoint offscreen and asserts its glow is painted with QtQuick.Shapes
// rather than the removed Qt5Compat RadialGradient/LinearGradient. The two corner
// glows and the middle band each become one Shape, so a fully-painted glow has
// three Shape descendants; the pre-port Qt5Compat build has zero.

import QtQuick
import QtTest
import QtQuick.Shapes

TestCase {
    id: testCase
    name: "GlowPoint"
    when: windowShown
    visible: true
    width: 200
    height: 200

    readonly property url glowUrl: Qt.resolvedUrl("../../declarativeimports/components/GlowPoint.qml")

    function countShapes(item) {
        var n = 0;
        if (item instanceof Shape) n++;
        if (item.children)
            for (var i = 0; i < item.children.length; i++) n += countShapes(item.children[i]);
        return n;
    }

    function makeGlow(props) {
        const component = Qt.createComponent(glowUrl);
        verify(component.status === Component.Ready, "GlowPoint.qml failed to compile: " + component.errorString());
        const glow = createTemporaryObject(component, testCase, props);
        verify(glow, "GlowPoint.qml failed to instantiate");
        return glow;
    }

    // The three gradient layers must be QtQuick.Shapes, not Qt5Compat effects.
    function test_glowPaintsWithShapes() {
        const glow = makeGlow({showGlow: true, size: 12, width: 80, height: 30, location: 4});
        compare(countShapes(glow), 3);
    }

    // Vertical edges drive the diagonal LinearGradient branch; still three Shapes.
    function test_verticalStillPaintsWithShapes() {
        const glow = makeGlow({showGlow: true, size: 12, width: 30, height: 80, location: 6});
        verify(glow.isVertical);
        compare(countShapes(glow), 3);
    }

    // Public API the dock binds to must survive the port.
    function test_publicApiPreserved() {
        const glow = makeGlow({showGlow: false, size: 10});
        compare(glow.showGlow, false);
        glow.showGlow = true;
        compare(glow.showGlow, true);
    }

    // The attention pulse has to drive animationColor all the way to the caller's
    // attentionColor and back down to basicColor. Green is used for the attention
    // color on purpose: GlowPoint's animationColor default is a hard-coded "red",
    // so a pulse that never consults attentionColor cannot pass by accident.
    function test_attentionPulseReachesAttentionColor() {
        const glow = makeGlow({showAttention: true, showGlow: false,
                               basicColor: "#0000ff", attentionColor: "#00ff00",
                               animation: 60, width: 20, height: 20});
        var maxGreen = 0;
        var maxBlue = 0;
        // Extract the channel as a Number inside the handler. Pushing the color
        // value itself into an array would store a live handle on the property,
        // so every entry would read back as whatever it holds at assert time.
        glow.animationColorChanged.connect(function () {
            maxGreen = Math.max(maxGreen, glow.animationColor.g);
            maxBlue = Math.max(maxBlue, glow.animationColor.b);
        });
        wait(400);
        verify(maxGreen > 0.9, "attention pulse never reached attentionColor (peak green " + maxGreen + ")");
        verify(maxBlue > 0.9, "attention pulse never returned to basicColor (peak blue " + maxBlue + ")");
    }

    // animationColor is a plain property that outlives the Loader wrapping the
    // pulse, so the second attention episode starts from whatever mid-transition
    // color the first one was interrupted at. Its peak must still be attentionColor.
    function test_attentionPeakSurvivesRepeatEpisodes() {
        const glow = makeGlow({showAttention: true, showGlow: false,
                               basicColor: "#0000ff", attentionColor: "#00ff00",
                               animation: 60, width: 20, height: 20});
        wait(200);
        glow.showAttention = false;
        wait(120);
        glow.showAttention = true;
        var maxGreen = 0;
        glow.animationColorChanged.connect(function () {
            maxGreen = Math.max(maxGreen, glow.animationColor.g);
        });
        wait(400);
        verify(maxGreen > 0.9, "repeat attention episode peaked at a stale color (peak green " + maxGreen + ")");
    }
}
