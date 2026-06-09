// Coverage for the containment's AutoSize ability. AutoSize reads a swarm of
// unqualified context names (root, latteView, parabolic, animations,
// visibilityManager, Plasmoid). QML resolves those against the component's
// creation context, so we name this TestCase `id: root` and declare every name
// the component touches as a property / lowercase-`id`'d mock here. With those
// in place the prediction-history helpers, updateIconSize()'s restore branch,
// the auto-size-animation handler, the doubler Timer, and the Connections
// handlers all run for real and assert observable effects.
//
// Live-only: the shrink/grow math inside updateIconSize() needs `isActive`
// truthy, which requires `Plasmoid.configuration.autoSizeEnabled` — the attached
// Plasmoid singleton has no live containment headlessly, so that dereference
// can't be satisfied. We cover updateIconSize()'s entry + restore branch (which
// don't need isActive) and leave the grow/shrink body to a live dock.
import QtQuick
import QtTest
import org.kde.latte.core 0.2 as LatteCore
import org.kde.plasma.plasmoid 2.0

TestCase {
    id: root
    name: "AutoSize11"
    when: windowShown
    visible: true
    width: 50
    height: 50

    // ---- bare context names the component reads unqualified ----
    // isActive short-circuits on behaveAsDockWithMask, so leaving it false keeps
    // the throwing Plasmoid.configuration dereference out of the binding.
    property bool behaveAsDockWithMask: false
    property bool containsOnlyPlasmaTasks: false
    property bool inConfigureAppletsMode: false
    property bool isHorizontal: true
    property bool isVertical: false
    property int maxLength: 1000

    // The first Connections block listens to containsOnlyPlasmaTasksChanged /
    // maxLengthChanged; those are the auto-generated change signals of the
    // properties above, so changing the property fires the handler.

    // animations.needBothAxis.addEvent/removeEvent are called from
    // onInAutoSizeAnimationChanged. Count the calls so we can assert the handler
    // body actually ran (not just that the entry tick fired).
    property int _addEventCalls: 0
    property int _removeEventCalls: 0
    property var _lastAddEventArg: null
    QtObject {
        id: animations
        property QtObject needBothAxis: QtObject {
            function addEvent(o) { root._addEventCalls++; root._lastAddEventArg = o; }
            function removeEvent(o) { root._removeEventCalls++; }
        }
    }

    QtObject {
        id: parabolic
        property QtObject factor: QtObject { property real zoom: 1.0 }
    }

    // visibilityManager owns the inNormalState signal the last Connections block
    // listens to; the handler calls sizer.updateIconSize() when inNormalState.
    QtObject {
        id: visibilityManager
        property bool inNormalState: true
    }

    QtObject {
        id: positionerObj
        property bool isOffScreen: false
    }

    // latteView feeds isActive's visibility checks plus the width/height/offscreen
    // Connections. It must expose width/height as notifying properties so the
    // onWidthChanged/onHeightChanged handlers fire.
    QtObject {
        id: latteView
        property real width: 500
        property real height: 44
        property QtObject visibility: QtObject {
            property int mode: LatteCore.Types.DodgeActive
        }
        property QtObject positioner: positionerObj
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/AutoSize.qml")

    // Mock for the declared `property Item metrics`. inCalculatedIconSize reads
    // iconSize/maxIconSize; updateIconSize reads totals.length; the metrics
    // Connections fires on portionIconSize. Mutable so we can flip values.
    Component {
        id: metricsMock
        Item {
            property int iconSize: 48
            property int maxIconSize: 48
            property int portionIconSize: -1
            property QtObject totals: QtObject { property int length: 100 }
        }
    }

    // Mock for `property Item visibility` - updateIconSize reads
    // inRelocationHiding/inNormalState off it.
    Component {
        id: visibilityMock
        Item {
            property bool inRelocationHiding: false
            property bool inNormalState: true
        }
    }

    // Mock for `property Item layouts` - the Justify branch sums the three
    // layout lengths; the normal branch reads mainLayout.length.
    Component {
        id: layoutsMock
        Item {
            property QtObject startLayout: QtObject { property int length: 10 }
            property QtObject mainLayout: QtObject { property int length: 80 }
            property QtObject endLayout: QtObject { property int length: 10 }
        }
    }

    // Mock for `property Item layouter` - isActive reads fillApplets.
    Component {
        id: layouterMock
        Item { property int fillApplets: 0 }
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {
            metrics: metricsMock.createObject(root),
            visibility: visibilityMock.createObject(root),
            layouts: layoutsMock.createObject(root),
            layouter: layouterMock.createObject(root)
        });
        verify(obj, "instantiate failed");
        return obj;
    }

    // clearHistory just zeroes the array length.
    function test_clearHistory() {
        const s = make();
        s.addPrediction(100, 90);
        verify(s.history.length > 0);
        s.clearHistory();
        compare(s.history.length, 0);
    }

    // addPrediction unshifts, then splices once the array grows past
    // historyMaxSize (10), trimming back toward historyMinSize (4).
    function test_addPrediction_grows_and_splices() {
        const s = make();
        s.clearHistory();
        for (var i = 0; i < 12; ++i) {
            s.addPrediction(100 + i, 90 + i);
        }
        verify(s.history.length <= s.historyMaxSize, "history must be trimmed below max");
        verify(s.history.length > 0, "history must keep recent entries");
        // The most recent unshifted entry sits at index 0.
        compare(s.history[0].current, 111);
        compare(s.history[0].predicted, 101);
    }

    // producesEndlessLoop: short-history early-return, the no-match path, and the
    // shrink-loop-detected path that returns true.
    function test_producesEndlessLoop_paths() {
        const s = make();

        // Fewer than 2 entries -> false immediately.
        s.clearHistory();
        compare(s.producesEndlessLoop(100, 90), false);

        // Two entries that don't match the query -> false (passes the length
        // guard, exercises the comparison branch).
        s.clearHistory();
        s.addPrediction(50, 60); // becomes history[1] after the next unshift
        s.addPrediction(70, 80); // history[0]
        compare(s.producesEndlessLoop(999, 888), false);

        // Shrink-loop signature: history[0].current > predicted (shrink at the
        // newest step) and history[1] is a grow step matching the query -> true.
        s.clearHistory();
        s.addPrediction(100, 120); // history[1]: current < predicted (grow)
        s.addPrediction(200, 150); // history[0]: current > predicted (shrink)
        compare(s.producesEndlessLoop(100, 120), true);
    }

    // Flip the metrics mock so inCalculatedIconSize goes false -> inAutoSizeAnimation
    // true, firing onInAutoSizeAnimationChanged. With `animations` mocked the
    // handler body runs for real: assert it called needBothAxis.addEvent(sizer),
    // then flip back and assert removeEvent.
    function test_inAutoSizeAnimationChanged_addRemove() {
        const s = make();
        verify(s.inCalculatedIconSize, "starts in calculated size");
        verify(!s.inAutoSizeAnimation, "so not in animation");
        const addBase = root._addEventCalls;
        const removeBase = root._removeEventCalls;

        // Break both equalities (iconSize 48, maxIconSize 64, sizer.iconSize -1)
        // -> inCalculatedIconSize false -> inAutoSizeAnimation true.
        s.metrics.maxIconSize = 64;
        tryVerify(function() { return s.inAutoSizeAnimation === true; }, 2000,
                  "inAutoSizeAnimation should flip true after metrics change");
        compare(root._addEventCalls, addBase + 1);
        // The handler passes the sizer itself to addEvent.
        compare(root._lastAddEventArg, s);

        // Restore the match -> inAutoSizeAnimation false -> removeEvent branch.
        s.metrics.maxIconSize = 48;
        tryVerify(function() { return s.inAutoSizeAnimation === false; }, 2000,
                  "inAutoSizeAnimation should flip back false");
        compare(root._removeEventCalls, removeBase + 1);
    }

    // updateIconSize()'s restore branch: when isActive is false and iconSize is
    // not the -1 default, the function resets iconSize to -1. isActive is false
    // here (behaveAsDockWithMask is false), so a direct call must reset it.
    function test_updateIconSize_restoresDefault() {
        const s = make();
        verify(!s.isActive, "isActive should be false with behaveAsDockWithMask false");
        s.iconSize = 30;            // pretend an earlier sizing run set it
        s.updateIconSize();
        compare(s.iconSize, -1);    // restore branch ran
        // Idempotent: already -1, the restore branch is skipped, stays -1.
        s.updateIconSize();
        compare(s.iconSize, -1);
    }

    // The metrics Connections onPortionIconSizeChanged fires when portionIconSize
    // changes to something other than -1, calling updateIconSize. Drive it with a
    // non-default iconSize so the resulting updateIconSize's restore branch leaves
    // an observable mark (iconSize back to -1).
    function test_metricsPortionChange_drivesUpdate() {
        const s = make();
        s.iconSize = 24;
        compare(s.metrics.portionIconSize, -1);
        s.metrics.portionIconSize = 42;   // != -1 -> handler calls updateIconSize
        tryVerify(function() { return s.iconSize === -1; }, 2000,
                  "portion change should drive updateIconSize restore");
    }

    // root.onMaxLengthChanged Connections: handler calls updateIconSize when the
    // positioner is on-screen. Same observable: a non-default iconSize is reset.
    function test_maxLengthChange_drivesUpdate() {
        const s = make();
        s.iconSize = 22;
        positionerObj.isOffScreen = false;
        root.maxLength = root.maxLength + 1;   // changing the property fires the handler
        tryVerify(function() { return s.iconSize === -1; }, 2000,
                  "maxLength change should drive updateIconSize restore");
    }

    // root.onContainsOnlyPlasmaTasksChanged Connections calls updateIconSize directly.
    function test_containsOnlyPlasmaTasksChange_drivesUpdate() {
        const s = make();
        s.iconSize = 36;
        root.containsOnlyPlasmaTasks = !root.containsOnlyPlasmaTasks;
        tryVerify(function() { return s.iconSize === -1; }, 2000,
                  "containsOnlyPlasmaTasks change should drive updateIconSize");
    }

    // latteView width/height Connections: onWidthChanged fires when horizontal and
    // portionIconSize != -1; onHeightChanged when vertical. Each calls updateIconSize.
    function test_latteViewSizeChange_drivesUpdate() {
        const s = make();
        s.metrics.portionIconSize = 7;    // must be != -1 for the handlers to act

        // Horizontal width path.
        root.isHorizontal = true;
        root.isVertical = false;
        s.iconSize = 40;
        latteView.width = 520;
        tryVerify(function() { return s.iconSize === -1; }, 2000,
                  "width change should drive updateIconSize when horizontal");

        // Vertical height path.
        root.isHorizontal = false;
        root.isVertical = true;
        s.iconSize = 41;
        latteView.height = 60;
        tryVerify(function() { return s.iconSize === -1; }, 2000,
                  "height change should drive updateIconSize when vertical");

        root.isHorizontal = true;
        root.isVertical = false;
    }

    // positioner.onIsOffScreenChanged Connections calls updateIconSize when the
    // dock comes back on-screen (the !isOffScreen branch).
    function test_isOffScreenChange_drivesUpdate() {
        const s = make();
        positionerObj.isOffScreen = true;    // start off-screen
        s.iconSize = 33;
        positionerObj.isOffScreen = false;   // back on-screen -> updateIconSize
        tryVerify(function() { return s.iconSize === -1; }, 2000,
                  "off-screen change should drive updateIconSize");
    }

    // visibilityManager.onInNormalStateChanged Connections calls updateIconSize
    // when inNormalState becomes true.
    function test_inNormalStateChange_drivesUpdate() {
        const s = make();
        visibilityManager.inNormalState = false;   // start out of normal state
        s.iconSize = 28;
        visibilityManager.inNormalState = true;    // -> updateIconSize
        tryVerify(function() { return s.iconSize === -1; }, 2000,
                  "normal-state change should drive updateIconSize");
    }

    // The doubler Timer's onTriggered, with secondTimeCallApplied false, sets the
    // flag and calls updateIconSize. Shrink the interval, restart, and assert the
    // flag flipped (set before updateIconSize runs).
    function test_doublerTimer_triggers() {
        const s = make();
        const t = findDoublerTimer(s);
        verify(t, "could not find doubleCallAutomaticUpdateIconSize timer");
        t.secondTimeCallApplied = false;
        t.interval = 1;
        t.restart();
        tryVerify(function() { return t.secondTimeCallApplied === true; }, 2000,
                  "doubler timer onTriggered did not set secondTimeCallApplied");
    }

    function findDoublerTimer(s) {
        const res = s.resources;
        for (var i = 0; i < res.length; i++) {
            // The only Timer in the file carries this property.
            if (res[i] && res[i].hasOwnProperty("secondTimeCallApplied"))
                return res[i];
        }
        return null;
    }
}
