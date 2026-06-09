// Coverage for the dock Behavior config page. The page reads ambient context
// objects supplied by the live config dialog (dialog, viewConfig,
// universalSettings, latteView, plasmoid, positioner, panelIsVertical). QML
// resolves a component's unqualified names against its *creation context*, so we
// declare each name as a lowercase-id'd QtObject on this TestCase (id: root) and
// create the staged, instrumented page parented into root. Its free identifiers
// then resolve to these stand-ins and its real handlers run against them.
//
// Every test drives one handler/function and asserts its observable effect: a
// recorded setNextLocation() call, a config write, a model rebuild, or a
// returned index. The four alignment and three visibility onPressedChanged
// handlers only act when the button's read-only `pressed` is true, which the
// offscreen synthetic-mouse pipeline never delivers, so those are not claimed
// here (see tests/coverage/live-only.md).
import QtQuick
import QtTest

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.latte.core 0.2 as LatteCore
import org.kde.latte.private.containment 0.1 as LatteContainment

TestCase {
    id: root
    name: "BehaviorConfig"
    when: windowShown
    visible: true
    width: 420
    height: 640

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/configuration/pages/BehaviorConfig.qml")

    // Records every positioner.setNextLocation(...) the page makes so location/
    // screen handlers can be asserted on their real side effect.
    property var locationCalls: []
    property bool panelIsVertical: false

    // ---- Fake ambient context (ids match the page's free identifiers) ----

    QtObject {
        id: positioner
        property string currentScreenName: "DP-1"
        function setNextLocation(layoutName, screensGroup, screenName, location, alignment) {
            root.locationCalls.push({layout: layoutName, group: screensGroup,
                                     screen: screenName, location: location,
                                     alignment: alignment});
        }
    }

    QtObject {
        id: visibilityObj
        property int mode: 0
        property int timerShow: 0
        property int timerHide: 0
        property bool enableKWinEdges: false
        property bool raiseOnDesktop: false
        property bool raiseOnActivity: false
    }

    QtObject {
        id: layoutObj
        property bool preferredForShortcutsTouched: false
    }

    QtObject {
        id: latteView
        property var positioner: positioner
        property var visibility: visibilityObj
        property var layout: layoutObj
        property int screensGroup: 0
        property bool onPrimary: true
        property bool byPassWM: false
        property bool isPreferredForShortcuts: false
        function isHighestPriorityView() { return true; }
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
        property bool showInlineProperties: false
        property bool isReady: true
        signal showSignal()
    }

    QtObject {
        id: universalSettings
        property var screens: []
        signal screensCountChanged()
    }

    QtObject {
        id: configuration
        property int alignment: 0
        property int activeWindowFilter: 0
        property int scrollAction: 0
        property bool dragActiveWindowEnabled: false
        property bool closeActiveWindowEnabled: false
        property bool titleTooltips: false
        property bool mouseWheelActions: false
        property bool autoSizeEnabled: false
        property int lastDodgeVisibilityMode: 0
        property int lastWindowsVisibilityMode: 0
        property int lastSidebarVisibilityMode: 0
        property int screenEdgeMargin: 0
        property int zoomLevel: 0
        property bool floatingInternalGapIsForced: false
        property bool hideFloatingGapForMaximized: false
        property bool floatingGapHidingWaitsMouse: false
        property bool floatingGapIsMirrored: false
        property int location: 0 // Floating: keep all four edges distinct from location
    }

    QtObject {
        id: plasmoid
        property var configuration: configuration
        property int location: 0 // Floating
    }

    // ---- Helpers --------------------------------------------------------------

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = c.createObject(root, { width: 400, height: 600, visible: true });
        verify(obj, "instantiate failed: " + c.errorString());
        return obj;
    }

    // Depth-first walk over children/resources/contentItem, deduped, collecting
    // every object the predicate accepts.
    function collect(node, pred) {
        const out = [];
        const stack = [node];
        const seen = [];
        while (stack.length) {
            const o = stack.pop();
            if (!o || seen.indexOf(o) !== -1)
                continue;
            seen.push(o);
            if (pred(o))
                out.push(o);
            const kids = o.children;
            if (kids !== undefined)
                for (var i = 0; i < kids.length; i++)
                    stack.push(kids[i]);
            const res = o.resources;
            if (res !== undefined)
                for (var j = 0; j < res.length; j++)
                    stack.push(res[j]);
            if (o.contentItem !== undefined && o.contentItem !== null)
                stack.push(o.contentItem);
        }
        return out;
    }

    // The four location buttons each carry a readonly `edge` int and a clicked
    // signal; nothing else on the page exposes `edge`.
    function locationButtons(page) {
        return collect(page, function(o) {
            return typeof o.edge === "number" && typeof o.clicked === "function";
        });
    }

    // LatteComponents.CheckBox: clicked signal + the value/tooltip aliases.
    function checkBoxes(page) {
        return collect(page, function(o) {
            return typeof o.clicked === "function"
                && typeof o.tooltip !== "undefined"
                && typeof o.value !== "undefined"
                && typeof o.checked !== "undefined";
        });
    }

    function findByText(items, txt) {
        for (var i = 0; i < items.length; i++)
            if (("" + items[i].text).indexOf(txt) !== -1)
                return items[i];
        return null;
    }

    // ---- Tests ----------------------------------------------------------------

    // Each location button's onClicked calls setNextLocation with its edge when
    // viewConfig.isReady and plasmoid.location !== edge. location starts at
    // Floating(0), so all four edges differ and every click records.
    function test_location_buttons_setNextLocation() {
        const page = make();
        const btns = locationButtons(page);
        compare(btns.length, 4, "expected the 4 location buttons");

        root.locationCalls = [];
        const edgesSeen = [];
        for (var i = 0; i < btns.length; i++) {
            const before = root.locationCalls.length;
            btns[i].clicked();
            compare(root.locationCalls.length, before + 1,
                    "button edge=" + btns[i].edge + " did not call setNextLocation");
            const call = root.locationCalls[root.locationCalls.length - 1];
            compare(call.location, btns[i].edge);
            compare(call.alignment, LatteCore.Types.NoneAlignment);
            compare(call.group, latteView.screensGroup);
            edgesSeen.push(btns[i].edge);
        }
        // The four real edges, all distinct.
        verify(edgesSeen.indexOf(PlasmaCore.Types.BottomEdge) !== -1);
        verify(edgesSeen.indexOf(PlasmaCore.Types.LeftEdge) !== -1);
        verify(edgesSeen.indexOf(PlasmaCore.Types.TopEdge) !== -1);
        verify(edgesSeen.indexOf(PlasmaCore.Types.RightEdge) !== -1);
        page.destroy();
    }

    // The onClicked guard: when plasmoid.location already equals an edge, that
    // button must NOT call setNextLocation. Proves the handler's branch, not just
    // its entry.
    function test_location_button_skips_current_edge() {
        plasmoid.location = PlasmaCore.Types.BottomEdge;
        const page = make();
        const btns = locationButtons(page);

        root.locationCalls = [];
        var bottomBtn = null;
        for (var i = 0; i < btns.length; i++)
            if (btns[i].edge === PlasmaCore.Types.BottomEdge)
                bottomBtn = btns[i];
        verify(bottomBtn, "bottom-edge button not found");

        bottomBtn.clicked();
        compare(root.locationCalls.length, 0,
                "clicking the already-current edge should be a no-op");

        // A non-current edge still fires.
        var leftBtn = null;
        for (var j = 0; j < btns.length; j++)
            if (btns[j].edge === PlasmaCore.Types.LeftEdge)
                leftBtn = btns[j];
        leftBtn.clicked();
        compare(root.locationCalls.length, 1);
        compare(root.locationCalls[0].location, PlasmaCore.Types.LeftEdge);

        plasmoid.location = 0; // restore
        page.destroy();
    }

    // updateScreens() rebuilds screensModel (the three group rows + each real
    // screen) and findScreen() locates a named screen in it. With a non-primary
    // view whose current screen isn't in any group, updateScreens appends that
    // screen and findScreen returns its row, which becomes screenCmb.currentIndex.
    function test_updateScreens_rebuild_and_findScreen() {
        latteView.onPrimary = false;
        latteView.screensGroup = 999; // none of Single/All/AllSecondary
        positioner.currentScreenName = "HDMI-1";
        universalSettings.screens = [{name: "DP-1"}, {name: "HDMI-1"}];

        const page = make();

        // Find the screen combo: a ComboBox whose model is the screensModel
        // ListModel (has a numeric count), distinct from the action combos whose
        // model is a plain JS array.
        const combo = collect(page, function(o) {
            return typeof o.currentIndex === "number"
                && typeof o.findScreen === "function";
        })[0];
        verify(combo, "screen combo not found");

        // Model = 3 group rows + 2 real screens. The current screen ("HDMI-1") is
        // already in the screens list, so updateScreens does NOT add a duplicate
        // extra row.
        compare(combo.count, 5);
        compare(combo.findScreen("HDMI-1"), 4); // group rows 0..2, DP-1 at 3, HDMI-1 at 4
        compare(combo.findScreen("DP-1"), 3);
        compare(combo.findScreen("does-not-exist"), 0); // findScreen's fallback
        // screensGroup 999 takes the else branch -> currentIndex = findScreen(current).
        compare(combo.currentIndex, 4);

        // A fresh showSignal() rebuild reflects an updated screens list. With
        // only DP-1 present and the current screen HDMI-1 now missing on a
        // non-primary view, updateScreens appends HDMI-1 as an extra row:
        // 3 group rows + HDMI-1 (extra) + DP-1 = 5.
        universalSettings.screens = [{name: "DP-1"}];
        viewConfig.showSignal();
        compare(combo.count, 5);
        // The appended extra row sits before the real screens, so findScreen now
        // locates the missing current screen at row 3.
        compare(combo.findScreen("HDMI-1"), 3);

        latteView.onPrimary = true;
        latteView.screensGroup = 0;
        positioner.currentScreenName = "DP-1";
        universalSettings.screens = [];
        page.destroy();
    }

    // screenCmb.onActivated maps the chosen row to a setNextLocation call:
    // 0 -> SingleScreenGroup/{primary-screen}, 1 -> AllScreensGroup,
    // 2 -> AllSecondaryScreensGroup, >2 -> SingleScreenGroup with the screen name.
    function test_screen_combo_activated_branches() {
        latteView.onPrimary = true;
        latteView.screensGroup = 0;
        positioner.currentScreenName = "DP-1";
        universalSettings.screens = [{name: "DP-1"}, {name: "HDMI-1"}];

        const page = make();
        const combo = collect(page, function(o) {
            return typeof o.currentIndex === "number"
                && typeof o.findScreen === "function";
        })[0];
        verify(combo, "screen combo not found");
        compare(combo.count, 5);

        root.locationCalls = [];

        combo.activated(0);
        compare(root.locationCalls.length, 1);
        compare(root.locationCalls[0].group, LatteCore.Types.SingleScreenGroup);
        compare(root.locationCalls[0].screen, "{primary-screen}");

        combo.activated(1);
        compare(root.locationCalls.length, 2);
        compare(root.locationCalls[1].group, LatteCore.Types.AllScreensGroup);

        combo.activated(2);
        compare(root.locationCalls.length, 3);
        compare(root.locationCalls[2].group, LatteCore.Types.AllSecondaryScreensGroup);

        // Row 4 = "HDMI-1": index>2 and (differs from findScreen(current) or
        // onPrimary) -> explicit single-screen with textAt(index).
        combo.activated(4);
        compare(root.locationCalls.length, 4);
        compare(root.locationCalls[3].group, LatteCore.Types.SingleScreenGroup);
        compare(root.locationCalls[3].screen, "HDMI-1");

        latteView.onPrimary = true;
        universalSettings.screens = [];
        page.destroy();
    }

    // activeWindowFilterCmb.onCurrentIndexChanged writes the matching enum into
    // plasmoid.configuration.activeWindowFilter (a 2-entry array model).
    function test_active_window_filter_combo_writes_config() {
        configuration.activeWindowFilter = LatteContainment.Types.ActiveInCurrentScreen;
        const page = make();

        const combo = collect(page, function(o) {
            return typeof o.currentIndex === "number"
                && typeof o.findScreen === "undefined"
                && o.model && o.model.length === 2;
        })[0];
        verify(combo, "active-window-filter combo not found");

        combo.currentIndex = LatteContainment.Types.ActiveFromAllScreens;
        compare(configuration.activeWindowFilter,
                LatteContainment.Types.ActiveFromAllScreens);

        combo.currentIndex = LatteContainment.Types.ActiveInCurrentScreen;
        compare(configuration.activeWindowFilter,
                LatteContainment.Types.ActiveInCurrentScreen);
        page.destroy();
    }

    // scrollAction.onCurrentIndexChanged writes the matching enum into
    // plasmoid.configuration.scrollAction (a 5-entry array model).
    function test_scroll_action_combo_writes_config() {
        configuration.scrollAction = LatteContainment.Types.ScrollNone;
        const page = make();

        const combo = collect(page, function(o) {
            return typeof o.currentIndex === "number"
                && typeof o.findScreen === "undefined"
                && o.model && o.model.length === 5;
        })[0];
        verify(combo, "scroll-action combo not found");

        combo.currentIndex = LatteContainment.Types.ScrollActivities;
        compare(configuration.scrollAction, LatteContainment.Types.ScrollActivities);

        combo.currentIndex = LatteContainment.Types.ScrollToggleMinimized;
        compare(configuration.scrollAction,
                LatteContainment.Types.ScrollToggleMinimized);
        page.destroy();
    }

    // The two delay TextFields' onValueChanged write timerShow/timerHide. value
    // is derived from text, so set text imperatively to a numeric string and the
    // handler runs.
    function test_delay_textfields_write_timers() {
        visibilityObj.mode = LatteCore.Types.DodgeActive; // keep the Delay column enabled
        visibilityObj.timerShow = 100;
        visibilityObj.timerHide = 200;
        const page = make();

        // LatteComponents.TextField: has a numeric `value`, a `maxValue`, and is a
        // TextInput-backed field (settable `text`); not a checkbox/combo.
        const fields = collect(page, function(o) {
            return typeof o.value === "number"
                && typeof o.maxValue !== "undefined"
                && typeof o.text === "string"
                && typeof o.clicked === "undefined"
                && typeof o.currentIndex === "undefined";
        });
        verify(fields.length >= 2, "expected the show/hide delay fields, got " + fields.length);

        // hideContainer's field carries maxValue 5000; the show field keeps the
        // default 3000. Use that to tell them apart.
        var showField = null, hideField = null;
        for (var i = 0; i < fields.length; i++) {
            if (fields[i].maxValue === 5000)
                hideField = fields[i];
            else
                showField = fields[i];
        }
        verify(showField, "show delay field not found");
        verify(hideField, "hide delay field not found");

        showField.text = "750";
        compare(visibilityObj.timerShow, 750);

        hideField.text = "1250";
        compare(visibilityObj.timerHide, 1250);
        page.destroy();
    }

    // The "Drag Active Window" / "Close Active Window" action buttons write their
    // config bool from `checked` on click. Set checked, then emit clicked().
    function test_action_buttons_write_config() {
        configuration.dragActiveWindowEnabled = false;
        configuration.closeActiveWindowEnabled = false;
        const page = make();

        // Checkable Buttons with a readonly dragActiveWindowEnabled/
        // closeActiveWindowEnabled mirror property.
        const dragBtn = collect(page, function(o) {
            return typeof o.clicked === "function"
                && typeof o.dragActiveWindowEnabled !== "undefined";
        })[0];
        const closeBtn = collect(page, function(o) {
            return typeof o.clicked === "function"
                && typeof o.closeActiveWindowEnabled !== "undefined";
        })[0];
        verify(dragBtn, "drag-active button not found");
        verify(closeBtn, "close-active button not found");

        dragBtn.checked = true;
        dragBtn.clicked();
        compare(configuration.dragActiveWindowEnabled, true);

        closeBtn.checked = true;
        closeBtn.clicked();
        compare(configuration.closeActiveWindowEnabled, true);
        page.destroy();
    }

    // The Items/Floating/Environment checkboxes each flip a config bool (or a
    // latteView bool) on clicked(). Drive each by its label text and assert the
    // single flip, which proves the specific onClicked handler ran.
    function test_checkbox_onclicked_flips() {
        configuration.titleTooltips = false;
        configuration.mouseWheelActions = false;
        configuration.autoSizeEnabled = false;
        configuration.floatingInternalGapIsForced = false;
        configuration.hideFloatingGapForMaximized = false;
        configuration.floatingGapHidingWaitsMouse = false;
        configuration.floatingGapIsMirrored = false;
        visibilityObj.enableKWinEdges = false;
        visibilityObj.raiseOnDesktop = false;
        visibilityObj.raiseOnActivity = false;
        latteView.byPassWM = false;
        latteView.isPreferredForShortcuts = false;
        layoutObj.preferredForShortcutsTouched = false;
        // Keep enabling guards satisfied: DodgeActive keeps Environment enabled,
        // and zoomLevel 0 / hideFloatingGapForMaximized control floating rows.
        visibilityObj.mode = LatteCore.Types.DodgeActive;
        configuration.zoomLevel = 0;

        const page = make();
        const boxes = checkBoxes(page);

        // titleTooltips: onClicked flips it.
        const titleChk = findByText(boxes, "Thin title tooltips");
        verify(titleChk, "title-tooltips checkbox not found");
        titleChk.clicked();
        compare(configuration.titleTooltips, true);

        const wheelChk = findByText(boxes, "Expand popup through mouse wheel");
        verify(wheelChk, "mouse-wheel checkbox not found");
        wheelChk.clicked();
        compare(configuration.mouseWheelActions, true);

        const autoChk = findByText(boxes, "Adjust size automatically");
        verify(autoChk, "auto-size checkbox not found");
        autoChk.clicked();
        compare(configuration.autoSizeEnabled, true);

        // Global-shortcuts checkbox writes latteView.isPreferredForShortcuts from
        // `checked` and marks the layout touched.
        const shortcutChk = findByText(boxes, "Activate based on position");
        verify(shortcutChk, "shortcuts checkbox not found");
        shortcutChk.checked = true;
        shortcutChk.clicked();
        compare(latteView.isPreferredForShortcuts, true);
        compare(layoutObj.preferredForShortcutsTouched, true);

        // Floating section: each flips its own config bool.
        const gapForced = findByText(boxes, "Always use floating gap");
        verify(gapForced, "floating-gap-forced checkbox not found");
        gapForced.clicked();
        compare(configuration.floatingInternalGapIsForced, true);

        const hideGap = findByText(boxes, "Hide floating gap for maximized");
        verify(hideGap, "hide-floating-gap checkbox not found");
        hideGap.clicked();
        compare(configuration.hideFloatingGapForMaximized, true);

        const delayGap = findByText(boxes, "Delay floating gap hiding");
        verify(delayGap, "delay-floating-gap checkbox not found");
        delayGap.clicked();
        compare(configuration.floatingGapHidingWaitsMouse, true);

        const mirrorGap = findByText(boxes, "Mirror floating gap");
        verify(mirrorGap, "mirror-floating-gap checkbox not found");
        mirrorGap.clicked();
        compare(configuration.floatingGapIsMirrored, true);

        // Environment section.
        const kwinEdge = findByText(boxes, "Activate KWin edge");
        verify(kwinEdge, "kwin-edge checkbox not found");
        kwinEdge.clicked();
        compare(visibilityObj.enableKWinEdges, true);

        const bypass = findByText(boxes, "Can be above fullscreen");
        verify(bypass, "bypass-wm checkbox not found");
        bypass.clicked();
        compare(latteView.byPassWM, true);

        const raiseDesktop = findByText(boxes, "Raise on desktop change");
        verify(raiseDesktop, "raise-on-desktop checkbox not found");
        raiseDesktop.clicked();
        compare(visibilityObj.raiseOnDesktop, true);

        const raiseActivity = findByText(boxes, "Raise on activity change");
        verify(raiseActivity, "raise-on-activity checkbox not found");
        raiseActivity.clicked();
        compare(visibilityObj.raiseOnActivity, true);

        page.destroy();
    }
}
