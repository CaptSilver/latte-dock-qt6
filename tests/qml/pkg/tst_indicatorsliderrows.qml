/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The percent-slider rows on the two shipped indicator configuration pages. The
// control itself is covered by tests/qml/tst_percentsliderrow.qml; what is pinned
// here is what each page asks of it -- the range and the expression that turns the
// slider's percentage back into a config value, since those are per-row and a
// transposed bound or a dropped rounding is invisible to a compile-only check.
//
// The glow Opacity row is deliberately NOT one of these: it writes config on every
// value change rather than on release, so folding it in would change behavior.
import QtQuick
import QtTest
import Stage 1.0

TestCase {
    id: root
    name: "IndicatorSliderRows"
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
        tryVerify(function() { return c.status !== Component.Loading; }, 6000);
        compare(c.status, Component.Ready, c.errorString());
        const page = createTemporaryObject(c, root, {width: 460, visible: true});
        verify(page, "no page item");
        return page;
    }

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
        return out;
    }

    // A captioned row, as opposed to the bare slider the Opacity row still uses:
    // only the shared control carries a `label` alongside the range properties.
    function isSliderRow(o) {
        return typeof o.label === "string"
            && typeof o.from !== "undefined"
            && typeof o.to !== "undefined"
            && typeof o.stepSize !== "undefined";
    }

    // Every shared row wraps a slider of its own, so a plain "is a slider" test would
    // also collect those. Only sliders the page declares directly count here.
    function isBareSlider(o) {
        if (typeof o.label !== "undefined" || typeof o.pressed === "undefined"
                || typeof o.from === "undefined" || typeof o.to === "undefined")
            return false;
        for (var p = o.parent; p; p = p.parent)
            if (isSliderRow(p))
                return false;
        return true;
    }

    function rows(page) { return collect(page, isSliderRow, [], []); }

    function rowByLabel(page, text) {
        const all = rows(page);
        for (var i = 0; i < all.length; i++)
            if (all[i].label === text)
                return all[i];
        return null;
    }

    // label -> [from, to], transcribed from the rows these replaced.
    function checkRange(page, label, from, to) {
        const row = rowByLabel(page, label);
        verify(row, 'no "' + label + '" row');
        compare(row.from, from, label + " lower bound");
        compare(row.to, to, label + " upper bound");
        compare(row.stepSize, 1, label + " step");
    }

    function test_default_page_rows_keep_their_ranges() {
        cfg = makeConfig();
        const page = loadPage(defaultUrl);
        compare(rows(page).length, 4, "the default page has four percent rows; Opacity is not one of them");
        checkRange(page, "Thickness", 3, 25);
        checkRange(page, "Position", 0, 30);
        checkRange(page, "Padding", 0, 80);
        checkRange(page, "Corner Margin", 0, 100);
    }

    function test_plasma_page_rows_keep_their_ranges() {
        cfg = makeConfig();
        const page = loadPage(plasmaUrl);
        compare(rows(page).length, 2, "the Plasma page has two percent rows");
        checkRange(page, "Padding", 0, 80);
        checkRange(page, "Corner Margin", 0, 100);
    }

    // Each row divides by 100 on the way back into config, and multiplies by 100 on the way in.
    // Both halves are asserted together: pinning only the commit leaves a transposed `value:` read
    // invisible, which is exactly the slip six hand-transcribed rows invite.
    function test_default_page_rows_commit_on_release() {
        cfg = makeConfig();
        const page = loadPage(defaultUrl);

        rowByLabel(page, "Position").released(24);
        fuzzyCompare(cfg.thickMargin, 0.24, 0.0001);
        fuzzyCompare(rowByLabel(page, "Position").value, 24, 0.0001);

        rowByLabel(page, "Padding").released(65);
        fuzzyCompare(cfg.lengthPadding, 0.65, 0.0001);
        fuzzyCompare(rowByLabel(page, "Padding").value, 65, 0.0001);

        rowByLabel(page, "Corner Margin").released(40);
        fuzzyCompare(cfg.backgroundCornerMargin, 0.40, 0.0001);
        fuzzyCompare(rowByLabel(page, "Corner Margin").value, 40, 0.0001);
    }

    function test_plasma_page_rows_commit_on_release() {
        cfg = makeConfig();
        const page = loadPage(plasmaUrl);

        rowByLabel(page, "Padding").released(12);
        fuzzyCompare(cfg.lengthPadding, 0.12, 0.0001);
        fuzzyCompare(rowByLabel(page, "Padding").value, 12, 0.0001);

        rowByLabel(page, "Corner Margin").released(77);
        fuzzyCompare(cfg.backgroundCornerMargin, 0.77, 0.0001);
        fuzzyCompare(rowByLabel(page, "Corner Margin").value, 77, 0.0001);
    }

    // Thickness is the odd one out: it rounds to two decimals on the way in, so the
    // stored value is a step of 0.01 and not the slider's full-precision quotient.
    function test_thickness_rounds_to_two_decimals() {
        cfg = makeConfig();
        const page = loadPage(defaultUrl);

        rowByLabel(page, "Thickness").released(17);
        fuzzyCompare(cfg.size, 0.17, 0.0001);

        rowByLabel(page, "Thickness").released(7.777);
        compare(cfg.size, 0.08, "the size key is rounded to two decimals, not stored raw");
    }

    // The glow Opacity slider was left alone on purpose: it writes on every value
    // change, and the shared row only commits on release.
    function test_glow_opacity_still_writes_without_a_release() {
        cfg = makeConfig();
        const page = loadPage(defaultUrl);

        const bare = collect(page, isBareSlider, [], []);
        compare(bare.length, 1, "only the Opacity row should still be a bare slider");
        compare(bare[0].stepSize, 5, "the Opacity slider keeps its five-percent step");

        bare[0].value = 60;
        fuzzyCompare(cfg.glowOpacity, 0.60, 0.0001, "opacity must update live, without a release");
    }
}
