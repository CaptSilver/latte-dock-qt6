/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Coverage for the two shipped indicator configuration pages. Both are plain
// ColumnLayouts that read context globals supplied by the indicator settings view
// (indicator, dialog) plus i18n/i18nc. None of those exist in a bare qmltestrunner,
// so the TestCase is named `id: root` and declares each one as a property or a
// lowercase-id'd QtObject on itself; QML resolves the pages' unqualified lookups
// against their creation context, which is this.
//
// These were the only config checkboxes in the tree with no click coverage, and all
// five of them were dead: QQC2's CheckBox has an `indicator` property of its own, so
// the `indicator.configuration.x` these pages read inside a CheckBox block resolved to
// the control's tick delegate instead of the settings view's indicator object. QML
// answers that with undefined rather than an error, so the pages rendered normally
// while no checkbox showed its saved value or saved a click.
import QtQuick
import QtTest
import Stage 1.0

TestCase {
    id: root
    name: "IndicatorConfigPages"
    when: windowShown
    visible: true
    width: 500
    height: 800

    readonly property url defaultUrl: Stage.share("latte/indicators/default/package/config/config.qml")
    readonly property url plasmaUrl: Stage.share("latte/indicators/org.kde.latte.plasma/package/config/config.qml")

    property var cfg
    property QtObject indicator: QtObject {
        property var configuration: root.cfg
        property bool latteTasksArePresent: true
    }

    QtObject {
        id: dialog
        property int optionsWidth: 420
    }

    function i18n() { return arguments.length > 0 ? "" + arguments[0] : ""; }
    function i18nc() { return arguments.length > 1 ? "" + arguments[1] : ""; }

    // Union of both pages' keys, so one mock serves either page.
    function makeConfig() {
        return Qt.createQmlObject('import QtQuick; QtObject {\n'
            + ' property int activeStyle: 0;\n'
            + ' property real size: 0.10;\n'
            + ' property bool minimizedTaskColoredDifferently: false;\n'
            + ' property bool extraDotOnActive: false;\n'
            + ' property bool glowEnabled: true;\n'
            + ' property bool enabledForApplets: true;\n'
            + ' property int glowApplyTo: 2;\n'
            + ' property real glowOpacity: 0.35;\n'
            + ' property real lengthPadding: 0.08;\n'
            + ' property real thickMargin: 0.0;\n'
            + ' property real backgroundCornerMargin: 1.0;\n'
            + ' property bool reversed: false;\n'
            + ' property bool clickedAnimationEnabled: false;\n'
            + '}', root, "indicatorConfigMock");
    }

    function loadPage(url) {
        const c = Qt.createComponent(url);
        tryVerify(function() { return c.status === Component.Ready || c.status === Component.Error; }, 6000);
        verify(c.status === Component.Ready, c.errorString());
        const page = createTemporaryObject(c, root, {width: 460, visible: true});
        verify(page, "no page item");
        return page;
    }

    // Depth-first collect over children, resources and contentChildren; seen[] dedupes
    // so an object reachable by more than one edge is not clicked twice.
    function collect(node, pred, out, seen) {
        if (!node)
            return out;
        for (var s = 0; s < seen.length; s++)
            if (seen[s] === node)
                return out;
        seen.push(node);
        if (pred(node))
            out.push(node);
        const kids = node.children ? node.children : [];
        for (var i = 0; i < kids.length; i++)
            collect(kids[i], pred, out, seen);
        const res = node.resources ? node.resources : [];
        for (var j = 0; j < res.length; j++)
            collect(res[j], pred, out, seen);
        const cc = node.contentChildren ? node.contentChildren : [];
        for (var k = 0; k < cc.length; k++)
            collect(cc[k], pred, out, seen);
        return out;
    }

    function isCheckBox(o) {
        return typeof o.clicked === "function"
            && typeof o.tooltip !== "undefined"
            && typeof o.value !== "undefined"
            && typeof o.checked !== "undefined";
    }

    readonly property var boolKeys: [
        "minimizedTaskColoredDifferently", "extraDotOnActive", "glowEnabled",
        "enabledForApplets", "reversed", "clickedAnimationEnabled"
    ]

    function snapshotBools() {
        var s = {};
        for (var i = 0; i < boolKeys.length; i++)
            s[boolKeys[i]] = cfg[boolKeys[i]];
        return s;
    }

    function changedBoolKeys(before) {
        var ch = [];
        for (var i = 0; i < boolKeys.length; i++)
            if (cfg[boolKeys[i]] !== before[boolKeys[i]])
                ch.push(boolKeys[i]);
        return ch;
    }

    // Clicks every checkbox and describes the ones that did not write exactly one key.
    // Two writes means a handler stacked on top of the built-in one; none means the box
    // is reading a target that is not there, which QML answers with undefined and no error.
    function badWritesOnClickingEach(page) {
        const boxes = collect(page, isCheckBox, [], []);
        var bad = [];
        for (var i = 0; i < boxes.length; i++) {
            const before = snapshotBools();
            boxes[i].clicked();
            const ch = changedBoolKeys(before);
            if (ch.length !== 1)
                bad.push('"' + boxes[i].text + '" wrote ' + ch.length + " keys [" + ch.join(",") + "]");
        }
        return bad.join("; ");
    }

    function test_default_indicator_checkboxes_write_config() {
        cfg = makeConfig();
        const page = loadPage(defaultUrl);
        compare(collect(page, isCheckBox, [], []).length, 4, "expected 4 checkboxes on the default indicator page");
        compare(badWritesOnClickingEach(page), "", "every checkbox must write exactly one config key");
    }

    // Only one of this page's three checkbox blocks is live; the other two are
    // commented out and must not come back by accident.
    function test_plasma_indicator_checkboxes_write_config() {
        cfg = makeConfig();
        const page = loadPage(plasmaUrl);
        compare(collect(page, isCheckBox, [], []).length, 1, "expected 1 live checkbox on the Plasma indicator page");
        compare(badWritesOnClickingEach(page), "", "every checkbox must write exactly one config key");
    }

    // A checkbox reads its key back as well as writing it, so an external change has
    // to reach the control. This is what a misspelled key would silently break.
    function test_checkbox_follows_an_external_config_change() {
        cfg = makeConfig();
        const page = loadPage(defaultUrl);
        const boxes = collect(page, isCheckBox, [], []);

        function byText(t) {
            for (var i = 0; i < boxes.length; i++)
                if (boxes[i].text === t)
                    return boxes[i];
            return null;
        }

        const reversed = byText("Reverse indicator style");
        verify(reversed, "reverse-style checkbox not found");
        compare(reversed.checked, false);
        cfg.reversed = true;
        compare(reversed.checked, true, "the control must follow a write to its key");
    }
}
