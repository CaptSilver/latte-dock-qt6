// Honest coverage for the dock Effects config page. The page reads a pile of
// ambient context objects (dialog, viewConfig, latteView, plasmoid) that only
// exist inside the live config dialog. We declare fake stand-ins with those
// exact ids in this file's scope and create the staged (instrumented) page
// inside this context so its free identifiers resolve. Every test below drives
// a real unit of the page and asserts the observable effect of running it:
// a configuration write, an indicator-type change, a tab-index change, or a
// viewConfig.setSticker side-effect.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "EffectsConfig"
    when: windowShown
    visible: true
    width: 400
    height: 600

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/configuration/pages/EffectsConfig.qml")

    property int stickerCalls: 0
    property var lastStickerArg: undefined

    // ---- Fake ambient context (ids match the free identifiers in the page) ----

    QtObject {
        id: configuration
        property bool appletShadowsEnabled: true
        property int shadowSize: 30
        property int shadowOpacity: 50
        property int shadowColorType: 0
        property string shadowColor: "080808"
        property bool animationsEnabled: true
        property int durationTime: 3
    }

    QtObject {
        id: indicatorObj
        property bool enabled: true
        property string type: "org.kde.latte.default"
        property var info: QtObject {
            property real minThicknessPadding: 0.0
        }
    }

    QtObject {
        id: latteView
        property var indicator: indicatorObj
    }

    QtObject {
        id: dialog
        property bool advancedLevel: true
        property real appliedWidth: 400
        property real optionsWidth: 360
        property real subGroupSpacing: 4
        property bool viewIsPanel: false
    }

    QtObject {
        id: viewConfig
        property bool isReady: true
        function setSticker(v) { root.stickerCalls++; root.lastStickerArg = v; }
    }

    QtObject {
        id: plasmoid
        property var configuration: configuration
        property int location: 4
    }

    // ---- Builder --------------------------------------------------------------

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed: " + c.errorString());
        return obj;
    }

    // Depth-first walk over the object tree collecting every visual/non-visual
    // child so the sliders, tab bar, header switches and buttons can be found.
    function collectAll(rootObj) {
        const out = [];
        const stack = [rootObj];
        const seen = [];
        while (stack.length) {
            const o = stack.pop();
            if (!o || seen.indexOf(o) !== -1)
                continue;
            seen.push(o);
            out.push(o);
            const kids = o.children;
            if (kids !== undefined && kids !== null) {
                for (var i = 0; i < kids.length; i++)
                    stack.push(kids[i]);
            }
            const res = o.resources;
            if (res !== undefined && res !== null) {
                for (var j = 0; j < res.length; j++)
                    stack.push(res[j]);
            }
            if (o.item !== undefined && o.item !== null)
                stack.push(o.item);
            if (o.contentItem !== undefined && o.contentItem !== null)
                stack.push(o.contentItem);
        }
        return out;
    }

    // Direct property probes only — never wrapped to swallow a throw. If a probe
    // throws it is a real failure of the assumption and the test should surface it.
    function ownersWith(all, fnName) {
        const hits = [];
        for (var i = 0; i < all.length; i++) {
            const o = all[i];
            if (o && typeof o[fnName] === "function")
                hits.push(o);
        }
        return hits;
    }

    function firstWith(all, fnName) {
        const hits = ownersWith(all, fnName);
        return hits.length ? hits[0] : null;
    }

    // Both shadow sliders own update*() that writes plasmoid.configuration when
    // the slider is not under the pointer. Call it directly and assert the write.
    function test_update_functions() {
        const obj = make();
        const all = collectAll(obj);

        const sizeOwners = ownersWith(all, "updateShadowSize");
        verify(sizeOwners.length > 0, "updateShadowSize owner not found");
        const sizeSlider = sizeOwners[0];
        verify(!sizeSlider.pressed, "size slider unexpectedly pressed");
        sizeSlider.value = 45;
        configuration.shadowSize = 0;
        sizeSlider.updateShadowSize();
        compare(configuration.shadowSize, 45, "updateShadowSize did not write value");

        const opOwners = ownersWith(all, "updateShadowOpacity");
        verify(opOwners.length > 0, "updateShadowOpacity owner not found");
        const opSlider = opOwners[0];
        verify(!opSlider.pressed, "opacity slider unexpectedly pressed");
        opSlider.value = 70;
        configuration.shadowOpacity = 0;
        opSlider.updateShadowOpacity();
        compare(configuration.shadowOpacity, 70, "updateShadowOpacity did not write value");
    }

    // Component.onCompleted connects each slider's valueChanged to its update*().
    // Mutating value emits valueChanged, which (slider not pressed) writes config.
    function test_value_changed_connection() {
        const obj = make();
        const all = collectAll(obj);

        const sizeSlider = firstWith(all, "updateShadowSize");
        verify(sizeSlider, "size slider not found");
        verify(!sizeSlider.pressed);
        configuration.shadowSize = 0;
        sizeSlider.value = 25;
        compare(configuration.shadowSize, 25, "valueChanged->updateShadowSize connection did not fire");
    }

    // tabBar.selectTab(type) maps the indicator type string to a currentIndex.
    function test_selecttab_sets_index() {
        const obj = make();
        const all = collectAll(obj);

        const tab = firstWith(all, "selectTab");
        verify(tab, "selectTab owner not found");

        tab.currentIndex = -1;
        tab.selectTab("org.kde.latte.default");
        compare(tab.currentIndex, 0, "Latte type should select tab 0");

        tab.currentIndex = -1;
        tab.selectTab("org.kde.latte.plasma");
        compare(tab.currentIndex, 1, "Plasma type should select tab 1");

        // An unknown type matches none of the branches and leaves the index put.
        tab.currentIndex = 1;
        tab.selectTab("org.kde.latte.does-not-exist");
        compare(tab.currentIndex, 1, "unknown type must not change the index");
    }

    // Each TabButton's onCheckedChanged writes latteView.indicator.type when it
    // becomes the checked tab. Moving currentIndex checks the matching button and
    // fires that handler; assert the indicator type it wrote.
    function test_tab_checked_writes_indicator_type() {
        const obj = make();
        const all = collectAll(obj);

        const tab = firstWith(all, "selectTab");
        verify(tab, "selectTab owner not found");

        indicatorObj.type = "scratch";
        tab.currentIndex = 1;
        compare(indicatorObj.type, "org.kde.latte.plasma",
                "checking the Plasma tab should write the plasma indicator type");

        tab.currentIndex = 0;
        compare(indicatorObj.type, "org.kde.latte.default",
                "checking the Latte tab should write the default indicator type");
    }

    // The StackView Connections.onCurrentItemChanged routes the pushed item's
    // type back through tabBar.selectTab() when viewConfig.isReady.
    function test_stackview_routes_to_selecttab() {
        const obj = make();
        const all = collectAll(obj);

        const tab = firstWith(all, "selectTab");
        verify(tab, "selectTab owner not found");

        var stack = null;
        for (var i = 0; i < all.length; i++) {
            const o = all[i];
            if (o && typeof o.push === "function" && typeof o.replace === "function"
                    && o.hasOwnProperty("currentItem")) {
                stack = o;
                break;
            }
        }
        verify(stack, "indicatorsStackView not found");

        tab.currentIndex = 0;
        const plasmaItem = Qt.createQmlObject(
            'import QtQuick; Item { property string type: "org.kde.latte.plasma" }',
            root, "stackItem");
        stack.push(plasmaItem);
        // onCurrentItemChanged -> selectTab("org.kde.latte.plasma") -> index 1.
        tryCompare(tab, "currentIndex", 1, 2000,
                   "pushing a plasma item should route to tab 1");
    }

    // The three HeaderSwitch rows expose a pressed() signal the page binds with
    // onPressed: to flip a configuration / indicator boolean. Emit the signal and
    // assert the toggle. Each header is matched by the boolean it owns.
    function test_header_switch_toggles() {
        const obj = make();
        const all = collectAll(obj);

        const switches = ownersWith(all, "pressed");
        // HeaderSwitch declares `signal pressed()` so it carries pressed() as an
        // invokable; filter to the three page-level ones by their `text`.
        var shadowSw = null, animSw = null, indSw = null;
        for (var i = 0; i < all.length; i++) {
            const o = all[i];
            if (!o || typeof o.pressed !== "function")
                continue;
            const t = o.text;
            if (t === undefined)
                continue;
            if (t.indexOf("Shadows") === 0) shadowSw = o;
            else if (t.indexOf("Animations") === 0) animSw = o;
            else if (t.indexOf("Indicators") === 0) indSw = o;
        }
        verify(shadowSw, "Shadows header switch not found");
        verify(animSw, "Animations header switch not found");
        verify(indSw, "Indicators header switch not found");

        const wasShadow = configuration.appletShadowsEnabled;
        shadowSw.pressed();
        compare(configuration.appletShadowsEnabled, !wasShadow, "Shadows header did not toggle");

        const wasAnim = configuration.animationsEnabled;
        animSw.pressed();
        compare(configuration.animationsEnabled, !wasAnim, "Animations header did not toggle");

        const wasInd = indicatorObj.enabled;
        indSw.pressed();
        compare(indicatorObj.enabled, !wasInd, "Indicators header did not toggle");
    }

    // A QML MouseArea exposes clicked/containsMouse/pressAndHold as inherited
    // members (so hasOwnProperty is useless here); probe the prototype directly.
    function isMouseArea(o) {
        return o && typeof o.clicked === "function"
                && o.containsMouse !== undefined
                && o.pressAndHold !== undefined;
    }

    // The user-color swatch MouseArea.onClicked selects UserColorShadow (== 2),
    // tells viewConfig to raise the sticker, and arms the color-dialog loader.
    // The page has several MouseAreas; only the swatch one touches these mocks,
    // so click them all and assert the swatch's specific net effect.
    function test_user_color_click() {
        const obj = make();
        const all = collectAll(obj);

        const areas = [];
        for (var i = 0; i < all.length; i++) {
            if (isMouseArea(all[i]))
                areas.push(all[i]);
        }
        verify(areas.length > 0, "no MouseArea found in the page");

        const before = root.stickerCalls;
        configuration.shadowColorType = 99;
        for (var k = 0; k < areas.length; k++)
            areas[k].clicked(null);

        compare(configuration.shadowColorType, 2, "swatch click should select UserColorShadow (2)");
        compare(root.stickerCalls, before + 1, "swatch click should call viewConfig.setSticker exactly once");
        compare(root.lastStickerArg, true, "swatch click should raise the sticker (true)");
    }
}
