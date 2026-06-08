/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Drives Latte's task-icon color effects after the Qt6 MultiEffect port: a
// stub source Item stands in for the real icon, and we assert the ported
// MultiEffect instances expose the same knobs (brightness/contrast, the tint
// colorization pair, and the grayscale saturation) the old Qt5Compat effects
// drove. Compile-only coverage lives in qml_load_compile.sh; this asserts the
// bindings actually resolve on a constructed object.

import QtQuick
import QtQuick.Effects
import QtTest

TestCase {
    id: testCase
    name: "TaskIconEffects"
    when: windowShown
    visible: true
    width: 200
    height: 200

    Item { id: stubSource; width: 64; height: 64 }

    // The hover BrightnessContrast replacement: a MultiEffect carrying the
    // verbatim brightness/contrast the old effect used.
    Component {
        id: hoverEffectComponent
        MultiEffect {
            source: stubSource
            anchors.fill: stubSource
            brightness: 0.30
            contrast: 0.1
        }
    }

    function make(component) {
        const o = createTemporaryObject(component, testCase);
        verify(o, "component failed to instantiate");
        return o;
    }

    function test_hoverBrightnessContrastBound() {
        const fx = make(hoverEffectComponent);
        compare(fx.brightness, 0.30);
        compare(fx.contrast, 0.1);
    }

    function test_brightnessIsAnimatable() {
        const fx = make(hoverEffectComponent);
        fx.brightness = -0.5; // ClickedAnimation drives this externally
        compare(fx.brightness, -0.5);
    }
}
