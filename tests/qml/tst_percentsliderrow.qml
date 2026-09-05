/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Interaction test for the shared percent-slider row: a caption, a slider and a
// "NN %" readout, with the config write deferred to release. The indicator
// configuration pages each carried their own byte-identical copy of this.
//
// The domain case is the one that matters. Indicator config UIs are loaded by an
// engine whose translation domain is latte_indicator_<pluginId>, but this file is
// extracted into latte-dock.pot, so a bare i18nc() would look the readout up in a
// catalog that no longer carries it and silently fall back to English.

import QtQuick
import QtQuick.Layouts
import QtTest
import org.kde.kirigami 2.20 as Kirigami

TestCase {
    id: testCase
    name: "PercentSliderRow"
    when: windowShown
    visible: true
    width: 400
    height: 120

    // Resolved relative to this file so the test is location-independent, and so it
    // reads the working tree rather than any installed copy.
    readonly property url targetUrl: Qt.resolvedUrl("../../declarativeimports/components/PercentSliderRow.qml")

    // The component sets Layout.fillWidth on itself and on its slider; those attached
    // bindings only resolve inside a real Layout, so create into this.
    ColumnLayout {
        id: host
        anchors.fill: parent
    }

    // Same metrics the component uses to size its readout.
    TextMetrics {
        id: referenceMetrics
        text: "M"
        font: Kirigami.Theme.defaultFont
    }

    property string lastDomain: "UNSET"
    property string lastContext: "UNSET"

    // Creation-context shims: the component resolves i18n* unqualified and there is
    // no localized context under qmltestrunner. Rendering %1 the way KLocalizedString
    // would lets the readout cases assert a real string.
    function i18ndc(domain, ctx, msg, arg) {
        testCase.lastDomain = domain;
        testCase.lastContext = ctx;
        return ("" + msg).replace("%1", arg);
    }

    function makeRow(props) {
        const component = Qt.createComponent(testCase.targetUrl);
        tryVerify(function() { return component.status !== Component.Loading; }, 6000);
        compare(component.status, Component.Ready, component.errorString());
        const row = createTemporaryObject(component, host, props || {});
        verify(row, "PercentSliderRow.qml failed to instantiate");
        return row;
    }

    // caption, slider, readout -- in that order. TextMetrics is not an Item, so it
    // lands in resources and does not shift these.
    function caption(row) { return row.children[0]; }
    function slider(row) { return row.children[1]; }
    function readout(row) { return row.children[2]; }

    function test_captionComesFromTheConsumer() {
        const row = makeRow({label: "Padding"});
        compare(caption(row).text, "Padding");
    }

    function test_readoutTracksTheSliderValue() {
        const row = makeRow({from: 0, to: 100, value: 40});
        compare(readout(row).text, "40 %");

        row.value = 85;
        compare(slider(row).value, 85, "the row's value must drive the slider");
        compare(readout(row).text, "85 %", "the readout must follow the slider");
    }

    // The slider's value is a real. Handing the raw one to i18ndc reads out as
    // "42.5 %", so the readout has to go through an int first.
    function test_readoutRendersAWholePercentage() {
        const row = makeRow({from: 0, to: 100, value: 42.5});
        compare(slider(row).value, 42.5, "the slider must keep the unrounded value");
        compare(readout(row).text, "42 %");
    }

    // Indicator config engines set their own domain, so the lookup has to name ours.
    function test_readoutNamesTheApplicationDomain() {
        makeRow({value: 10});
        compare(testCase.lastDomain, "latte-dock");
        compare(testCase.lastContext, "number in percentage, e.g. 85 %");
    }

    function test_releaseEmitsTheSliderValueOnce() {
        const row = makeRow({from: 0, to: 100, value: 30});
        const spy = createTemporaryObject(signalSpyComponent, testCase, {target: row, signalName: "released"});
        verify(spy, "no SignalSpy");
        verify(spy.valid, "PercentSliderRow declares no released signal");

        const control = slider(row);
        mousePress(control, control.width / 2, control.height / 2);
        compare(spy.count, 0, "nothing may be committed while the handle is held");

        mouseRelease(control, control.width / 2, control.height / 2);
        compare(spy.count, 1, "release must commit exactly once");
        compare(spy.signalArguments[0][0], control.value, "the raw slider value must be handed back");
    }

    // Pins the width hoist: the readout is fixed at four characters so the slider
    // does not resize as the number grows.
    function test_readoutIsFixedAtFourCharacters() {
        const row = makeRow({value: 50});
        const expected = referenceMetrics.advanceWidth * 4;
        verify(expected > 0, "reference metrics did not resolve");
        compare(readout(row).Layout.minimumWidth, expected);
        compare(readout(row).Layout.maximumWidth, expected);
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
