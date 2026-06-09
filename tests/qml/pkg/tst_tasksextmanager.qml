// Coverage spike: drive the launcher/frozen-task bookkeeping in the plasmoid's
// TasksExtendedManager through its public functions. The component is loaded
// from the staged (instrumented) package by file URL, so every function call
// fires a Cov tick.
import QtQuick
import QtTest

TestCase {
    id: tc
    name: "TasksExtManager"
    when: windowShown

    // Load the instrumented component from the staged install tree. The stage
    // lives at <repo>/build/_qmlcov/stage; from tests/qml/pkg that's up three
    // (to the repo root) then down into the staged plasmoid package.
    readonly property url target: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/TasksExtendedManager.qml")

    function make() {
        const c = Qt.createComponent(target);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, tc);
        verify(obj, "instantiate failed");
        return obj;
    }

    function test_waitingLaunchers() {
        const m = make();
        compare(m.waitingLaunchersLength(), 0);
        verify(!m.waitingLauncherExists("applications:firefox.desktop"));
        m.addWaitingLauncher("applications:firefox.desktop");
        // addWaitingLauncher early-returns when it already exists.
        m.addWaitingLauncher("applications:firefox.desktop");
        verify(m.waitingLauncherExists("firefox.desktop")); // substring match via equals()
        compare(m.waitingLaunchersLength(), 1);
        m.printWaitingLaunchers();
        m.removeWaitingLauncher("applications:firefox.desktop");
        compare(m.waitingLaunchersLength(), 0);
        // removeWaitingLauncher over an empty set walks the loop and returns.
        m.removeWaitingLauncher("nope");
    }

    function test_toBeAdded() {
        const m = make();
        verify(!m.toBeAddedLauncherExists("a:b.desktop"));
        m.addToBeAddedLauncher("a:b.desktop");
        m.addToBeAddedLauncher("a:b.desktop"); // exists -> early return
        verify(m.toBeAddedLauncherExists("a:b.desktop"));
        compare(m.launchersToBeAddedCount, 1);
        m.printToBeAddedLaunchers();
        m.removeToBeAddedLauncher("a:b.desktop");
        compare(m.launchersToBeAddedCount, 0);
        m.removeToBeAddedLauncher("missing"); // loop, no match
    }

    function test_toBeRemoved() {
        const m = make();
        verify(!m.isLauncherToBeRemoved("x:y.desktop"));
        // removeToBeRemovedLauncher early-returns when not present.
        m.removeToBeRemovedLauncher("x:y.desktop");
        m.addToBeRemovedLauncher("x:y.desktop");
        m.addToBeRemovedLauncher("x:y.desktop"); // exists -> early return
        verify(m.isLauncherToBeRemoved("x:y.desktop"));
        compare(m.launchersToBeRemovedCount, 1);
        m.printToBeRemovedLaunchers();
        m.removeToBeRemovedLauncher("x:y.desktop");
        compare(m.launchersToBeRemovedCount, 0);
    }

    function test_immediate() {
        const m = make();
        verify(!m.immediateLauncherExists("im:1.desktop"));
        m.addImmediateLauncher("im:1.desktop");
        m.addImmediateLauncher("im:1.desktop"); // exists branch
        verify(m.immediateLauncherExists("im:1.desktop"));
        m.printImmediateLaunchers();
        m.removeImmediateLauncher("im:1.desktop");
        verify(!m.immediateLauncherExists("im:1.desktop"));
        m.removeImmediateLauncher("missing"); // loop, no match
    }

    function test_frozenTasks() {
        const m = make();
        verify(m.getFrozenTask("t1") === undefined);
        m.setFrozenTask("t1", 1.5);   // push branch
        m.setFrozenTask("t1", 2.0);   // update-existing branch
        const f = m.getFrozenTask("t1");
        verify(f !== undefined);
        compare(f.zoom, 2.0);
        m.printFrozenTasks();
        m.removeFrozenTask("t1");
        verify(m.getFrozenTask("t1") === undefined);
        m.removeFrozenTask("t1");     // taskIndex stays -1, no splice
    }

    function test_toBeMoved() {
        const m = make();
        verify(!m.isLauncherToBeMoved("mv:1.desktop"));
        compare(m.posOfLauncherToBeMoved("mv:1.desktop"), -1);
        m.addLauncherToBeMoved("mv:1.desktop", 3);
        m.addLauncherToBeMoved("mv:1.desktop", 9); // already moving -> skip
        verify(m.isLauncherToBeMoved("mv:1.desktop"));
        compare(m.posOfLauncherToBeMoved("mv:1.desktop"), 3);
        compare(m.launchersToBeMovedCount, 1);
        m.printToBeMovedLaunchers();
        m.removeLauncherToBeMoved("mv:1.desktop");
        verify(!m.isLauncherToBeMoved("mv:1.desktop"));
        m.removeLauncherToBeMoved("mv:1.desktop"); // not present -> early return
    }

    // The garbage-collector timer's onTriggered only prints + splices the local
    // arrays (no tasksModel), so we can reach it by shrinking the interval and
    // letting it fire. This covers the biggest non-model handler in the file.
    function test_garbageCollectorTimer() {
        const m = make();
        m.addImmediateLauncher("gc:1.desktop");
        m.addToBeAddedLauncher("gc:2.desktop");
        m.addLauncherToBeMoved("gc:3.desktop", 0);
        m.addToBeRemovedLauncher("gc:4.desktop");
        m.addWaitingLauncher("gc:5.desktop");
        m.setFrozenTask("gc:t", 1.2);

        const gc = findGcTimer(m);
        verify(gc, "could not find arraysGarbageCollectorTimer");
        gc.interval = 1;
        gc.restart();
        tryVerify(function() { return m.launchersToBeMovedCount === 0
                                   && m.launchersToBeAddedCount === 0
                                   && m.launchersToBeRemovedCount === 0; },
                  2000, "GC timer did not run / reset counters");
    }

    // Timers are non-visual, so they live in `resources`, not `children`. The
    // GC timer is the one with a 30s interval; match on that so the test
    // survives reordering.
    function findGcTimer(m) {
        const res = m.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].interval === 30000)
                return res[i];
        }
        return null;
    }
}
