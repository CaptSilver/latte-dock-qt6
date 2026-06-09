// Drives the plasmoid task SubWindows bookkeeping (window-count, state
// aggregation, title collection, next/prev/minimize cycling) through its public
// functions and asserts the observable result of each.
//
// Two context puzzles the staged (instrumented) SubWindows needs solved to run
// headlessly:
//   1. It reads *unqualified* object names (taskItem, root, tasksModel, index).
//      Those resolve through the QML lexical scope of the document that creates
//      it, not a visual parent's properties -- so we load it through a Loader
//      whose host root declares them; a Loader runs its source in the Loader's
//      creation context.
//   2. It reads *model-role* names: the top-level IsLauncher/IsStartup/IsWindow/
//      IsGroupParent/isGroupParent (the SubWindows row's own roles), and the
//      inner DelegateModel children's IsActive/IsMinimized/IsWindow/display/
//      WinIdList. QML forbids upper-case *property* names, but a ListModel's
//      *roles* may be upper-case and resolve as unqualified names in a
//      delegate's context.
//
// So: the Loader is the delegate of a one-row Repeater over the host's roles
// ListModel (top-level roles). The inner DelegateModel's source is tasksModel,
// itself a ListModel that also carries the request* methods SubWindows calls,
// counting the calls so the cycling functions are checked by side effect.
import QtQuick
import QtQml.Models
import QtTest

TestCase {
    id: tc
    name: "SubWindows"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/task/SubWindows.qml")

    // tasksModel: the source of the inner DelegateModel AND the object that gets
    // request* called on it. A ListModel can carry upper-case roles (via
    // append) and also host plain JS functions, so it doubles as the dispatch
    // spy: each cycling call lands here and bumps a counter.
    ListModel {
        id: fakeTasksModel
        property int activateCalls: 0
        property int toggleCalls: 0
        property var lastActivate: undefined
        property var lastToggle: undefined
        function makeModelIndex(a, b) { return { row: a, col: b }; }
        function requestActivate(idx) { activateCalls++; lastActivate = idx; }
        function requestToggleMinimized(idx) { toggleCalls++; lastToggle = idx; }
    }

    // taskItem: SubWindows reads isActive/isMinimized/isWindow/isGroupParent,
    // calls modelIndex(), and connects to its checkWindowsStates() signal.
    QtObject {
        id: fakeTaskItem
        property bool isActive: false
        property bool isMinimized: false
        property bool isWindow: true
        property bool isGroupParent: false
        signal checkWindowsStates()
        function modelIndex() { return tc.fakeRootIndex; }
    }

    QtObject {
        id: fakeRoot
        property bool inDraggingPhase: false
        property bool plasma515: true
    }

    property var fakeRootIndex: ({ row: 0 })

    // Reset the inner model to a single child row read by the DelegateModel.
    function seedChild(active, minimized, window) {
        fakeTasksModel.clear();
        fakeTasksModel.append({
            IsActive: active, IsMinimized: minimized, IsWindow: window,
            display: "win0", WinIdList: [11], LegacyWinIdList: [11]
        });
    }

    // Two children, so the group windowsCount (2) is distinct from the
    // non-group hardcoded 1. child0 = active+shown winId 11, child1 = minimized
    // winId 22.
    function seedTwo() {
        fakeTasksModel.clear();
        fakeTasksModel.append({
            IsActive: true, IsMinimized: false, IsWindow: true,
            display: "win0", WinIdList: [11], LegacyWinIdList: [11]
        });
        fakeTasksModel.append({
            IsActive: false, IsMinimized: true, IsWindow: false,
            display: "win1", WinIdList: [22], LegacyWinIdList: [22]
        });
    }

    // Host document. The roles ListModel feeds the top-level roles, the host
    // root feeds the unqualified object names, and the Loader (delegate) loads
    // the staged SubWindows in that creation context.
    property Component hostComponent: Component {
        id: hostComp
        Item {
            id: host
            property var taskItem: fakeTaskItem
            property var root: fakeRoot
            property var tasksModel: fakeTasksModel
            property int index: 0

            property alias roles: rolesModel
            property var sub: null

            ListModel {
                id: rolesModel
                ListElement {
                    IsLauncher: false
                    IsStartup: false
                    IsWindow: true
                    IsGroupParent: false
                    isGroupParent: false
                }
            }

            Repeater {
                id: rep
                model: rolesModel
                delegate: Loader {
                    source: tc.targetUrl
                    onLoaded: host.sub = item
                }
            }
        }
    }

    function makeHost() {
        seedChild(true, false, true);
        const c = tc.hostComponent;
        verify(c.status === Component.Ready, "host compile failed: " + c.errorString());
        const h = createTemporaryObject(c, tc);
        verify(h, "host instantiate failed");
        verify(h.sub, "SubWindows did not load through the host Loader");
        return h;
    }

    function setGroup(host, on) {
        host.roles.setProperty(0, "isGroupParent", on);
        host.roles.setProperty(0, "IsGroupParent", on);
    }

    // Instantiation wires the checkWindowsStates connection (Component.onCompleted)
    // and the DelegateModel rootIndex assignment (its Component.onCompleted), and
    // evaluates the windowsCount / isLauncher / isStartup / isWindow bindings off
    // the seeded roles.
    function test_instantiate() {
        const h = makeHost();
        const sub = h.sub;
        // not launcher/startup; isGroupParent role is false -> windowsCount 1
        compare(sub.windowsCount, 1);
        compare(sub.isWindow, true);
        compare(sub.isLauncher, false);
        compare(sub.isStartup, false);
        compare(sub.lastActiveWinInGroup, -1);
    }

    // windowsCount group branch returns windowsRepeater.count. With two seeded
    // children it is 2, distinct from the non-group constant 1, so the branch is
    // observably taken.
    function test_windowsCount_group() {
        seedTwo();
        const h = tc.hostComponent.createObject(tc);
        verify(h && h.sub, "host/SubWindows did not load");
        const sub = h.sub;
        compare(sub.windowsCount, 1);   // role still non-group
        setGroup(h, true);
        compare(sub.windowsCount, 2);   // group branch -> windowsRepeater.count
        setGroup(h, false);
        compare(sub.windowsCount, 1);
        h.destroy();
    }

    // initializeStates() non-group branch sets hasActive/hasMinimized/hasShown
    // and windowsMinimized off taskItem's own state.
    function test_states_single() {
        const h = makeHost();
        const sub = h.sub;
        setGroup(h, false);

        fakeTaskItem.isActive = true;
        fakeTaskItem.isMinimized = false;
        fakeTaskItem.isWindow = true;
        sub.initializeStates();
        compare(sub.hasActive, true);
        compare(sub.hasMinimized, false);
        compare(sub.hasShown, true);
        compare(sub.windowsMinimized, 0);

        fakeTaskItem.isActive = false;
        fakeTaskItem.isMinimized = true;
        sub.initializeStates();
        compare(sub.hasMinimized, true);
        compare(sub.hasShown, false);
        compare(sub.windowsMinimized, 1);

        fakeTaskItem.isMinimized = false;
        fakeTaskItem.isWindow = true;
        sub.initializeStates();
        compare(sub.hasShown, true);
        compare(sub.windowsMinimized, 0);
    }

    // initializeStates() group branch -> checkInternalStates() over the model.
    // Two children (one active+shown, one minimized) light all three flags and
    // count exactly one minimized.
    function test_states_group() {
        seedTwo();
        const h = tc.hostComponent.createObject(tc);
        verify(h && h.sub, "host/SubWindows did not load");
        const sub = h.sub;
        setGroup(h, true);

        sub.initializeStates();
        compare(sub.hasActive, true);
        compare(sub.hasMinimized, true);
        compare(sub.hasShown, true);
        compare(sub.windowsMinimized, 1);

        // a single non-minimized window: minimized count drops to 0
        seedChild(false, false, true);
        sub.initializeStates();
        compare(sub.hasShown, true);
        compare(sub.hasMinimized, false);
        compare(sub.windowsMinimized, 0);

        setGroup(h, false);
        h.destroy();
    }

    // updateStates() schedules the 200ms timer when not dragging; drop the
    // interval and let it fire so initializeStates() runs from the timer. The
    // dragging branch must NOT start the timer.
    function test_updateStates_timer() {
        const h = makeHost();
        const sub = h.sub;
        setGroup(h, false);
        fakeRoot.inDraggingPhase = false;

        const t = findTimer(sub);
        verify(t, "could not find initializeStatesTimer");
        t.interval = 1;

        sub.hasActive = false;
        fakeTaskItem.isActive = true;
        sub.updateStates();   // starts the timer
        tryVerify(function() { return sub.hasActive === true; }, 2000,
                  "timer-driven initializeStates did not run");

        // dragging branch: updateStates must NOT (re)start the timer
        t.stop();
        fakeRoot.inDraggingPhase = true;
        sub.updateStates();
        compare(t.running, false);
        fakeRoot.inDraggingPhase = false;
    }

    // checkInternalStates() directly over a one-row minimized model: hasMinimized
    // set, hasShown left false, exactly one minimized counted.
    function test_checkInternalStates() {
        const h = makeHost();
        const sub = h.sub;
        seedChild(false, true, false);
        sub.hasMinimized = false;
        sub.hasShown = false;
        sub.checkInternalStates();
        compare(sub.hasMinimized, true);
        compare(sub.hasShown, false);
        compare(sub.windowsMinimized, 1);
    }

    // windowsTitles() walks the model and returns the display roles in order.
    function test_windowsTitles() {
        seedTwo();
        const h = tc.hostComponent.createObject(tc);
        verify(h && h.sub, "host/SubWindows did not load");
        const sub = h.sub;
        const titles = sub.windowsTitles();
        compare(titles.length, 2);
        compare(titles[0], "win0");
        compare(titles[1], "win1");
        h.destroy();
    }

    // The cycling functions early-return unless taskItem.isGroupParent, so no
    // request* dispatch reaches tasksModel.
    function test_cycling_nonGroup() {
        const h = makeHost();
        const sub = h.sub;
        fakeTaskItem.isGroupParent = false;
        fakeTasksModel.activateCalls = 0;
        fakeTasksModel.toggleCalls = 0;
        sub.activateNextTask();
        sub.activatePreviousTask();
        sub.minimizeTask();
        compare(fakeTasksModel.activateCalls, 0);
        compare(fakeTasksModel.toggleCalls, 0);
    }

    // Cycling over a group, asserting the dispatch side effect on each arm:
    //   - active child found: next+prev each activate, minimize toggles
    //   - no active child but lastActive matches a winId: same dispatch
    //   - no active child, lastActive no match: default-to-0 arms still dispatch
    //   - single minimized child, no active/lastActive: minimizeTask finds
    //     nothing to toggle (every candidate is already minimized) -> no toggle
    function test_cycling_group() {
        const h = makeHost();
        const sub = h.sub;
        fakeTaskItem.isGroupParent = true;

        // child active -> next/prev activate, minimize toggles
        seedChild(true, false, true);
        fakeTasksModel.activateCalls = 0;
        fakeTasksModel.toggleCalls = 0;
        sub.activateNextTask();
        sub.activatePreviousTask();
        compare(fakeTasksModel.activateCalls, 2);
        compare(fakeTasksModel.lastActivate.row, 0);   // index role is 0
        sub.minimizeTask();
        compare(fakeTasksModel.toggleCalls, 1);

        // no active child but lastActive matches the child's winId -> fallback
        // loops find it and dispatch
        seedChild(false, false, true);
        sub.lastActiveWinInGroup = 11;
        fakeTasksModel.activateCalls = 0;
        fakeTasksModel.toggleCalls = 0;
        sub.activateNextTask();
        sub.activatePreviousTask();
        sub.minimizeTask();
        compare(fakeTasksModel.activateCalls, 2);
        compare(fakeTasksModel.toggleCalls, 1);

        // no active child, lastActive no match -> default-to-0 arms still dispatch
        sub.lastActiveWinInGroup = 999;
        fakeTasksModel.activateCalls = 0;
        fakeTasksModel.toggleCalls = 0;
        sub.activateNextTask();
        sub.activatePreviousTask();
        sub.minimizeTask();
        compare(fakeTasksModel.activateCalls, 2);
        compare(fakeTasksModel.toggleCalls, 1);

        // single already-minimized child, nothing active or remembered ->
        // minimizeTask has no un-minimized window to toggle
        seedChild(false, true, false);
        sub.lastActiveWinInGroup = -1;
        fakeTasksModel.toggleCalls = 0;
        sub.minimizeTask();
        compare(fakeTasksModel.toggleCalls, 0);

        fakeTaskItem.isGroupParent = false;
    }

    // The checkWindowsStates signal connection (Component.onCompleted) routes
    // back into initializeStates().
    function test_checkWindowsStates_signal() {
        const h = makeHost();
        const sub = h.sub;
        setGroup(h, false);
        fakeTaskItem.isActive = true;
        fakeTaskItem.isMinimized = false;
        sub.hasActive = false;
        fakeTaskItem.checkWindowsStates();   // -> initializeStates()
        compare(sub.hasActive, true);
    }

    // Timers are non-visual: they live in resources, not children.
    function findTimer(sub) {
        const res = sub.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].hasOwnProperty("interval") && res[i].interval === 200)
                return res[i];
        }
        for (var j = 0; j < res.length; j++) {
            if (res[j] && res[j].hasOwnProperty("interval"))
                return res[j];
        }
        return null;
    }
}
