/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Interaction test for Latte's CheckBox control: instantiates it offscreen and
// synthesizes a real mouse click, plus exercises the Qt6 compatibility aliases
// (partiallyCheckedEnabled -> tristate, checkedState -> checkState) and the
// value -> checked/checkState mapping. This is the "drive the control without a
// live dock" layer; qml_load_compile.sh only compiles, this one clicks.

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
}
