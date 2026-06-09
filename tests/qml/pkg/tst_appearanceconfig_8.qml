// Coverage for the dock's Appearance configuration page. The page is a
// PlasmaComponents.Page that reads context globals supplied by the real config
// view (plasmoid, dialog, latteView, viewConfig, universalSettings,
// themeExtended) plus the i18n/i18nc helpers. None exist in a bare
// qmltestrunner, so we build a wrapper Item that declares them as properties and
// loads the staged (instrumented) page through a Loader. Loader-created items
// inherit the wrapper's QML context, so the page's unqualified lookups resolve
// to our mocks.
//
// Every test here drives a real handler/function and asserts the observable
// effect: a configuration write, a returned index, or a mock side-effect. The
// three ScrollArea onClicked handlers need a live QQuickMouseEvent (a no-arg or
// QtObject-shaped emit throws in Qt6), so they're left to a live view, not
// gamed here.
import QtQuick
import QtTest

TestCase {
    id: tc
    name: "AppearanceConfig"
    when: windowShown
    visible: true
    width: 400
    height: 600

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/configuration/pages/AppearanceConfig.qml")

    // LatteCore.Types.Alignment: Center=0, Justify=10. The page branches on these.
    readonly property int alignCenter: 0
    readonly property int alignJustify: 10
    readonly property int alignLeft: 1

    // LatteContainment.Types.ThemeColorsGroup integer values.
    readonly property int colPlasma: 0
    readonly property int colReverse: 1
    readonly property int colSmart: 2
    readonly property int colDark: 3
    readonly property int colLight: 4
    readonly property int colLayout: 5

    // A fresh configuration object per test so writes from one test don't leak.
    function makeConfig() {
        return Qt.createQmlObject('import QtQuick; QtObject {\n'
            + ' property int iconSize: 48;\n'
            + ' property real proportionIconSize: -1;\n'
            + ' property int zoomLevel: 16;\n'
            + ' property real maxLength: 90;\n'   // Double in the real config (config/main.xml)
            + ' property real minLength: 10;\n'
            + ' property real offset: 0;\n'
            + ' property int alignment: 0;\n'   // Center
            + ' property bool animationsEnabled: true;\n'
            + ' property bool maximizeWhenMaximized: false;\n'
            + ' property int lengthExtMargin: 5;\n'
            + ' property int thickMargin: 8;\n'
            + ' property int screenEdgeMargin: -1;\n'
            + ' property int themeColors: 0;\n'
            + ' property int windowColors: 0;\n'
            + ' property bool useThemePanel: true;\n'
            + ' property int panelSize: 100;\n'
            + ' property int panelTransparency: -1;\n'
            + ' property int backgroundRadius: -1;\n'
            + ' property int backgroundShadowSize: -1;\n'
            + ' property bool blurEnabled: false;\n'
            + ' property bool panelShadows: true;\n'
            + ' property bool panelOutline: false;\n'
            + ' property bool backgroundAllCorners: false;\n'
            + ' property bool solidBackgroundForMaximized: false;\n'
            + ' property bool backgroundOnlyOnMaximized: false;\n'
            + ' property bool disablePanelShadowForMaximized: false;\n'
            + ' property bool plasmaBackgroundForPopups: false;\n'
            + '}', tc, "appearanceConfigMock");
    }

    // Build the wrapper that supplies the page's context globals and loads the
    // instrumented page. viewConfig.isReady starts true so the update* handlers
    // take their write branch instead of the clamp branch; flipping it lets us
    // exercise the other side too.
    function makeWrapper(cfg, isReady) {
        const wrapperSrc =
            'import QtQuick\n'
          + 'Item {\n'
          + '  id: dialog\n'
          + '  width: 400; height: 600\n'
          + '  property int appliedWidth: 380\n'
          + '  property int optionsWidth: 320\n'
          + '  property int subGroupSpacing: 8\n'
          + '  property bool advancedLevel: true\n'
          + '  property bool viewIsPanel: false\n'
          + '  property bool kirigamiLibraryIsFound: true\n'
          + '  property var plasmoid: QtObject { property var configuration }\n'
          + '  property var viewConfig: QtObject {\n'
          + '     property bool isReady: ' + (isReady ? 'true' : 'false') + '\n'
          + '     function syncGeometry() { dialog.syncCalls++; }\n'
          + '  }\n'
          + '  property int syncCalls: 0\n'
          + '  property var latteView: QtObject {\n'
          + '     property var visibility: QtObject { property int mode: 0 }\n'
          + '     property var metrics: QtObject { property int maxIconSize: 64 }\n'
          + '     property var indicator: QtObject { property var info: QtObject { property real minThicknessPadding: 0.0 } }\n'
          + '  }\n'
          + '  property var universalSettings: QtObject { property bool colorsScriptIsPresent: false }\n'
          + '  property var themeExtended: QtObject { property bool hasShadow: true }\n'
          + '  function i18n() { return arguments.length > 0 ? "" + arguments[0] : ""; }\n'
          + '  function i18nc() { return arguments.length > 1 ? "" + arguments[1] : ""; }\n'
          + '  property Loader pageLoader: Loader {\n'
          + '     anchors.fill: parent\n'
          + '     source: dialog.pageSource\n'
          + '  }\n'
          + '  property url pageSource\n'
          + '}\n';
        const w = Qt.createQmlObject(wrapperSrc, tc, "appearanceConfigWrapper");
        w.plasmoid.configuration = cfg;
        // Set the source last so the page is created with the mocks already wired.
        w.pageSource = targetUrl;
        return w;
    }

    function pageOf(w) {
        tryVerify(function() { return w.pageLoader.status === Loader.Ready
                                   || w.pageLoader.status === Loader.Error; }, 4000);
        verify(w.pageLoader.status === Loader.Ready,
               "page load failed: " + w.pageLoader.sourceComponent
               + " status=" + w.pageLoader.status);
        const item = w.pageLoader.item;
        verify(item, "no loaded page item");
        return item;
    }

    // Depth-first hunt for an object carrying a given function name; used to
    // reach the sliders' update* methods which live deep in the layout tree.
    function findWithFunction(root, fnName) {
        if (!root)
            return null;
        if (typeof root[fnName] === "function")
            return root;
        const kids = root.children ? root.children : [];
        for (var i = 0; i < kids.length; i++) {
            const hit = findWithFunction(kids[i], fnName);
            if (hit)
                return hit;
        }
        const res = root.resources ? root.resources : [];
        for (var j = 0; j < res.length; j++) {
            const hit2 = findWithFunction(res[j], fnName);
            if (hit2)
                return hit2;
        }
        return null;
    }

    // Flatten the whole instance tree (children + resources + contentChildren).
    function collectAll(root, acc) {
        if (!root || acc.indexOf(root) !== -1)
            return;
        acc.push(root);
        const buckets = [root.children, root.resources, root.contentChildren];
        for (var b = 0; b < buckets.length; b++) {
            const list = buckets[b];
            if (!list)
                continue;
            for (var i = 0; i < list.length; i++)
                collectAll(list[i], acc);
        }
    }

    // Construction wires each slider's valueChanged -> update* connection in
    // Component.onCompleted. Prove that wiring is live: change a slider value and
    // watch the connected handler write configuration.
    function test_completed_handlers_wire_value_connections() {
        const cfg = makeConfig();
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        // valueChanged -> updateIconSize connection from Component.onCompleted@96.
        const iconOwner = findWithFunction(page, "updateIconSize");
        verify(iconOwner, "icon size slider not found");
        iconOwner.value = 200;
        compare(cfg.iconSize, 200, "valueChanged connection did not write iconSize");

        // valueChanged -> updatePanelSize connection from Component.onCompleted@921.
        const panelOwner = findWithFunction(page, "updatePanelSize");
        verify(panelOwner, "panel size slider not found");
        panelOwner.value = 73;
        compare(cfg.panelSize, 73, "valueChanged connection did not write panelSize");

        w.destroy();
    }

    // Drive the simple write-through update* functions directly and assert the
    // exact configuration value each lands.
    function test_update_functions_write_through() {
        const cfg = makeConfig();
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const iconOwner = findWithFunction(page, "updateIconSize");
        iconOwner.value = 64;
        iconOwner.updateIconSize();
        compare(cfg.iconSize, 64);

        const panelOwner = findWithFunction(page, "updatePanelSize");
        panelOwner.value = 55;
        panelOwner.updatePanelSize();
        compare(cfg.panelSize, 55);

        const transpOwner = findWithFunction(page, "updatePanelTransparency");
        transpOwner.value = 42;
        transpOwner.updatePanelTransparency();
        compare(cfg.panelTransparency, 42);

        const radiusOwner = findWithFunction(page, "updateBackgroundRadius");
        radiusOwner.value = 12;
        radiusOwner.updateBackgroundRadius();
        compare(cfg.backgroundRadius, 12);

        const shadowOwner = findWithFunction(page, "updateBackgroundShadowSize");
        shadowOwner.value = 9;
        shadowOwner.updateBackgroundShadowSize();
        compare(cfg.backgroundShadowSize, 9);

        w.destroy();
    }

    // updateProportionIconSize: value===1 stores -1, anything else stores value.
    function test_update_proportion_icon_size_branches() {
        const cfg = makeConfig();
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const propOwner = findWithFunction(page, "updateProportionIconSize");
        verify(propOwner, "proportion slider not found");

        propOwner.value = 1;
        propOwner.updateProportionIconSize();
        compare(cfg.proportionIconSize, -1, "value===1 should store -1");

        propOwner.value = 5;
        propOwner.updateProportionIconSize();
        compare(cfg.proportionIconSize, 5, "value!==1 should store value");

        w.destroy();
    }

    // updateZoomLevel maps the slider (1..2.25) to zoomLevel = round((value-1)*20).
    function test_update_zoom_level_math() {
        const cfg = makeConfig();
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const zoomOwner = findWithFunction(page, "updateZoomLevel");
        verify(zoomOwner, "zoom slider not found");

        zoomOwner.value = 1.5;
        zoomOwner.updateZoomLevel();
        compare(cfg.zoomLevel, 10, "round((1.5-1)*20) === 10");

        zoomOwner.value = 2.0;
        zoomOwner.updateZoomLevel();
        compare(cfg.zoomLevel, 20, "round((2.0-1)*20) === 20");

        w.destroy();
    }

    // updateMaxLength/updateMinLength write branch (viewConfig.isReady=true) with a
    // Left alignment so no offset rebalancing fires: maxLength = max(value, minLength, 1).
    function test_max_min_length_write_branch() {
        const cfg = makeConfig();
        cfg.alignment = alignLeft;     // not Center/Justify -> simplest branch
        cfg.minLength = 10;
        cfg.offset = 0;
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const maxOwner = findWithFunction(page, "updateMaxLength");
        maxOwner.value = 80;
        maxOwner.updateMaxLength();
        compare(cfg.maxLength, 80, "max(80, minLength 10, 1) === 80");

        // value below minLength clamps up to minLength.
        maxOwner.value = 5;
        maxOwner.updateMaxLength();
        compare(cfg.maxLength, 10, "max(5, minLength 10, 1) === 10");

        const minOwner = findWithFunction(page, "updateMinLength");
        minOwner.value = 22;
        minOwner.updateMinLength();
        compare(cfg.minLength, 22, "minLength written through directly");

        w.destroy();
    }

    // updateMaxLength Center branch: a large total pushes the offset to keep the
    // panel on screen. With alignment=Center, offset rebalances via suggestedValue.
    function test_max_length_center_rebalances_offset() {
        const cfg = makeConfig();
        cfg.alignment = alignCenter;
        cfg.offset = 40;
        cfg.minLength = 1;
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const maxOwner = findWithFunction(page, "updateMaxLength");
        maxOwner.value = 80;
        maxOwner.updateMaxLength();

        // maxLength = max(80, minLength 1, localMin 1) = 80.
        compare(cfg.maxLength, 80);
        // centeredCheck (|40| + 80/2 = 80 > 50) triggers the rebalance branch, which
        // pulls the offset in so the centered panel stays on screen. The branch's
        // guarantee is |offset| + maxLength/2 <= 50; assert that contract plus the
        // fact that it moved the offset off its starting 40.
        verify(cfg.offset !== 40, "centered rebalance should move the offset");
        verify(Math.abs(cfg.offset) + cfg.maxLength / 2 <= 50,
               "rebalanced offset keeps the centered panel on screen, got offset=" + cfg.offset);

        w.destroy();
    }

    // With viewConfig.isReady=false the length handlers take their else/clamp
    // branch: a value below minLength is clamped up rather than written.
    function test_length_clamp_branch_when_not_ready() {
        const cfg = makeConfig();
        cfg.minLength = 30;
        cfg.maxLength = 90;
        const w = makeWrapper(cfg, false);
        const page = pageOf(w);

        const maxOwner = findWithFunction(page, "updateMaxLength");
        const beforeMax = cfg.maxLength;
        maxOwner.value = 5;            // below minLength/localMinValue -> clamp value
        maxOwner.updateMaxLength();
        compare(maxOwner.value, 30, "value clamped up to minLength");
        compare(cfg.maxLength, beforeMax, "not-ready branch must not write maxLength");

        const minOwner = findWithFunction(page, "updateMinLength");
        const beforeMin = cfg.minLength;
        minOwner.value = 99;           // above maxLength -> clamp down
        minOwner.updateMinLength();
        compare(minOwner.value, cfg.maxLength, "value clamped down to maxLength");
        compare(cfg.minLength, beforeMin, "not-ready branch must not write minLength");

        w.destroy();
    }

    // updateOffset: userInputIsValid=true writes value straight through; the
    // sliderIsReady gate (isReady && from/to match the computed bounds) must hold.
    function test_offset_user_input_writes_value() {
        const cfg = makeConfig();
        cfg.alignment = alignCenter;
        cfg.maxLength = 60;            // screenLengthMaxFactor = (100-60)/2 = 20
        cfg.offset = 0;
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const offOwner = findWithFunction(page, "updateOffset");
        verify(offOwner, "offset slider not found");
        verify(offOwner.sliderIsReady, "offset slider bounds not synced -> updateOffset would no-op");

        offOwner.userInputIsValid = true;
        offOwner.value = 15;           // within [-20, 20]
        offOwner.updateOffset();
        compare(cfg.offset, 15, "valid user input written straight to offset");

        w.destroy();
    }

    // updateOffset: userInputIsValid=false re-clamps offset into [from,to] and
    // writes it back. Start from an out-of-range stored offset.
    function test_offset_reclamps_when_input_invalid() {
        const cfg = makeConfig();
        cfg.alignment = alignCenter;
        cfg.maxLength = 60;            // bounds [-20, 20]
        cfg.offset = 80;               // out of range
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const offOwner = findWithFunction(page, "updateOffset");
        verify(offOwner.sliderIsReady);
        offOwner.userInputIsValid = false;
        offOwner.updateOffset();
        compare(cfg.offset, 20, "offset re-clamped to the slider's max bound");

        w.destroy();
    }

    // colorsToIndex maps each ThemeColorsGroup id to a combobox index; unknown
    // ids fall through to undefined.
    function test_colors_to_index_mapping() {
        const cfg = makeConfig();
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const cb = findWithFunction(page, "colorsToIndex");
        verify(cb, "colorsToIndex owner not found");

        compare(cb.colorsToIndex(colPlasma), 0);
        compare(cb.colorsToIndex(colDark), 1);
        compare(cb.colorsToIndex(colLight), 2);
        compare(cb.colorsToIndex(colReverse), 3);
        compare(cb.colorsToIndex(colLayout), 3);
        compare(cb.colorsToIndex(colSmart), 4);
        compare(cb.colorsToIndex(99), undefined, "unknown color id falls through");

        w.destroy();
    }

    // The three length-row ScrollAreas nudge maxLength/minLength/offset by smallStep
    // (0.1) on a Ctrl-held wheel; without Ctrl the guarded body is skipped. Emit the
    // signal with a wheel object (the signal takes a QtObject, so our mock fits).
    // Each ScrollArea writes a different config field, so we discover which is which
    // by scrolling and watching which field moved.
    function test_scrollarea_ctrl_scroll_nudges_length_fields() {
        const cfg = makeConfig();
        cfg.maxLength = 50;
        cfg.minLength = 20;
        cfg.offset = 5;
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const all = [];
        collectAll(page, all);
        const wheelCtrl = Qt.createQmlObject(
            'import QtQuick; QtObject { property int modifiers: ' + Qt.ControlModifier + ' }',
            tc, "wheelCtrl");
        const wheelNone = Qt.createQmlObject(
            'import QtQuick; QtObject { property int modifiers: 0 }',
            tc, "wheelNone");

        var scrollAreas = [];
        for (var i = 0; i < all.length; i++) {
            if (all[i] && typeof all[i].scrolledUp === "function"
                       && typeof all[i].scrolledDown === "function")
                scrollAreas.push(all[i]);
        }
        verify(scrollAreas.length >= 3, "expected the three length-row ScrollAreas, found " + scrollAreas.length);

        // No modifier -> every handler runs but no config field changes.
        const snap0 = { max: cfg.maxLength, min: cfg.minLength, off: cfg.offset };
        for (var s = 0; s < scrollAreas.length; s++)
            scrollAreas[s].scrolledUp(wheelNone);
        compare(cfg.maxLength, snap0.max, "no-Ctrl scroll must not move maxLength");
        compare(cfg.minLength, snap0.min, "no-Ctrl scroll must not move minLength");
        compare(cfg.offset, snap0.off, "no-Ctrl scroll must not move offset");

        // Ctrl held: each ScrollArea raises exactly one field by 0.1. Track which.
        var movedMax = false, movedMin = false, movedOff = false;
        for (var t = 0; t < scrollAreas.length; t++) {
            const b = { max: cfg.maxLength, min: cfg.minLength, off: cfg.offset };
            scrollAreas[t].scrolledUp(wheelCtrl);
            if (Math.abs(cfg.maxLength - b.max - 0.1) < 1e-9) movedMax = true;
            if (Math.abs(cfg.minLength - b.min - 0.1) < 1e-9) movedMin = true;
            if (Math.abs(cfg.offset - b.off - 0.1) < 1e-9) movedOff = true;
            // scroll back down so the next ScrollArea starts from a known field state
            scrollAreas[t].scrolledDown(wheelCtrl);
        }
        verify(movedMax, "a ScrollArea Ctrl-scroll should raise maxLength by smallStep");
        verify(movedMin, "a ScrollArea Ctrl-scroll should raise minLength by smallStep");
        verify(movedOff, "a ScrollArea Ctrl-scroll should raise offset by smallStep");

        w.destroy();
    }

    // The HeaderSwitch background toggle emits pressed() (a no-arg signal) which
    // flips useThemePanel.
    function test_background_headerswitch_toggles_usethemepanel() {
        const cfg = makeConfig();
        cfg.useThemePanel = true;
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        // HeaderSwitch exposes a custom no-arg pressed() signal; the page's
        // onPressed handler toggles useThemePanel.
        const all = [];
        collectAll(page, all);
        var hs = null;
        for (var i = 0; i < all.length; i++) {
            // HeaderSwitch is the only object with both a 'pressed' signal and a
            // 'checked' property in this page's tree.
            if (all[i] && typeof all[i].pressed === "function"
                       && all[i].checked !== undefined
                       && all[i].text === "Background") {
                hs = all[i];
                break;
            }
        }
        verify(hs, "Background HeaderSwitch not found");

        hs.pressed();
        compare(cfg.useThemePanel, false, "pressed() should toggle useThemePanel off");
        hs.pressed();
        compare(cfg.useThemePanel, true, "second pressed() toggles it back on");

        w.destroy();
    }

    // The four background-style buttons (Blur/Shadows/Outline/All Corners) and the
    // dynamic-visibility checkboxes write configuration on clicked(). Emit each
    // clicked() (no-arg, AbstractButton) and assert the matching config flip.
    function test_background_buttons_and_checkboxes_write_config() {
        const cfg = makeConfig();
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const all = [];
        collectAll(page, all);

        function byText(t) {
            for (var i = 0; i < all.length; i++) {
                if (all[i] && all[i].text === t && typeof all[i].clicked === "function")
                    return all[i];
            }
            return null;
        }

        // Blur button: onClicked writes blurEnabled = checked.
        const blur = byText("Blur");
        verify(blur, "Blur button not found");
        blur.checked = true;
        blur.clicked();
        compare(cfg.blurEnabled, true, "Blur click writes checked state");

        // Shadows button: onClicked writes panelShadows = checked.
        const shadows = byText("Shadows");
        verify(shadows, "Shadows button not found");
        shadows.checked = false;
        shadows.clicked();
        compare(cfg.panelShadows, false, "Shadows click writes checked state");

        // Outline button: onClicked writes panelOutline = checked. Set checked then click.
        const outline = byText("Outline");
        verify(outline, "Outline button not found");
        outline.checked = true;
        outline.clicked();
        compare(cfg.panelOutline, true, "Outline click writes checked state");

        // All Corners button likewise.
        const corners = byText("All Corners");
        verify(corners, "All Corners button not found");
        corners.checked = true;
        corners.clicked();
        compare(cfg.backgroundAllCorners, true, "All Corners click writes checked state");

        // The three dynamic-visibility checkboxes each toggle their config flag.
        const solidTouch = byText("Prefer opaque background when touching any window");
        verify(solidTouch, "solid-when-touching checkbox not found");
        const beforeSolid = cfg.solidBackgroundForMaximized;
        solidTouch.clicked();
        compare(cfg.solidBackgroundForMaximized, !beforeSolid, "toggles solidBackgroundForMaximized");

        const hideWhenNotNeeded = byText("Hide background when not needed");
        verify(hideWhenNotNeeded, "hide-when-not-needed checkbox not found");
        const beforeHide = cfg.backgroundOnlyOnMaximized;
        hideWhenNotNeeded.clicked();
        compare(cfg.backgroundOnlyOnMaximized, !beforeHide, "toggles backgroundOnlyOnMaximized");

        const hideShadow = byText("Hide background shadow for maximized windows");
        verify(hideShadow, "hide-shadow checkbox not found");
        const beforeShadowHide = cfg.disablePanelShadowForMaximized;
        hideShadow.clicked();
        compare(cfg.disablePanelShadowForMaximized, !beforeShadowHide, "toggles disablePanelShadowForMaximized");

        // maximizeWhenMaximized checkbox toggles on click.
        const maximize = byText("Maximize panel length in presence of maximized windows");
        verify(maximize, "maximize checkbox not found");
        const beforeMax = cfg.maximizeWhenMaximized;
        maximize.clicked();
        compare(cfg.maximizeWhenMaximized, !beforeMax, "checkbox click toggles maximizeWhenMaximized");

        // plasmaBackgroundForPopups exceptions checkbox toggles on click.
        const popups = byText("Prefer Plasma background and colors for expanded applets");
        verify(popups, "popups checkbox not found");
        const beforePopups = cfg.plasmaBackgroundForPopups;
        popups.clicked();
        compare(cfg.plasmaBackgroundForPopups, !beforePopups, "checkbox click toggles plasmaBackgroundForPopups");

        w.destroy();
    }

    // The simple-margin sliders write configuration from their own onPressedChanged
    // when released (pressed===false). Emit pressedChanged() and assert the write.
    function test_margin_sliders_write_on_pressed_change() {
        const cfg = makeConfig();
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        const all = [];
        collectAll(page, all);

        // The three margin sliders have no named update* function -- they handle
        // onPressedChanged inline. Identify them by their distinctive from/to
        // ranges and that pressed===false (a headless slider is never pressed).
        var lengthMargin = null, screenEdge = null;
        for (var i = 0; i < all.length; i++) {
            const o = all[i];
            if (!o || typeof o.pressedChanged !== "function" || o.pressed === undefined)
                continue;
            if (o.from === 0 && o.to === 25 && o.value !== undefined)
                lengthMargin = o;          // lengthExtMargin slider (to: maxMargin 25)
            if (o.from === -1 && o.to === 256)
                screenEdge = o;            // screenEdgeMargin slider
        }
        verify(lengthMargin, "lengthExtMargin slider not found");
        verify(screenEdge, "screenEdgeMargin slider not found");

        lengthMargin.value = 17;
        lengthMargin.pressedChanged();     // released -> writes lengthExtMargin
        compare(cfg.lengthExtMargin, 17);

        screenEdge.value = 64;
        screenEdge.pressedChanged();
        compare(cfg.screenEdgeMargin, 64);

        w.destroy();
    }

    // The syncGeometry Timer's onTriggered calls viewConfig.syncGeometry(). Shrink
    // the interval and let it fire; assert the mock recorded the call.
    function test_syncGeometry_timer_calls_viewconfig() {
        const cfg = makeConfig();
        const w = makeWrapper(cfg, true);
        const page = pageOf(w);

        function findTimer(root) {
            if (!root)
                return null;
            if (root.interval === 400 && typeof root.restart === "function"
                    && typeof root.triggered !== "undefined")
                return root;
            const buckets = [root.resources, root.children, root.contentChildren];
            for (var b = 0; b < buckets.length; b++) {
                const list = buckets[b];
                if (!list)
                    continue;
                for (var i = 0; i < list.length; i++) {
                    const hit = findTimer(list[i]);
                    if (hit)
                        return hit;
                }
            }
            return null;
        }

        const timer = findTimer(page);
        verify(timer, "syncGeometry timer not found");
        compare(w.syncCalls, 0, "no sync before the timer fires");
        timer.interval = 1;
        timer.restart();
        tryVerify(function() { return w.syncCalls > 0; }, 2000, "syncGeometry never fired");

        w.destroy();
    }
}
