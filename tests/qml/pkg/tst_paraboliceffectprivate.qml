// Coverage for the containment's ParabolicEffectPrivate ability. The component
// extends AbilityHost.ParabolicEffect (so restoreZoomIsBlocked, currentParabolicItem,
// directRenderingEnabled, sglClearZoom and the start/stopRestoreZoomTimer methods are
// real inherited members) and adds the restore-zoom bookkeeping plus a 50ms restore
// timer. It reads a handful of unqualified context names — root (Connections fallback
// target + the isBindingUpdateEnabled drag guard), animations, debug, layouts, view,
// settings — which QML resolves against the creation context. We name the TestCase
// `root` and shape every one of those names like the real object, then drive the
// instrumented units and assert an observable effect each.
//
// Loaded from the staged (instrumented) package by file URL so each unit fires a Cov
// tick. `layouts` carries startLayout/mainLayout/endLayout Items so the
// restoreZoomIsBlockedFromApplet Binding evaluates cleanly (its value loop walks
// grid.children) and so the onContextMenuIsShownChanged Connections has a live target.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "ParabolicEffectPrivate"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/privates/ParabolicEffectPrivate.qml")

    // ---- unqualified context names the component reads ----

    // The isBindingUpdateEnabled drag guard reads root.dragOverlay; null keeps the
    // binding enabled (the real "not dragging" state) so the value loop runs.
    property QtObject dragOverlay: null

    // `debug` is the component's `property Item debug` slot, read by the restore-zoom
    // Timer onTriggered (debug.timersEnabled). It must be an Item (a QtObject fails the
    // initial-property type-check and silently stays null), not a catch-all.
    Item {
        id: debug
        property bool timersEnabled: false
    }

    // `animations` and `settings` are declared slots on the component (Item/QtObject)
    // that this file never dereferences itself; supply benign mocks so they're non-null
    // and shaped like the real abilities rather than catch-alls.
    Item { id: animations }
    QtObject { id: settings }

    // `layouts` is the component's `property Item layouts` slot. It feeds the
    // restoreZoomIsBlockedFromApplet Binding (walks startLayout/mainLayout/endLayout
    // .children) and is the onContextMenuIsShownChanged Connections target. It must be an
    // Item, not a QtObject (the typed slot rejects a QtObject and leaves layouts null,
    // which makes the Binding throw and the Connections target null). Empty grids -> the
    // binding resolves to false; contextMenuIsShown's auto change signal drives the handler.
    Item {
        id: layouts
        property Item startLayout: Item {}
        property Item mainLayout: Item {}
        property Item endLayout: Item {}
        property bool contextMenuIsShown: false
    }

    // `view` mock. The onContainsMouseChanged Connections targets view.visibility, so it
    // needs a containsMouse bool plus the change signal. setCurrentParabolicItem writes
    // view.parabolic.currentItem, so view.parabolic must be a writable object.
    // containsMouse carries its own auto-generated change signal; assigning it drives
    // the onContainsMouseChanged Connections handler.
    QtObject {
        id: visibilityObj
        property bool containsMouse: true
    }
    QtObject {
        id: viewParabolicObj
        property var currentItem: null
    }
    QtObject {
        id: view
        property QtObject visibility: visibilityObj
        property QtObject parabolic: viewParabolicObj
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {
            animations: animations,
            debug: debug,
            layouts: layouts,
            view: view,
            settings: settings
        });
        verify(obj, "instantiate failed");
        return obj;
    }

    // The 50ms restore timer lives in `resources` (non-visual). Find it by interval so
    // we can read its running state and shrink it when we want it to fire.
    function restoreTimer(m) {
        const res = m.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].interval === 50)
                return res[i];
        }
        return null;
    }

    // startRestoreZoomTimer: starts the restore timer when restoreZoomIsBlocked is
    // false, and early-returns (no start) when it is blocked. stopRestoreZoomTimer
    // stops a running timer. Drive both directly and observe timer.running.
    function test_start_stop_restoreZoomTimer() {
        const m = make();
        const t = restoreTimer(m);
        verify(t, "could not find restoreZoomTimer");

        // blocked -> startRestoreZoomTimer early-returns, timer stays stopped.
        t.stop();
        m.restoreZoomIsBlocked = true;
        m.startRestoreZoomTimer();
        compare(t.running, false);

        // not blocked -> timer starts.
        m.restoreZoomIsBlocked = false;
        m.startRestoreZoomTimer();
        compare(t.running, true);

        // stopRestoreZoomTimer stops the running timer.
        m.stopRestoreZoomTimer();
        compare(t.running, false);

        // stopRestoreZoomTimer over an already-stopped timer is a no-op (guard path).
        m.stopRestoreZoomTimer();
        compare(t.running, false);
    }

    // setDirectRenderingEnabled writes _privates.directRenderingEnabled, observable via
    // the inherited readonly directRenderingEnabled alias.
    function test_setDirectRenderingEnabled() {
        const m = make();
        m.setDirectRenderingEnabled(true);
        compare(m.directRenderingEnabled, true);
        m.setDirectRenderingEnabled(false);
        compare(m.directRenderingEnabled, false);
    }

    // setCurrentParabolicItem forwards into view.parabolic.currentItem.
    function test_setCurrentParabolicItem() {
        const m = make();
        viewParabolicObj.currentItem = null;
        const marker = animations; // any retained Item works as the stored value
        m.setCurrentParabolicItem(marker);
        compare(viewParabolicObj.currentItem, marker);
    }

    // setCurrentParabolicItemIndex records lastParabolicItemIndex every call, and on a
    // rapid jump (>=2 indices, direct-rendering off, both indices valid) flips
    // direct-rendering on via setDirectRenderingEnabled.
    function test_setCurrentParabolicItemIndex() {
        const m = make();
        m.setDirectRenderingEnabled(false);
        m.lastParabolicItemIndex = -1;

        // first call: lastIndex was -1 so the rapid-movement guard is skipped, index recorded.
        m.setCurrentParabolicItemIndex(0);
        compare(m.lastParabolicItemIndex, 0);
        compare(m.directRenderingEnabled, false);

        // small step (0 -> 1): abs diff 1 < 2, no direct rendering.
        m.setCurrentParabolicItemIndex(1);
        compare(m.lastParabolicItemIndex, 1);
        compare(m.directRenderingEnabled, false);

        // rapid jump (1 -> 5): abs diff 4 >= 2 -> direct rendering switches on.
        m.setCurrentParabolicItemIndex(5);
        compare(m.lastParabolicItemIndex, 5);
        compare(m.directRenderingEnabled, true);
    }

    // onRestoreZoomIsBlockedChanged: clearing the blocked flag starts the restore timer
    // (via startRestoreZoomTimer); setting it stops the timer (via stopRestoreZoomTimer).
    function test_onRestoreZoomIsBlockedChanged() {
        const m = make();
        const t = restoreTimer(m);
        verify(t, "could not find restoreZoomTimer");
        t.stop();

        // false -> true transition: handler calls stopRestoreZoomTimer (timer already
        // stopped, stays stopped) and exercises the blocked branch.
        m.restoreZoomIsBlocked = true;
        compare(t.running, false);

        // true -> false transition: handler calls startRestoreZoomTimer -> timer runs.
        m.restoreZoomIsBlocked = false;
        compare(t.running, true);
    }

    // onCurrentParabolicItemChanged: a null current item starts the restore timer; a
    // non-null one stops it.
    function test_onCurrentParabolicItemChanged() {
        const m = make();
        const t = restoreTimer(m);
        verify(t, "could not find restoreZoomTimer");

        // ensure not blocked so startRestoreZoomTimer actually starts.
        m.restoreZoomIsBlocked = false;
        t.stop();

        // set a non-null current item -> handler stops the timer.
        m.currentParabolicItem = animations;
        compare(t.running, false);

        // clear it -> handler starts the timer.
        m.currentParabolicItem = null;
        compare(t.running, true);
    }

    // onContainsMouseChanged (Connections target view.visibility): when the mouse leaves
    // (containsMouse false) and the timer isn't running, the handler starts it. Drive the
    // real containsMouseChanged signal so the mock side-effect proves the body ran.
    function test_onContainsMouseChanged() {
        const m = make();
        const t = restoreTimer(m);
        verify(t, "could not find restoreZoomTimer");
        m.restoreZoomIsBlocked = false;
        visibilityObj.containsMouse = true;
        t.stop();

        // mouse leaves (true -> false) + timer stopped -> handler starts the restore timer.
        visibilityObj.containsMouse = false;
        compare(t.running, true);

        // mouse re-enters (false -> true) -> handler takes the no-op branch, timer left running.
        visibilityObj.containsMouse = true;
        compare(t.running, true);
    }

    // onContextMenuIsShownChanged (Connections target layouts): when the context menu
    // closes and the timer isn't running, the handler starts it.
    function test_onContextMenuIsShownChanged() {
        const m = make();
        const t = restoreTimer(m);
        verify(t, "could not find restoreZoomTimer");
        m.restoreZoomIsBlocked = false;
        layouts.contextMenuIsShown = false;
        t.stop();

        // menu opens (false -> true): handler guard (!contextMenuIsShown) is false, no start.
        layouts.contextMenuIsShown = true;
        compare(t.running, false);

        // menu closes (true -> false) + timer stopped -> handler starts the restore timer.
        layouts.contextMenuIsShown = false;
        compare(t.running, true);
    }

    // restoreZoomTimer onTriggered (full body, not the early return): with the zoom not
    // blocked and no current item, it resets lastParabolicItemIndex, turns direct
    // rendering off, and emits sglClearZoom. Shrink the interval and assert the signal
    // fired plus the side effects.
    function test_restoreZoomTimer_triggered() {
        const m = make();
        const t = restoreTimer(m);
        verify(t, "could not find restoreZoomTimer");

        m.restoreZoomIsBlocked = false;
        m.currentParabolicItem = null;
        m.setDirectRenderingEnabled(true);
        m.lastParabolicItemIndex = 7;
        debug.timersEnabled = false;

        const spy = createTemporaryObject(spyComp, root,
                                          {target: m, signalName: "sglClearZoom"});
        verify(spy, "spy instantiate failed");

        t.stop();
        t.interval = 1;
        t.restart();

        tryVerify(function() { return spy.count >= 1; }, 2000,
                  "restore timer never emitted sglClearZoom");
        compare(m.lastParabolicItemIndex, -1);
        compare(m.directRenderingEnabled, false);
    }

    // restoreZoomTimer onTriggered early-return branch: when a current item is present
    // the handler returns immediately — no sglClearZoom, lastParabolicItemIndex kept.
    function test_restoreZoomTimer_triggered_earlyReturn() {
        const m = make();
        const t = restoreTimer(m);
        verify(t, "could not find restoreZoomTimer");

        m.restoreZoomIsBlocked = false;
        m.currentParabolicItem = animations; // present -> early return
        m.setDirectRenderingEnabled(true);
        m.lastParabolicItemIndex = 7;

        const spy = createTemporaryObject(spyComp, root,
                                          {target: m, signalName: "sglClearZoom"});
        verify(spy, "spy instantiate failed");

        t.stop();
        t.interval = 1;
        t.restart();

        // Give the timer time to fire; the early-return path leaves everything untouched.
        wait(200);
        compare(spy.count, 0);
        compare(m.lastParabolicItemIndex, 7);
        compare(m.directRenderingEnabled, true);
    }

    Component {
        id: spyComp
        SignalSpy {}
    }
}
