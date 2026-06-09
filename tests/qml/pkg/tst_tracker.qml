// Drives the animations Tracker helper (an event-set bookkeeper) through its
// two public functions, addEvent and removeEvent. The component is loaded from
// the staged (instrumented) package by file URL so the Cov.tick calls fire, and
// every assertion pins an observable effect: the `count` property and the
// `events` array it maintains.
//
// Tracker reads no unqualified creation-context names — it is a self-contained
// Item with two local properties — so no mock context is needed. Both functions
// and both of their branches (the dedup/early-out guard and the mutating path)
// run honestly headless.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "AnimationsTracker"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/definition/animations/Tracker.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // addEvent: a fresh event is pushed and bumps count; re-adding the same
    // event hits the indexOf >= 0 guard and is a no-op (no dup, count unchanged).
    function test_addEvent() {
        const m = make();
        compare(m.count, 0);
        compare(m.events.length, 0);

        m.addEvent("zoom");
        compare(m.count, 1);
        compare(m.events.length, 1);
        compare(m.events[0], "zoom");

        // distinct event -> appended, count rises again
        m.addEvent("hover");
        compare(m.count, 2);
        compare(m.events.length, 2);

        // duplicate -> guarded early-out, nothing appended, count stays put
        m.addEvent("zoom");
        compare(m.count, 2);
        compare(m.events.length, 2);
    }

    // removeEvent: removing a present event splices it out and drops count;
    // removing an absent event hits the pos < 0 guard and leaves state alone.
    function test_removeEvent() {
        const m = make();
        m.addEvent("a");
        m.addEvent("b");
        compare(m.count, 2);

        // present -> spliced out, count drops, only "b" remains
        m.removeEvent("a");
        compare(m.count, 1);
        compare(m.events.length, 1);
        compare(m.events.indexOf("a"), -1);
        compare(m.events[0], "b");

        // absent -> guarded early-out, no splice, count unchanged
        m.removeEvent("a");
        compare(m.count, 1);
        compare(m.events.length, 1);

        // remove the last remaining event -> empties the set
        m.removeEvent("b");
        compare(m.count, 0);
        compare(m.events.length, 0);
    }
}
