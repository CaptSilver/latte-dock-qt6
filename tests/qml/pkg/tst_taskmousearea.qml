// Drives the plasmoid TaskMouseArea (the per-task MouseArea living inside
// TaskItem) through its instrumented units: the Connections press/release
// forwarders, the onEntered/onExited/onContainsMouseChanged hover handlers,
// and the two internal Timers (_hoveredTimer previews delay, _resistanerTimer
// drag resistance). The staged (instrumented) copy is loaded so Cov.tick fires.
//
// TaskMouseArea reads everything unqualified from the TaskItem/main context;
// this TestCase is named id: root and declares each name the exercised code
// touches, shaped like the real object:
//   taskItem          -> visible/resistanceDelay/containsMouse/isWindow/
//                        isLauncher/inBlockingAnimation/isDragged +
//                        mousePressed/mouseReleased/showPreviewWindow +
//                        abilities.myView / abilities.debug
//   windowsPreviewDlg -> .visible/.activeItem/.hide(v)
//   scrollableList    -> .autoScrollFor(item, v)
//   backend           -> .windowViewAvailable/
//                        .cancelHighlightWindows()
//   root.*            -> the handler flag set (showPreviews, highlightWindows,
//                        autoScrollTasksEnabled, disableAllWindowsFunctionality,
//                        taskInAnimation, ...) plus the press bookkeeping the
//                        uninstrumented big onPressed/onReleased handlers write
//                        (lastButtonClicked, pressX/pressY) — those two handlers
//                        run on every real press/release and must not throw.
// Hover/press/release are real synthetic mouse events: the area is parented to
// a small inner Item (arena) so the cursor can genuinely enter AND leave it,
// which is what flips containsMouse and fires entered/exited.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "TaskMouseArea"
    when: windowShown
    visible: true
    width: 300
    height: 300

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/task/TaskMouseArea.qml")

    // hoverEnabled binding inputs — all must read "hoverable" or no hover
    // event ever reaches the handlers.
    property bool inAnimation: false
    property bool isStartup: false
    property bool taskInAnimation: false
    property bool inBouncingAnimation: false
    property bool isSeparator: false

    // onEntered / hoveredTimer inputs.
    property bool isLauncher: false
    property bool isAbleToShowPreview: true
    property bool showPreviewsIsBlockedFromReleaseEvent: false
    property bool showPreviews: false
    property bool highlightWindows: false
    property bool disableAllWindowsFunctionality: false
    property bool autoScrollTasksEnabled: false

    // onContainsMouseChanged reads the unqualified isWindow (the TaskItem one),
    // distinct from taskItem.isWindow which onEntered/hoveredTimer read.
    property bool isWindow: false

    // Ambient state of the uninstrumented onPressed/onReleased/onPositionChanged
    // bodies that run as a side effect of real press/release/move events. They
    // write lastButtonClicked/pressX/pressY and short-circuit on the rest; every
    // name must resolve or the handler throws mid-event.
    property int leftClickAction: 0
    property bool inBlockingAnimation: false
    property bool inAttentionBuiltinAnimation: false
    property bool isGroupParent: false
    property int lastButtonClicked: -1
    property real pressX: -1
    property real pressY: -1
    property bool isDragged: false
    property bool disableRestoreZoom: false

    // model.WinIdList feeds root.windowsHovered from three handlers.
    property var model: ({ WinIdList: [11, 22], IsLauncher: false })

    // root functions the handlers call, recording for assertions.
    function modifierAccepted(mouse) { return false; }
    property int publishGeometriesCalls: 0
    function slotPublishGeometries() { publishGeometriesCalls++; }
    property int activateTaskCalls: 0
    function activateTask() { activateTaskCalls++; }
    property int windowsHoveredCalls: 0
    property var windowsHoveredArgs: null
    function windowsHovered(ids, hovered) { windowsHoveredCalls++; windowsHoveredArgs = [ids, hovered]; }
    property real lastHidePreview: -1
    function hidePreview(v) { lastHidePreview = v; }

    QtObject {
        id: taskItem
        property bool visible: true
        property int resistanceDelay: 60000   // resistaner timer must never fire on its own mid-test
        property bool inBlockingAnimation: false
        property bool isDragged: false
        property bool isLauncher: false
        property bool isWindow: false
        property bool containsMouse: false
        property bool showPreviewsIsBlockedFromReleaseEvent: false
        property bool isAbleToShowPreview: true
        property var lastPressed: null
        property var lastReleased: null
        property int showPreviewWindowCalls: 0
        function mousePressed(x, y, b) { lastPressed = [x, y, b]; }
        function mouseReleased(x, y, b) { lastReleased = [x, y, b]; }
        function showPreviewWindow() { showPreviewWindowCalls++; }
        // onPositionChanged gates on myView; the resistaner timer reads debug.
        property QtObject abilities: QtObject {
            property QtObject myView: QtObject {
                property bool isReady: false
                property bool isShownFully: true
            }
            property QtObject debug: QtObject {
                property bool timersEnabled: false
            }
        }
    }

    QtObject {
        id: windowsPreviewDlg
        property bool visible: false
        property var activeItem: null
        property int hideCalls: 0
        property real lastHide: -1
        function hide(v) { hideCalls++; lastHide = v; }
    }

    QtObject {
        id: scrollableList
        property int autoScrollCalls: 0
        property var lastAutoScrollItem: null
        property var lastAutoScrollValue: null
        function autoScrollFor(item, v) { autoScrollCalls++; lastAutoScrollItem = item; lastAutoScrollValue = v; }
    }

    QtObject {
        id: backend
        property bool windowViewAvailable: false
        property int cancelHighlightCalls: 0
        function cancelHighlightWindows() { cancelHighlightCalls++; }
    }

    // The MouseArea anchors-fills its parent; giving it a small arena leaves
    // room in the window for the cursor to sit outside it (park spot 280,280).
    Item {
        id: arena
        x: 0
        y: 0
        width: 100
        height: 100
    }

    function init() {
        // shared mock state persists across tests; reset to the quiet defaults
        inAnimation = false;
        taskInAnimation = false;
        isLauncher = false;
        isAbleToShowPreview = true;
        showPreviewsIsBlockedFromReleaseEvent = false;
        showPreviews = false;
        highlightWindows = false;
        disableAllWindowsFunctionality = false;
        autoScrollTasksEnabled = false;
        isWindow = false;
        lastButtonClicked = -1;
        pressX = -1;
        pressY = -1;
        publishGeometriesCalls = 0;
        activateTaskCalls = 0;
        windowsHoveredCalls = 0;
        windowsHoveredArgs = null;
        lastHidePreview = -1;

        taskItem.inBlockingAnimation = false;
        taskItem.isDragged = false;
        taskItem.isLauncher = false;
        taskItem.isWindow = false;
        taskItem.containsMouse = false;
        taskItem.showPreviewsIsBlockedFromReleaseEvent = false;
        taskItem.isAbleToShowPreview = true;
        taskItem.lastPressed = null;
        taskItem.lastReleased = null;
        taskItem.showPreviewWindowCalls = 0;

        windowsPreviewDlg.visible = false;
        windowsPreviewDlg.activeItem = null;
        windowsPreviewDlg.hideCalls = 0;
        windowsPreviewDlg.lastHide = -1;

        scrollableList.autoScrollCalls = 0;
        scrollableList.lastAutoScrollItem = null;
        scrollableList.lastAutoScrollValue = null;

        backend.cancelHighlightCalls = 0;
    }

    function make() {
        // park the cursor outside the arena first so a freshly created area
        // never starts hovered from a previous test's cursor position
        mouseMove(root, 280, 280);
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, arena, {});
        verify(obj, "instantiate failed");
        verify(obj.hoverEnabled, "hoverEnabled must resolve true or hover never reaches the handlers");
        return obj;
    }

    // _resistanerTimer has no alias; it is the non-visual Timer in resources
    // that is not the aliased hoveredTimer (the Connections carries no restart).
    function resistanerTimerOf(obj) {
        const res = obj.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i] !== obj.hoveredTimer
                    && typeof res[i].restart === "function" && res[i].interval !== undefined) {
                return res[i];
            }
        }
        return null;
    }

    // Connections onPressed/onReleased forward the real mouse event's x/y/button
    // to taskItem.mousePressed/mouseReleased. A synthetic press+release carries a
    // genuine QQuickMouseEvent, which is the only way these handlers can run.
    function test_pressAndReleaseForwardToTaskItem() {
        const obj = make();
        // launcher task: the big onReleased body routes to the plain
        // activateTask() branch instead of the window-view machinery
        taskItem.isLauncher = true;
        root.isLauncher = true;   // keeps onEntered quiet on the hover that precedes the press

        mousePress(root, 40, 40, Qt.LeftButton);
        verify(taskItem.lastPressed !== null, "mousePressed was not forwarded");
        compare(taskItem.lastPressed[0], 40);
        compare(taskItem.lastPressed[1], 40);
        compare(taskItem.lastPressed[2], Qt.LeftButton);
        // the uninstrumented onPressed body ran alongside: press bookkeeping
        compare(obj.pressed, true);
        compare(root.lastButtonClicked, Qt.LeftButton);
        verify(root.publishGeometriesCalls >= 1);

        mouseRelease(root, 40, 40, Qt.LeftButton);
        verify(taskItem.lastReleased !== null, "mouseReleased was not forwarded");
        compare(taskItem.lastReleased[0], 40);
        compare(taskItem.lastReleased[1], 40);
        compare(taskItem.lastReleased[2], Qt.LeftButton);
        // the uninstrumented onReleased body: launcher -> activateTask, then cleanup
        compare(obj.pressed, false);
        compare(root.activateTaskCalls, 1);
        compare(backend.cancelHighlightCalls, 1);
    }

    // onEntered launcher branch: an open preview dialog is hidden with the "1"
    // debug tag; the release-event block flag resets; autoscroll is asked to
    // bring the task into view.
    function test_entered_launcherHidesOpenPreviews() {
        const obj = make();
        root.isLauncher = true;
        windowsPreviewDlg.visible = true;
        taskItem.showPreviewsIsBlockedFromReleaseEvent = true;
        root.autoScrollTasksEnabled = true;

        mouseMove(root, 50, 50);
        verify(obj.containsMouse, "hover did not enter the area");
        compare(windowsPreviewDlg.hideCalls, 1);
        compare(windowsPreviewDlg.lastHide, 1);
        compare(taskItem.showPreviewsIsBlockedFromReleaseEvent, false);
        compare(scrollableList.autoScrollCalls, 1);
        verify(scrollableList.lastAutoScrollItem === taskItem);
        compare(scrollableList.lastAutoScrollValue, false);
    }

    // onEntered previews branch, dialog hidden: the hovered timer is armed so
    // the preview appears only after the configured delay.
    function test_entered_armsHoveredTimer() {
        const obj = make();
        root.showPreviews = true;

        verify(!obj.hoveredTimer.running);
        mouseMove(root, 50, 50);
        verify(obj.containsMouse, "hover did not enter the area");
        verify(obj.hoveredTimer.running, "hovered timer was not armed");
        obj.hoveredTimer.stop();
    }

    // onEntered previews branch, dialog already visible: the preview updates
    // immediately and hovered windows get highlighted.
    function test_entered_refreshesVisiblePreview() {
        const obj = make();
        root.showPreviews = true;
        root.highlightWindows = true;
        windowsPreviewDlg.visible = true;
        taskItem.isWindow = true;
        taskItem.containsMouse = true;   // forwarded verbatim to windowsHovered

        mouseMove(root, 50, 50);
        compare(taskItem.showPreviewWindowCalls, 1);
        compare(root.windowsHoveredCalls, 1);
        compare(root.windowsHoveredArgs[0].length, 2);
        compare(root.windowsHoveredArgs[0][0], 11);
        compare(root.windowsHoveredArgs[1], true);
        // no hovered-timer arming when the dialog is already up
        verify(!obj.hoveredTimer.running);
    }

    // onExited restores the preview ability and (with previews on) schedules a
    // hide; onContainsMouseChanged on the same leave drops the pressed flag and
    // un-highlights the hovered windows.
    function test_exited_restoresPreviewStateAndDropsHover() {
        const obj = make();
        // enter with quiet flags: launcher + hidden dialog takes no preview action
        root.isLauncher = true;
        mouseMove(root, 50, 50);
        verify(obj.containsMouse, "hover did not enter the area");

        // seed what the two leave handlers must undo/emit
        taskItem.isAbleToShowPreview = false;
        root.showPreviews = true;
        root.isWindow = true;
        root.highlightWindows = true;
        obj.pressed = true;
        root.windowsHoveredCalls = 0;
        root.windowsHoveredArgs = null;

        mouseMove(root, 280, 280);
        verify(!obj.containsMouse, "hover did not leave the area");
        // onExited
        compare(taskItem.isAbleToShowPreview, true);
        compare(root.lastHidePreview, 17.5);
        // onContainsMouseChanged(false)
        compare(obj.pressed, false);
        compare(root.windowsHoveredCalls, 1);
        compare(root.windowsHoveredArgs[1], false);
    }

    // hoveredTimer onTriggered: guarded off while all-windows functionality is
    // disabled; enabled again it shows the preview for the still-hovered task
    // and highlights its windows. The interval binding to
    // Plasmoid.configuration.previewsDelay is dead headlessly (no live applet),
    // so writing the interval directly is safe.
    function test_hoveredTimer_showsPreviewsAndHighlights() {
        const obj = make();
        taskItem.containsMouse = true;
        root.showPreviews = true;
        taskItem.isWindow = true;
        root.highlightWindows = true;

        root.disableAllWindowsFunctionality = true;
        obj.hoveredTimer.interval = 1;
        obj.hoveredTimer.restart();
        wait(50);
        compare(taskItem.showPreviewWindowCalls, 0);
        compare(root.windowsHoveredCalls, 0);

        root.disableAllWindowsFunctionality = false;
        obj.hoveredTimer.restart();
        tryVerify(function() { return taskItem.showPreviewWindowCalls === 1; }, 2000,
                  "hovered timer did not show the preview");
        compare(root.windowsHoveredCalls, 1);
        compare(root.windowsHoveredArgs[1], true);
    }

    // resistaner timer onTriggered: marks the task dragged unless it sits in a
    // blocking animation.
    function test_resistanerTimer_marksTaskDragged() {
        const obj = make();
        const t = resistanerTimerOf(obj);
        verify(t, "resistaner timer not found in resources");

        // blocked: isDragged stays untouched
        taskItem.inBlockingAnimation = true;
        taskItem.isDragged = false;
        t.interval = 1;
        t.restart();
        wait(50);
        compare(taskItem.isDragged, false);

        taskItem.inBlockingAnimation = false;
        t.restart();
        tryVerify(function() { return taskItem.isDragged === true; }, 2000,
                  "resistaner timer did not mark the task dragged");
    }
}
