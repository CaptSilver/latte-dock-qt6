// Drives the launcher/frozen-task bookkeeping in the plasmoid's
// TasksExtendedManager through its real public functions, the three
// launcher-signal Connections handlers, and the three internal Timers.
// The component is loaded from the staged (instrumented) package by file
// URL, and every assertion pins an observable effect: a return value, a
// counter/property change, a signal emission, or a mock side-effect.
//
// The component reads three unqualified context names — appletAbilities,
// tasksModel — only inside its Connections target and its move/sync timer
// handlers. We declare them on the TestCase (the component's creation
// context) shaped like the real objects so those handlers resolve and run
// for real instead of throwing ReferenceError.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "TasksExtManager"
    when: windowShown

    // Mock of the launchers ability the component's Connections binds to.
    // It carries the three real signals plus the one method the sync timer
    // calls, and records that the method ran. Not a catch-all: only the
    // members TasksExtendedManager actually touches.
    QtObject {
        id: launchersAbility
        signal launcherInRemoving(string launcherUrl)
        signal launcherInAdding(string launcherUrl)
        signal launcherInMoving(string launcherUrl, int pos)
        property int validateCalls: 0
        function validateSyncedLaunchersOrder() { validateCalls++; }
    }

    QtObject {
        id: appletAbilities
        property QtObject launchers: launchersAbility
    }

    // Mock of the LibTaskManager model the move/sync timers drive. Records
    // the move arguments and the syncLaunchers call so the timer handlers
    // are asserted, not merely executed.
    QtObject {
        id: tasksModel
        property int moveFrom: -99
        property int moveTo: -99
        property int moveCalls: 0
        property int syncCalls: 0
        function move(from, to) { moveFrom = from; moveTo = to; moveCalls++; }
        function syncLaunchers() { syncCalls++; }
    }

    // Load the instrumented component from the staged install tree. The
    // stage lives at <repo>/build/_qmlcov/stage; from tests/qml/pkg that's
    // up three (to the repo root) then down into the staged plasmoid package.
    readonly property url target: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/TasksExtendedManager.qml")

    function make() {
        const c = Qt.createComponent(target);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root);
        verify(obj, "instantiate failed");
        return obj;
    }

    function test_waitingLaunchers() {
        const m = make();
        compare(m.waitingLaunchersLength(), 0);
        verify(!m.waitingLauncherExists("applications:firefox.desktop"));

        m.addWaitingLauncher("applications:firefox.desktop");
        compare(m.waitingLaunchersLength(), 1);
        // addWaitingLauncher early-returns when an equal entry already exists.
        m.addWaitingLauncher("applications:firefox.desktop");
        compare(m.waitingLaunchersLength(), 1);

        // equals() matches on either string containing the other, so the
        // bare desktop id finds the fully-qualified stored entry.
        verify(m.waitingLauncherExists("firefox.desktop"));
        verify(!m.waitingLauncherExists("chromium.desktop"));

        m.printWaitingLaunchers();

        // removeWaitingLauncher fires waitingLauncherRemoved on a hit.
        const spy = createTemporaryObject(signalSpyComponent, root,
                                          {target: m, signalName: "waitingLauncherRemoved"});
        m.removeWaitingLauncher("applications:firefox.desktop");
        compare(spy.count, 1);
        compare(spy.signalArguments[0][0], "applications:firefox.desktop");
        compare(m.waitingLaunchersLength(), 0);

        // removeWaitingLauncher over an empty set walks the loop, no signal.
        m.removeWaitingLauncher("nope");
        compare(spy.count, 1);
        verify(!m.waitingLauncherExists("nope"));
    }

    function test_toBeAdded() {
        const m = make();
        verify(!m.toBeAddedLauncherExists("a:b.desktop"));
        compare(m.launchersToBeAddedCount, 0);

        m.addToBeAddedLauncher("a:b.desktop");
        compare(m.launchersToBeAddedCount, 1);
        m.addToBeAddedLauncher("a:b.desktop"); // exists -> early return
        compare(m.launchersToBeAddedCount, 1);
        verify(m.toBeAddedLauncherExists("a:b.desktop"));

        m.printToBeAddedLaunchers();

        m.removeToBeAddedLauncher("a:b.desktop");
        compare(m.launchersToBeAddedCount, 0);
        verify(!m.toBeAddedLauncherExists("a:b.desktop"));

        m.removeToBeAddedLauncher("missing"); // loop, no match
        compare(m.launchersToBeAddedCount, 0);
    }

    function test_toBeRemoved() {
        const m = make();
        verify(!m.isLauncherToBeRemoved("x:y.desktop"));
        compare(m.launchersToBeRemovedCount, 0);

        // removeToBeRemovedLauncher early-returns when not present.
        m.removeToBeRemovedLauncher("x:y.desktop");
        compare(m.launchersToBeRemovedCount, 0);

        m.addToBeRemovedLauncher("x:y.desktop");
        compare(m.launchersToBeRemovedCount, 1);
        m.addToBeRemovedLauncher("x:y.desktop"); // exists -> early return
        compare(m.launchersToBeRemovedCount, 1);
        verify(m.isLauncherToBeRemoved("x:y.desktop"));

        m.printToBeRemovedLaunchers();

        m.removeToBeRemovedLauncher("x:y.desktop");
        compare(m.launchersToBeRemovedCount, 0);
        verify(!m.isLauncherToBeRemoved("x:y.desktop"));
    }

    function test_immediate() {
        const m = make();
        verify(!m.immediateLauncherExists("im:1.desktop"));

        m.addImmediateLauncher("im:1.desktop");
        verify(m.immediateLauncherExists("im:1.desktop"));
        m.addImmediateLauncher("im:1.desktop"); // already present -> no dup
        verify(m.immediateLauncherExists("im:1.desktop"));

        m.printImmediateLaunchers();

        m.removeImmediateLauncher("im:1.desktop");
        verify(!m.immediateLauncherExists("im:1.desktop"));
        m.removeImmediateLauncher("missing"); // loop, no match
        verify(!m.immediateLauncherExists("missing"));
    }

    function test_frozenTasks() {
        const m = make();
        verify(m.getFrozenTask("t1") === undefined);

        m.setFrozenTask("t1", 1.5);   // push branch
        compare(m.getFrozenTask("t1").zoom, 1.5);
        m.setFrozenTask("t1", 2.0);   // update-existing branch
        const f = m.getFrozenTask("t1");
        verify(f !== undefined);
        compare(f.zoom, 2.0);

        m.printFrozenTasks();

        m.removeFrozenTask("t1");
        verify(m.getFrozenTask("t1") === undefined);
        m.removeFrozenTask("t1");     // taskIndex stays -1, no splice
        verify(m.getFrozenTask("t1") === undefined);
    }

    function test_toBeMoved() {
        const m = make();
        verify(!m.isLauncherToBeMoved("mv:1.desktop"));
        compare(m.posOfLauncherToBeMoved("mv:1.desktop"), -1);
        compare(m.launchersToBeMovedCount, 0);

        m.addLauncherToBeMoved("mv:1.desktop", 3);
        compare(m.launchersToBeMovedCount, 1);
        m.addLauncherToBeMoved("mv:1.desktop", 9); // already moving -> skip
        compare(m.launchersToBeMovedCount, 1);
        verify(m.isLauncherToBeMoved("mv:1.desktop"));
        // the second (skipped) add must NOT overwrite the stored pos.
        compare(m.posOfLauncherToBeMoved("mv:1.desktop"), 3);

        // negative target positions are clamped to 0 by addLauncherToBeMoved.
        m.addLauncherToBeMoved("mv:neg.desktop", -5);
        compare(m.posOfLauncherToBeMoved("mv:neg.desktop"), 0);

        m.printToBeMovedLaunchers();

        m.removeLauncherToBeMoved("mv:1.desktop");
        verify(!m.isLauncherToBeMoved("mv:1.desktop"));
        m.removeLauncherToBeMoved("mv:1.desktop"); // not present -> early return
        verify(!m.isLauncherToBeMoved("mv:1.desktop"));
    }

    // The three launcher-state signals from appletAbilities.launchers drive
    // the Connections handlers, which forward into the to-be-removed/added/
    // moved books. Emit each and assert the resulting array state.
    function test_connectionsHandlers() {
        const m = make();

        launchersAbility.launcherInRemoving("conn:rm.desktop");
        verify(m.isLauncherToBeRemoved("conn:rm.desktop"));
        compare(m.launchersToBeRemovedCount, 1);

        launchersAbility.launcherInAdding("conn:add.desktop");
        verify(m.toBeAddedLauncherExists("conn:add.desktop"));
        compare(m.launchersToBeAddedCount, 1);

        launchersAbility.launcherInMoving("conn:mv.desktop", 4);
        verify(m.isLauncherToBeMoved("conn:mv.desktop"));
        compare(m.posOfLauncherToBeMoved("conn:mv.desktop"), 4);
        compare(m.launchersToBeMovedCount, 1);
    }

    // moveLauncherToCorrectPos arms launchersToBeMovedTimer, which on fire
    // calls tasksModel.move(from,to) and chains delayedLaynchersSyncTimer,
    // which calls tasksModel.syncLaunchers() +
    // appletAbilities.launchers.validateSyncedLaunchersOrder() and zeroes the
    // moved counter. Shrink both timer intervals and assert the mock saw the
    // real calls with the right arguments.
    function test_moveLauncherToCorrectPos() {
        const m = make();
        tasksModel.moveFrom = -99;
        tasksModel.moveTo = -99;
        tasksModel.moveCalls = 0;
        tasksModel.syncCalls = 0;
        launchersAbility.validateCalls = 0;

        m.addLauncherToBeMoved("mv:run.desktop", 7);
        verify(m.isLauncherToBeMoved("mv:run.desktop"));

        // launchersToBeMovedTimer = 50ms interval, delayedLaynchersSyncTimer = 450ms.
        const moveTimer = findTimerByInterval(m, 50);
        const syncTimer = findTimerByInterval(m, 450);
        verify(moveTimer, "could not find launchersToBeMovedTimer");
        verify(syncTimer, "could not find delayedLaynchersSyncTimer");
        moveTimer.interval = 1;
        syncTimer.interval = 1;

        m.moveLauncherToCorrectPos("mv:run.desktop", 2);
        // moveLauncherToCorrectPos removed it from the pending-move book.
        verify(!m.isLauncherToBeMoved("mv:run.desktop"));

        // move timer fires -> tasksModel.move(2, 7)
        tryVerify(function() { return tasksModel.moveCalls === 1; }, 2000,
                  "move timer never called tasksModel.move");
        compare(tasksModel.moveFrom, 2);
        compare(tasksModel.moveTo, 7);

        // sync timer chained -> syncLaunchers + validate + counter reset
        tryVerify(function() { return tasksModel.syncCalls === 1; }, 2000,
                  "sync timer never called tasksModel.syncLaunchers");
        compare(launchersAbility.validateCalls, 1);
        compare(m.launchersToBeMovedCount, 0);
    }

    // The garbage-collector timer's onTriggered prints + clears every local
    // array and zeroes the paused-state counters. Seed all six books, fire it,
    // and assert everything drained.
    function test_garbageCollectorTimer() {
        const m = make();
        m.addImmediateLauncher("gc:1.desktop");
        m.addToBeAddedLauncher("gc:2.desktop");
        m.addLauncherToBeMoved("gc:3.desktop", 0);
        m.addToBeRemovedLauncher("gc:4.desktop");
        m.addWaitingLauncher("gc:5.desktop");
        m.setFrozenTask("gc:t", 1.2);

        verify(m.launchersInPausedStateCount > 0);

        const gc = findTimerByInterval(m, 30000);
        verify(gc, "could not find arraysGarbageCollectorTimer");
        gc.interval = 1;
        gc.restart();

        tryVerify(function() { return m.launchersToBeMovedCount === 0
                                   && m.launchersToBeAddedCount === 0
                                   && m.launchersToBeRemovedCount === 0
                                   && m.launchersInPausedStateCount === 0; },
                  2000, "GC timer did not run / reset counters");
        verify(!m.immediateLauncherExists("gc:1.desktop"));
        verify(!m.waitingLauncherExists("gc:5.desktop"));
        verify(m.getFrozenTask("gc:t") === undefined);
    }

    // Timers are non-visual, so they live in `resources`, not `children`.
    // The three timers carry distinct default intervals (50 / 450 / 30000 ms),
    // so match on that — survives reordering and doesn't depend on id strings.
    function findTimerByInterval(m, wantedInterval) {
        const res = m.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].interval === wantedInterval)
                return res[i];
        }
        return null;
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
