/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Interaction test for Latte's CheckBox control: instantiates it offscreen and
// synthesizes a real mouse click, plus exercises the Qt6 compatibility aliases
// (partiallyCheckedEnabled -> tristate, checkedState -> checkState), the
// value -> checked/checkState mapping and the bindTarget/bindProperty pair the
// settings pages use to name a config key once. This is the "drive the control
// without a live dock" layer; qml_load_compile.sh only compiles, this one clicks.

import QtQuick
import QtTest

TestCase {
    id: testCase
    name: "LatteCheckBox"
    when: windowShown
    visible: true
    width: 200
    height: 200

    // Resolved relative to this file so the test is location-independent.
    readonly property url checkBoxUrl: Qt.resolvedUrl("../../declarativeimports/components/CheckBox.qml")

    function makeCheckBox() {
        const component = Qt.createComponent(checkBoxUrl);
        verify(component.status === Component.Ready, "CheckBox.qml failed to compile: " + component.errorString());
        const box = createTemporaryObject(component, testCase);
        verify(box, "CheckBox.qml failed to instantiate");
        return box;
    }

    function test_valueMapsToChecked() {
        const box = makeCheckBox();
        compare(box.checked, false);
        box.value = 1;
        compare(box.checked, true);
    }

    function test_tristateValueMapsToCheckState() {
        const box = makeCheckBox();
        box.partiallyCheckedEnabled = true; // alias -> tristate
        box.value = Qt.PartiallyChecked;
        compare(box.checkedState, Qt.PartiallyChecked); // alias -> checkState
    }

    function test_clickToggles() {
        const box = makeCheckBox();
        const before = box.checked;
        mouseClick(box);
        compare(box.checked, !before);
    }

    // Stands in for the config maps the settings pages hand the control:
    // plasmoid.configuration, indicator.configuration, latteView.visibility.
    function makeConfig() {
        return Qt.createQmlObject('import QtQuick; QtObject { property bool first: false; property bool second: false }',
                                  testCase, "checkBoxConfigMock");
    }

    function test_boundKeyDrivesChecked() {
        const cfg = makeConfig();
        const box = makeCheckBox();
        verify(typeof box.bindTarget !== "undefined", "CheckBox.qml declares no bindTarget");
        verify(typeof box.bindProperty !== "undefined", "CheckBox.qml declares no bindProperty");

        box.bindTarget = cfg;
        box.bindProperty = "first";
        compare(box.checked, false);

        cfg.first = true;
        compare(box.checked, true, "a write to the bound key must re-drive the control");

        box.bindProperty = "second";
        compare(box.checked, false, "naming another key must re-read from that key");

        cfg.second = true;
        compare(box.checked, true, "after a key change the control follows the new key");
    }

    function test_boundClickWritesTheKey() {
        const cfg = makeConfig();
        const box = makeCheckBox();
        verify(typeof box.bindTarget !== "undefined", "CheckBox.qml declares no bindTarget");
        box.bindTarget = cfg;
        box.bindProperty = "first";

        mouseClick(box);
        compare(cfg.first, true, "clicking a bound checkbox must write its key");
        compare(box.checked, true);

        mouseClick(box);
        compare(cfg.first, false, "a second click must write back");
        compare(box.checked, false);
    }

    // A handler declared here is not replaced by one at the use site -- both fire.
    // Sites that compute their state keep their own onClicked, so the built-in write
    // has to stay inert whenever no key is named; otherwise the two inversions cancel
    // and the setting silently stops saving while the box still ticks on screen.
    function test_unboundClickWritesNothing() {
        const cfg = makeConfig();
        const box = makeCheckBox();
        compare(box.bindProperty, "", "an unbound checkbox names no key");

        box.clicked();
        mouseClick(box);
        compare(cfg.first, false, "an unbound checkbox must not write anything");
        compare(cfg.second, false);
        compare(box.checked, true, "an unbound checkbox still toggles itself");
    }
}
