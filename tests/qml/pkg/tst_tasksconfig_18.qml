// Coverage for the dock's Tasks configuration page. The page is a
// PlasmaComponents.Page that reads context globals supplied by the real config
// view (tasks, dialog, latteView, plasmoid) plus i18n/i18nc. None of those exist
// in a bare qmltestrunner, so we name the TestCase `id: root` and declare every
// global the page reads as a property / lowercase-id'd QtObject on it. QML
// resolves the page's unqualified lookups against its creation context, so they
// bind to these mocks.
//
// What we honestly cover here:
//   - disableAllWindowsFunctionality, the page's one declared binding.
//   - onIsCurrentPageChanged@29, driven through both branches (count<=1 no-op,
//     count>1 requests the visual indicator).
//   - the 20 checkbox onClicked@* handlers, each asserted to flip exactly one
//     configuration bool.
//   - the scrolling HeaderSwitch onPressed@447 toggle.
//
// What we deliberately do NOT claim (live-only -- see tests/coverage/live-only.md):
//   - the launcher-group button onPressedChanged handlers: their only observable
//     effect (writing launchersGroup) runs inside `if (pressed)`, and a QQC2
//     Button's `pressed` can't be forced true offscreen (no real pointer grab).
//   - the combo onCurrentIndexChanged handlers: their config write-back only fires
//     through real popup interaction; assigning currentIndex imperatively neither
//     re-runs the handler nor writes back here.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "TasksConfig"
    when: windowShown
    visible: true
    width: 500
    height: 800

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/configuration/pages/TasksConfig.qml")

    // Context globals the page reads. A fresh cfg per test keeps writes isolated.
    property var cfg
    property var tasks: QtObject { property int id: 7; property var configuration: root.cfg }
    property var plasmoid: QtObject { property var configuration: root.cfg }

    QtObject {
        id: dialog
        property int appliedWidth: 480
        property int optionsWidth: 420
        property int subGroupSpacing: 8
        property bool advancedLevel: true
        property bool basicLevel: false
        property bool panelIsVertical: false
        property var currentPage: null
    }

    // latteView.extendedInterface.appletRequestedVisualIndicator bumps this; the
    // page's count drives whether onIsCurrentPageChanged takes its inner branch.
    property int indicatorRequests: 0
    property int modelCount: 0

    QtObject {
        id: latteView
        property bool isPreferredForShortcuts: false
        function isHighestPriorityView() { return true; }
        property QtObject layout: QtObject { property bool preferredForShortcutsTouched: false }
        property QtObject indicator: QtObject {
            property QtObject info: QtObject {
                property bool providesTaskLauncherAnimation: false
                property bool providesInAttentionAnimation: false
                property bool providesGroupedWindowAddedAnimation: false
                property bool providesGroupedWindowRemovedAnimation: false
            }
        }
        property QtObject extendedInterface: QtObject {
            property QtObject latteTasksModel: QtObject { property int count: root.modelCount }
            function appletRequestedVisualIndicator(id) { root.indicatorRequests++; }
        }
    }

    function i18n() { return arguments.length > 0 ? "" + arguments[0] : ""; }
    function i18nc() { return arguments.length > 1 ? "" + arguments[1] : ""; }

    // Every bool the checkbox value/clicked bindings touch, in no special order.
    readonly property var boolKeys: [
        "hideAllTasks", "showInfoBadge", "showProgressBadge", "showAudioBadge",
        "infoBadgeProminentColorEnabled", "audioBadgeActionsEnabled",
        "isPreferredForDroppedLaunchers", "showWindowActions", "previewWindowAsPopup",
        "isPreferredForPositionShortcuts", "showOnlyCurrentScreen", "showOnlyCurrentDesktop",
        "showOnlyCurrentActivity", "showWindowsOnlyFromLaunchers", "groupTasksByDefault",
        "animationLauncherBouncing", "animationWindowInAttention", "animationNewWindowSliding",
        "animationWindowAddedInGroup", "animationWindowRemovedFromGroup",
        "scrollTasksEnabled", "autoScrollTasksEnabled"
    ]

    // A fresh tasks.configuration per test. Covers every property the page's
    // CheckBox/ComboBox/Button bindings read.
    function makeConfig() {
        return Qt.createQmlObject('import QtQuick; QtObject {\n'
            + ' property bool hideAllTasks: false;\n'
            + ' property bool showInfoBadge: true;\n'
            + ' property bool showProgressBadge: true;\n'
            + ' property bool showAudioBadge: true;\n'
            + ' property bool infoBadgeProminentColorEnabled: false;\n'
            + ' property bool audioBadgeActionsEnabled: false;\n'
            + ' property bool isPreferredForDroppedLaunchers: false;\n'
            + ' property bool showWindowActions: true;\n'
            + ' property bool previewWindowAsPopup: false;\n'
            + ' property bool isPreferredForPositionShortcuts: false;\n'
            + ' property bool showOnlyCurrentScreen: false;\n'
            + ' property bool showOnlyCurrentDesktop: false;\n'
            + ' property bool showOnlyCurrentActivity: false;\n'
            + ' property bool showWindowsOnlyFromLaunchers: false;\n'
            + ' property bool groupTasksByDefault: true;\n'
            + ' property bool animationLauncherBouncing: true;\n'
            + ' property bool animationWindowInAttention: true;\n'
            + ' property bool animationNewWindowSliding: true;\n'
            + ' property bool animationWindowAddedInGroup: true;\n'
            + ' property bool animationWindowRemovedFromGroup: true;\n'
            + ' property int launchersGroup: 0;\n'
            + ' property bool scrollTasksEnabled: true;\n'
            + ' property int manualScrollTasksType: 0;\n'
            + ' property bool autoScrollTasksEnabled: false;\n'
            + ' property int leftClickAction: 0;\n'
            + ' property int middleClickAction: 0;\n'
            + ' property int hoverAction: 0;\n'
            + ' property int taskScrollAction: 0;\n'
            + ' property int modifier: 0;\n'
            + ' property int modifierClick: 0;\n'
            + ' property int modifierClickAction: 0;\n'
            + ' property int id: 7;\n'
            + ' property bool animationsEnabled: true;\n'
            + '}', root, "tasksConfigMock");
    }

    function loadPage() {
        const c = Qt.createComponent(targetUrl);
        tryVerify(function() { return c.status === Component.Ready || c.status === Component.Error; }, 6000);
        verify(c.status === Component.Ready, c.errorString());
        const page = createTemporaryObject(c, root, {});
        verify(page, "no page item");
        return page;
    }

    // Depth-first collect of every object matching a predicate, descending
    // children, resources and contentChildren (config sections nest several
    // layouts deep, non-visual handlers live in resources). seen[] dedupes so an
    // object reachable by more than one edge is visited once -- else its handler
    // fires twice and a toggle nets to no change.
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

    // LatteComponents.CheckBox is a PlasmaComponents.CheckBox carrying the
    // page-specific "value"/"tooltip" aliases and a "clicked" signal.
    function isCheckBox(o) {
        return typeof o.clicked === "function"
            && typeof o.tooltip !== "undefined"
            && typeof o.value !== "undefined"
            && typeof o.checked !== "undefined";
    }

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

    // disableAllWindowsFunctionality is the page's one declared property: a live
    // binding onto tasks.configuration.hideAllTasks. Prove it tracks both ways.
    function test_disable_all_windows_binding() {
        cfg = makeConfig();
        const page = loadPage();

        compare(page.disableAllWindowsFunctionality, false);
        cfg.hideAllTasks = true;
        compare(page.disableAllWindowsFunctionality, true);
        cfg.hideAllTasks = false;
        compare(page.disableAllWindowsFunctionality, false);
    }

    // Drive onIsCurrentPageChanged@29 through both branches. count<=1 takes the
    // no-op path; count>1 calls appletRequestedVisualIndicator(tasks.id), which
    // our mock counts.
    function test_iscurrentpage_indicator_request() {
        cfg = makeConfig();
        root.indicatorRequests = 0;
        root.modelCount = 0;
        const page = loadPage();

        // Becoming current with count<=1: handler runs, inner branch skipped.
        dialog.currentPage = page;
        compare(page.isCurrentPage, true);
        compare(root.indicatorRequests, 0);

        // Leave, then return with count>1 so the indicator fires exactly once.
        dialog.currentPage = null;
        compare(page.isCurrentPage, false);
        root.modelCount = 3;
        dialog.currentPage = page;
        compare(page.isCurrentPage, true);
        compare(root.indicatorRequests, 1);
    }

    // Emit clicked() on every checkbox so each inline onClicked toggle handler
    // runs. The boxes have no ids, so collect them from the tree. Each handler
    // flips exactly one tasks.configuration bool; assert that per box so all 20
    // handlers are covered with an observable effect, not just one.
    function test_checkbox_onclicked_handlers() {
        cfg = makeConfig();
        const page = loadPage();

        const boxes = collect(page, isCheckBox, [], []);
        compare(boxes.length, 20, "expected the 20 badge/interaction/filter/animation checkboxes");

        for (var i = 0; i < boxes.length; i++) {
            const before = snapshotBools();
            boxes[i].clicked();
            const ch = changedBoolKeys(before);
            compare(ch.length, 1, "checkbox " + i + " onClicked flipped " + ch.length + " bools: " + ch.join(","));
        }
    }

    // The scrolling HeaderSwitch exposes a "pressed" signal whose handler toggles
    // scrollTasksEnabled (onPressed@447). Emit it directly and assert the toggle.
    function test_header_switch_pressed_handler() {
        cfg = makeConfig();
        const page = loadPage();

        // HeaderSwitch: a "checked" + "tooltip" like a checkbox but exposes a
        // "pressed" *signal* (callable) and no "value" alias / combo markers.
        const switches = collect(page, function(o) {
            return typeof o.pressed === "function"
                && typeof o.checked !== "undefined"
                && typeof o.value === "undefined"
                && typeof o.currentIndex === "undefined";
        }, [], []);
        verify(switches.length >= 1, "scrolling HeaderSwitch not found");

        const before = cfg.scrollTasksEnabled;
        switches[0].pressed();
        compare(cfg.scrollTasksEnabled, !before);
        switches[0].pressed();
        compare(cfg.scrollTasksEnabled, before);
    }
}
