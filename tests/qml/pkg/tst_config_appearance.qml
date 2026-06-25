// Coverage for the plasmoid's Appearance config page
// (plasmoid/package/contents/ui/config/ConfigAppearance.qml). The page reads one
// unqualified context name, `plasmoid` (via plasmoid.formFactor for the `vertical`
// binding). A bare qmltestrunner has no `plasmoid`, so we build a wrapper Item
// that declares it as a property and loads the staged (instrumented) page through
// a Loader; the Loader-created item inherits the wrapper's QML context, so the
// page's unqualified `plasmoid` lookup resolves to our mock.
//
// The real logic on this page lives in the icon-size ComboBox: its
// onCurrentIndexChanged maps the selected index to cfg_iconSize (16/22/32/48/64/
// 96/128/256, default 64) and its onRealValueChanged maps a programmatically set
// realValue back to a currentIndex while `startup` is still true. Each test drives
// one of those handlers and asserts the resulting mapping.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "ConfigAppearance"
    when: windowShown
    visible: true
    width: 400
    height: 600

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/config/ConfigAppearance.qml")

    // PlasmaCore.Types.Vertical == 3; the page compares plasmoid.formFactor against
    // it for `vertical`. Horizontal (2) keeps `vertical` false.
    readonly property int formFactorHorizontal: 2
    readonly property int formFactorVertical: 3

    // Wrapper supplies the page's only context global (`plasmoid`) and loads the
    // instrumented page through a Loader so the page is created inside this context.
    function makeWrapper(formFactor) {
        const wrapperSrc =
            'import QtQuick\n'
          + 'Item {\n'
          + '  id: host\n'
          + '  width: 400; height: 600\n'
          + '  property var plasmoid: QtObject { property int formFactor: ' + formFactor + ' }\n'
          + '  function i18n() { return arguments.length > 0 ? "" + arguments[0] : ""; }\n'
          + '  property Loader pageLoader: Loader { anchors.fill: parent; source: host.pageSource }\n'
          + '  property url pageSource\n'
          + '}\n';
        const w = Qt.createQmlObject(wrapperSrc, root, "configAppearanceWrapper");
        w.pageSource = targetUrl;
        return w;
    }

    function pageOf(w) {
        tryVerify(function() { return w.pageLoader.status === Loader.Ready
                                   || w.pageLoader.status === Loader.Error; }, 4000);
        verify(w.pageLoader.status === Loader.Ready,
               "page load failed status=" + w.pageLoader.status);
        const item = w.pageLoader.item;
        verify(item, "no loaded page item");
        return item;
    }

    // Depth-first hunt for an object carrying a given property name.
    function findWithProperty(node, propName) {
        if (!node)
            return null;
        if (node[propName] !== undefined)
            return node;
        const buckets = [node.children, node.resources, node.contentChildren];
        for (var b = 0; b < buckets.length; b++) {
            const list = buckets[b];
            if (!list)
                continue;
            for (var i = 0; i < list.length; i++) {
                const hit = findWithProperty(list[i], propName);
                if (hit)
                    return hit;
            }
        }
        return null;
    }

    // The icon-size ComboBox is the only object on the page carrying `realValue`
    // plus `currentIndex` plus a `startup` flag.
    function iconComboOf(page) {
        const all = collectAll(page, []);
        for (var i = 0; i < all.length; i++) {
            const o = all[i];
            if (o && o.realValue !== undefined && o.currentIndex !== undefined
                  && o.startup !== undefined)
                return o;
        }
        return null;
    }

    function collectAll(node, acc) {
        if (!node || acc.indexOf(node) !== -1)
            return acc;
        acc.push(node);
        const buckets = [node.children, node.resources, node.contentChildren];
        for (var b = 0; b < buckets.length; b++) {
            const list = buckets[b];
            if (!list)
                continue;
            for (var i = 0; i < list.length; i++)
                collectAll(list[i], acc);
        }
        return acc;
    }

    // The page loads with the mocked plasmoid context wired; the icon ComboBox is
    // reachable and the binding-driven `vertical` resolves against plasmoid.formFactor.
    function test_page_loads_with_plasmoid_context() {
        const w = makeWrapper(formFactorHorizontal);
        const page = pageOf(w);
        compare(page.vertical, false, "horizontal formFactor -> vertical binding false");
        verify(iconComboOf(page), "icon-size ComboBox not reachable");
        w.destroy();
    }

    // `vertical` binds to plasmoid.formFactor == Vertical(3). Build with a vertical
    // form factor and assert the binding takes the true branch.
    function test_vertical_binding_tracks_formfactor() {
        const w = makeWrapper(formFactorVertical);
        const page = pageOf(w);
        compare(page.vertical, true, "vertical formFactor -> vertical binding true");
        w.destroy();
    }

    // onCurrentIndexChanged maps each combobox index to the documented icon size.
    // Drive currentIndex across the full 0..7 range and assert realValue (which the
    // page aliases to cfg_iconSize).
    function test_current_index_maps_to_icon_size() {
        const w = makeWrapper(formFactorHorizontal);
        const page = pageOf(w);
        const cmb = iconComboOf(page);
        verify(cmb, "icon-size ComboBox not found");

        // Consume the startup phase first so onRealValueChanged stops fighting our
        // index writes: setting realValue once flips startup=false.
        cmb.realValue = 64;
        cmb.startup = false;

        const expected = [16, 22, 32, 48, 64, 96, 128, 256];
        for (var i = 0; i < expected.length; i++) {
            // onCurrentIndexChanged only fires on an actual change, so step the
            // index away before landing on the value under test.
            cmb.currentIndex = (i === 0) ? 7 : 0;
            cmb.currentIndex = i;
            compare(cmb.realValue, expected[i],
                    "index " + i + " should map to " + expected[i] + "px");
        }
        w.destroy();
    }

    // The onCurrentIndexChanged switch has a default arm: any index outside 0..7
    // stores 64.
    function test_current_index_default_arm() {
        const w = makeWrapper(formFactorHorizontal);
        const page = pageOf(w);
        const cmb = iconComboOf(page);
        verify(cmb, "icon-size ComboBox not found");

        cmb.realValue = 32;
        cmb.startup = false;

        cmb.currentIndex = 2;       // known arm to move it off 64 first
        compare(cmb.realValue, 32);
        cmb.currentIndex = 9;       // out of range -> default arm
        compare(cmb.realValue, 64, "out-of-range index falls to default 64");
        w.destroy();
    }

    // onRealValueChanged runs only while startup is true: it maps a stored
    // realValue back to the matching currentIndex, then clears startup. Force
    // startup back on, set realValue, and assert the index it selects.
    function test_real_value_maps_back_to_index_during_startup() {
        const w = makeWrapper(formFactorHorizontal);
        const page = pageOf(w);
        const cmb = iconComboOf(page);
        verify(cmb, "icon-size ComboBox not found");

        const sizeToIndex = [[16, 0], [22, 1], [32, 2], [48, 3],
                             [64, 4], [96, 5], [128, 6], [256, 7]];
        for (var i = 0; i < sizeToIndex.length; i++) {
            // onRealValueChanged only fires on an actual change, so push realValue to
            // a sentinel first, then re-arm startup and set the value under test.
            cmb.realValue = -1;
            cmb.startup = true;                 // re-arm the startup mapping
            cmb.realValue = sizeToIndex[i][0];
            compare(cmb.currentIndex, sizeToIndex[i][1],
                    sizeToIndex[i][0] + "px should select index " + sizeToIndex[i][1]);
            compare(cmb.startup, false, "onRealValueChanged clears startup after mapping");
        }
        w.destroy();
    }

    // onRealValueChanged default arm: an unmapped realValue selects index 4 (64px)
    // while startup is true.
    function test_real_value_default_arm_selects_64() {
        const w = makeWrapper(formFactorHorizontal);
        const page = pageOf(w);
        const cmb = iconComboOf(page);
        verify(cmb, "icon-size ComboBox not found");

        cmb.startup = true;
        cmb.realValue = 77;                     // not in the size table
        compare(cmb.currentIndex, 4, "unmapped size falls to default index 4 (64px)");
        compare(cmb.startup, false);
        w.destroy();
    }

    // Once startup is false, onRealValueChanged must NOT rewrite currentIndex — the
    // guard protects user-driven combobox changes from being clobbered.
    function test_real_value_guard_after_startup() {
        const w = makeWrapper(formFactorHorizontal);
        const page = pageOf(w);
        const cmb = iconComboOf(page);
        verify(cmb, "icon-size ComboBox not found");

        // Burn startup, settle on a known index.
        cmb.startup = false;
        cmb.currentIndex = 1;                    // -> realValue 22 via onCurrentIndexChanged
        compare(cmb.realValue, 22);

        // Now poke realValue directly; with startup false the handler is a no-op,
        // so currentIndex stays put.
        cmb.realValue = 256;
        compare(cmb.currentIndex, 1, "post-startup realValue change must not move the index");
        w.destroy();
    }
}
