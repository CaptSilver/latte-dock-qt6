// Drives the plasmoid taskslayout MouseHandler (the tasks DropArea) through the
// units the coverage tool instrumented: the two Timer onTriggered handlers, the
// root-targeted onDragSourceChanged Connections handler, the DropArea's pure
// drop-classification helpers (isMovingTask / isDroppingSeparator /
// isDroppingOnlyLaunchers), clearDroppingFlags, and onHoveredItemChanged. The
// component is loaded from the staged (instrumented) package by file URL so the
// Cov.tick calls fire, and every claimed unit asserts an observable effect.
//
// Unqualified creation-context names the instrumented bodies read are declared
// on this TestCase (named id: root) shaped like the real objects:
//   root            -> the Connections target + root.dragSource / showPreviewForTasks
//   windowsPreviewDlg -> .activeItem / .visible / .hide(v)   (onHoveredItemChanged,
//                        activationTimer)
//   backend         -> .isApplication(url)                   (isDroppingOnlyLaunchers)
//   tasksModel      -> .requestActivate(idx)                 (activationTimer)
//   toolTipDelegate -> .currentItem                          (activationTimer)
// None is a catch-all; each carries only the members the target touches.
//
// onDragLeave ignores its event arg, so emitting the DropArea's dragLeave(null)
// signal exercises it honestly (its reset effects are asserted). The other three
// pointer handlers (onDragEnter/onDragMove/onDrop) dereference the
// DeclarativeDragDropEvent* and were not instrumented anyway; they are reported
// live-only, not gamed.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "MouseHandler"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/taskslayout/MouseHandler.qml")

    // The Connections inside MouseHandler binds target: root and reads
    // dragSource unqualified; both resolve here. showPreviewForTasks is called
    // qualified as root.showPreviewForTasks from the activationTimer group-parent
    // branch, so it lives on root too and records its argument.
    property var dragSource: null
    property var lastPreviewArg: undefined
    property int previewCalls: 0
    function showPreviewForTasks(item) { previewCalls++; lastPreviewArg = item; }

    // windowsPreviewDlg: onHoveredItemChanged hides it when a different item is
    // hovered; activationTimer reads .visible. Records the hide() argument.
    QtObject {
        id: windowsPreviewDlg
        property Item activeItem: null
        property bool visible: false
        property real lastHide: -1
        property int hideCalls: 0
        function hide(v) { hideCalls++; lastHide = v; }
    }

    // backend.isApplication is called by isDroppingOnlyLaunchers' every() over the
    // dropped urls. Records the urls it was asked about and answers from a flag.
    QtObject {
        id: backend
        property bool isAppAnswer: true
        property var asked: []
        function isApplication(url) { asked.push(url); return isAppAnswer; }
    }

    // tasksModel.requestActivate is the activationTimer's non-launcher branch
    // effect; toolTipDelegate.currentItem its tooltip-guard read.
    QtObject {
        id: tasksModel
        property var lastActivate: undefined
        property int activateCalls: 0
        function requestActivate(idx) { activateCalls++; lastActivate = idx; }
    }
    QtObject {
        id: toolTipDelegate
        property int currentItem: -1
    }

    // A task-delegate stand-in for dropHandler.hoveredItem: carries the model
    // role bag (.m) and the index accessors the activationTimer reads.
    Component {
        id: hoverItemComponent
        Item {
            property var m: ({})
            property int itemIndex: 0
            property int modelIdx: 0
            function modelIndex() { return modelIdx; }
        }
    }

    Component { id: plainItemComponent; Item {} }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // The DropArea (dropHandler) holds the pure classification helpers and the
    // flag state; it is the instrumented Item's visual child carrying isMovingTask.
    function dropHandlerOf(obj) {
        const kids = obj.children;
        for (var i = 0; i < kids.length; i++) {
            if (kids[i] && typeof kids[i].isMovingTask === "function")
                return kids[i];
        }
        return null;
    }

    function findTimerByInterval(obj, wanted) {
        // ignoreItemTimer sits in dArea.resources; activationTimer in the DropArea's.
        var pools = [obj.resources];
        const dh = dropHandlerOf(obj);
        if (dh)
            pools.push(dh.resources);
        for (var p = 0; p < pools.length; p++) {
            const res = pools[p];
            for (var i = 0; i < res.length; i++) {
                if (res[i] && res[i].interval === wanted)
                    return res[i];
            }
        }
        return null;
    }

    // A mock drag event shaped like DeclarativeDragDropEvent for the pure helpers:
    // .mimeData.formats (array), .hasUrls, .urls, and getDataAsByteArray(fmt).
    function mockEvent(opts) {
        return {
            mimeData: {
                formats: opts.formats !== undefined ? opts.formats : [],
                hasUrls: opts.hasUrls !== undefined ? opts.hasUrls : false,
                urls: opts.urls !== undefined ? opts.urls : [],
                serviceName: opts.serviceName !== undefined ? opts.serviceName : "",
                getDataAsByteArray: function(fmt) { return opts.serviceName !== undefined ? opts.serviceName : ""; }
            }
        };
    }

    // isMovingTask: true iff the taskbuttonitem mimetype is present.
    function test_isMovingTask() {
        const m = make();
        const dh = dropHandlerOf(m);
        verify(dh, "dropHandler not found");
        verify(dh.isMovingTask(mockEvent({formats: ["application/x-orgkdeplasmataskmanager_taskbuttonitem"]})));
        verify(!dh.isMovingTask(mockEvent({formats: ["text/uri-list"]})));
    }

    // isDroppingSeparator: true only when text/x-plasmoidservicename is the first
    // format AND the service name is one of the two separator applets.
    function test_isDroppingSeparator() {
        const m = make();
        const dh = dropHandlerOf(m);
        verify(dh.isDroppingSeparator(mockEvent({
            formats: ["text/x-plasmoidservicename"],
            serviceName: "org.kde.latte.separator"
        })));
        verify(dh.isDroppingSeparator(mockEvent({
            formats: ["text/x-plasmoidservicename"],
            serviceName: "audoban.applet.separator"
        })));
        // right format, wrong service -> false
        verify(!dh.isDroppingSeparator(mockEvent({
            formats: ["text/x-plasmoidservicename"],
            serviceName: "org.kde.latte.plasmoid"
        })));
        // separator service but not first format -> false
        verify(!dh.isDroppingSeparator(mockEvent({
            formats: ["text/uri-list", "text/x-plasmoidservicename"],
            serviceName: "org.kde.latte.separator"
        })));
    }

    // isDroppingOnlyLaunchers: when urls are present it asks backend.isApplication
    // for each (every()); the false fall-through is the no-urls, servicename-first
    // case. Assert both the backend handoff and the early-false branch.
    function test_isDroppingOnlyLaunchers() {
        const m = make();
        const dh = dropHandlerOf(m);

        backend.asked = [];
        backend.isAppAnswer = true;
        verify(dh.isDroppingOnlyLaunchers(mockEvent({
            hasUrls: true,
            urls: ["applications:firefox.desktop", "applications:dolphin.desktop"]
        })));
        // every() consulted the backend for the dropped urls
        compare(backend.asked.length, 2);
        compare(backend.asked[0], "applications:firefox.desktop");

        // one url not an application -> every() short-circuits to false
        backend.asked = [];
        backend.isAppAnswer = false;
        verify(!dh.isDroppingOnlyLaunchers(mockEvent({
            hasUrls: true,
            urls: ["file:///tmp/x"]
        })));

        // no urls and servicename is the first format -> the guarded false return
        verify(!dh.isDroppingOnlyLaunchers(mockEvent({
            hasUrls: false,
            formats: ["text/x-plasmoidservicename"]
        })));
    }

    // clearDroppingFlags zeroes the four inDropping* flags. Seed them via the
    // public read-only aliases' backing (set on the DropArea) then clear.
    function test_clearDroppingFlags() {
        const m = make();
        const dh = dropHandlerOf(m);
        dh.inDroppingFiles = true;
        dh.inDroppingOnlyLaunchers = true;
        dh.inDroppingSeparator = true;
        dh.inMovingTask = true;
        verify(dh.eventIsAccepted);

        dh.clearDroppingFlags();
        compare(dh.inDroppingFiles, false);
        compare(dh.inDroppingOnlyLaunchers, false);
        compare(dh.inDroppingSeparator, false);
        compare(dh.inMovingTask, false);
        verify(!dh.eventIsAccepted);
    }

    // onHoveredItemChanged hides the windows-preview dialog when the newly hovered
    // item differs from the dialog's active item. Drive the alias and assert the
    // hide(6.7) mock side-effect.
    function test_onHoveredItemChanged_hidesPreview() {
        const m = make();
        const other = createTemporaryObject(plainItemComponent, root, {});
        const hovered = createTemporaryObject(plainItemComponent, root, {});
        windowsPreviewDlg.activeItem = other;
        windowsPreviewDlg.lastHide = -1;
        windowsPreviewDlg.hideCalls = 0;

        m.hoveredItem = hovered;   // different from activeItem -> hide(6.7)
        compare(windowsPreviewDlg.hideCalls, 1);
        compare(windowsPreviewDlg.lastHide, 6.7);

        // hovering the active item itself must NOT hide it again.
        m.hoveredItem = other;
        compare(windowsPreviewDlg.hideCalls, 1);
    }

    // onDragSourceChanged (Connections target: root): when dragSource clears it
    // nulls ignoredItem and stops the ignore timer. Seed a non-null ignoredItem,
    // flip dragSource truthy then back to null, and assert it was cleared.
    function test_onDragSourceChanged_clearsIgnoredItem() {
        const m = make();
        const item = createTemporaryObject(plainItemComponent, root, {});
        root.dragSource = item;       // truthy: handler runs, !dragSource is false
        m.ignoredItem = item;
        verify(m.ignoredItem === item);

        root.dragSource = null;       // clears -> ignoredItem = null
        verify(m.ignoredItem === null);
    }

    // ignoreItemTimer onTriggered nulls ignoredItem. Seed it, shrink the 200ms
    // timer and fire it, assert the clear.
    function test_ignoreItemTimer_clearsIgnoredItem() {
        const m = make();
        const item = createTemporaryObject(plainItemComponent, root, {});
        root.dragSource = item;       // keep the Connections from nulling it first
        m.ignoredItem = item;
        verify(m.ignoredItem === item);

        const t = findTimerByInterval(m, 200);
        verify(t, "ignoreItemTimer (200ms) not found");
        t.interval = 1;
        t.restart();
        tryVerify(function() { return m.ignoredItem === null; }, 2000,
                  "ignoreItemTimer did not clear ignoredItem");
    }

    // activationTimer onTriggered, group-parent branch: with no dropping flags set
    // and a hovered group-parent item, it calls root.showPreviewForTasks(item).
    function test_activationTimer_groupParentShowsPreview() {
        const m = make();
        const dh = dropHandlerOf(m);
        dh.inDroppingOnlyLaunchers = false;
        dh.inDroppingSeparator = false;

        const hov = createTemporaryObject(hoverItemComponent, root, {});
        hov.m = {IsGroupParent: true, IsLauncher: false};
        m.hoveredItem = hov;

        root.previewCalls = 0;
        root.lastPreviewArg = undefined;
        tasksModel.activateCalls = 0;

        const t = findTimerByInterval(m, 250);
        verify(t, "activationTimer (250ms) not found");
        t.interval = 1;
        t.restart();

        tryVerify(function() { return root.previewCalls === 1; }, 2000,
                  "activationTimer did not call showPreviewForTasks");
        compare(root.lastPreviewArg, hov);
        // group-parent path must not request activation
        compare(tasksModel.activateCalls, 0);
    }

    // activationTimer onTriggered, non-launcher branch: a hovered window task
    // (not group-parent, not launcher) requests activation via tasksModel.
    function test_activationTimer_taskRequestsActivate() {
        const m = make();
        const dh = dropHandlerOf(m);
        dh.inDroppingOnlyLaunchers = false;
        dh.inDroppingSeparator = false;

        const hov = createTemporaryObject(hoverItemComponent, root, {});
        hov.m = {IsGroupParent: false, IsLauncher: false};
        hov.modelIdx = 7;
        m.hoveredItem = hov;

        windowsPreviewDlg.visible = false;   // skip the inner tooltip-hide guard
        root.previewCalls = 0;
        tasksModel.activateCalls = 0;
        tasksModel.lastActivate = undefined;

        const t = findTimerByInterval(m, 250);
        verify(t, "activationTimer (250ms) not found");
        t.interval = 1;
        t.restart();

        tryVerify(function() { return tasksModel.activateCalls === 1; }, 2000,
                  "activationTimer did not request task activation");
        compare(tasksModel.lastActivate, 7);
        // non-group path must not show a group preview
        compare(root.previewCalls, 0);
    }

    // onDragLeave resets the whole drag state: containsDrag cleared, hovered item
    // nulled, the four dropping flags cleared (via clearDroppingFlags), and the
    // activation timer stopped. The handler never reads its event, so emitting the
    // DropArea's dragLeave(null) signal drives it honestly.
    function test_onDragLeave_resetsDragState() {
        const m = make();
        const dh = dropHandlerOf(m);

        const hov = createTemporaryObject(plainItemComponent, root, {});
        m.hoveredItem = hov;
        m.containsDrag = true;
        dh.inDroppingFiles = true;
        dh.inMovingTask = true;
        dh.inDroppingOnlyLaunchers = true;
        dh.inDroppingSeparator = true;
        verify(dh.eventIsAccepted);

        dh.dragLeave(null);

        compare(m.containsDrag, false);
        verify(m.hoveredItem === null);
        verify(!dh.eventIsAccepted);
        compare(dh.inDroppingFiles, false);
        compare(dh.inMovingTask, false);
        compare(dh.inDroppingOnlyLaunchers, false);
        compare(dh.inDroppingSeparator, false);
    }

    // activationTimer onTriggered, early-return branch: while dropping launchers
    // the handler returns before touching either sink.
    function test_activationTimer_droppingLaunchersEarlyReturn() {
        const m = make();
        const dh = dropHandlerOf(m);
        dh.inDroppingOnlyLaunchers = true;   // forces the guard's early return

        const hov = createTemporaryObject(hoverItemComponent, root, {});
        hov.m = {IsGroupParent: true, IsLauncher: false};
        m.hoveredItem = hov;

        root.previewCalls = 0;
        tasksModel.activateCalls = 0;

        const t = findTimerByInterval(m, 250);
        verify(t, "activationTimer (250ms) not found");
        t.interval = 1;
        t.restart();

        // give the timer time to fire, then assert neither sink was touched.
        wait(80);
        compare(root.previewCalls, 0);
        compare(tasksModel.activateCalls, 0);
    }
}
