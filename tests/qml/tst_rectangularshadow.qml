/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Verifies the rounded-rect shadow ports off Qt5Compat DropShadow onto Qt6's
// QtQuick.Effects.RectangularShadow: the ComboBox popup card (WS3.1) and the
// reusable ExternalShadow edge component (WS3.2) must both still compile, and a
// bare RectangularShadow must accept the contract properties we drive it with.

import QtQuick
import QtQuick.Effects
import QtTest

TestCase {
    id: testCase
    name: "RectangularShadow"
    when: windowShown
    visible: true
    width: 200
    height: 200

    readonly property url comboBoxUrl: Qt.resolvedUrl("../../declarativeimports/components/ComboBox.qml")
    readonly property url externalShadowUrl: Qt.resolvedUrl("../../declarativeimports/components/ExternalShadow.qml")

    function compileReady(url) {
        const component = Qt.createComponent(url);
        verify(component.status === Component.Ready, url + " failed to compile: " + component.errorString());
        return component;
    }

    function test_comboBoxCompiles() {
        compileReady(comboBoxUrl);
    }

    function test_externalShadowCompiles() {
        const component = compileReady(externalShadowUrl);
        const obj = createTemporaryObject(component, testCase, { shadowSize: 7, shadowColor: "#040404" });
        verify(obj, "ExternalShadow.qml failed to instantiate");
    }

    Component {
        id: rectShadowComponent
        RectangularShadow {
            color: Qt.rgba(0, 0, 0, 0.3)
            blur: 4
            spread: 0
            radius: 2
            offset: Qt.point(2, 2)
        }
    }

    function test_rectangularShadowAcceptsContractProps() {
        const shadow = createTemporaryObject(rectShadowComponent, testCase);
        verify(shadow, "RectangularShadow failed to instantiate");
        compare(shadow.blur, 4);
        compare(shadow.radius, 2);
    }
}
