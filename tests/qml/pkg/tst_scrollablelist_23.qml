// Drives the tasks ScrollableList Flickable through its scroll functions
// (increase/decreasePos, increase/decreasePosWithStep), the focus and autoscroll
// helpers, the contentsExtraSpace clamp handler, and the alignment state machine.
// Loaded from the staged (instrumented) package by file URL so every executed
// function fires a Cov tick.
//
// ScrollableList reads UNQUALIFIED context names (root, appletAbilities,
// scrollableList) plus the Plasmoid singleton and LatteCore.Types. Those resolve
// against the component's creation context, so we build a wrapper whose ids ARE
// `root`/`appletAbilities`/`scrollableList` and load the staged file through a
// Loader inside that wrapper: a Loader-instantiated item resolves unqualified
// names against the wrapper's context, satisfying ScrollableList's lookups.
//
// Every test asserts an observable effect: the scroll functions all converge
// contentX/contentY onto a value we compute from the function's own math, the
// early-return branches assert the position is left untouched, and the alignment
// machine is checked against the expected LatteCore.Types enum per edge/align.
import QtQuick
import QtTest

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.latte.core 0.2 as LatteCore

TestCase {
    id: tc
    name: "ScrollableList"
    when: windowShown
    visible: true
    width: 400
    height: 400

    readonly property string targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/taskslayout/ScrollableList.qml")

    // The wrapper QML: declares the unqualified ids ScrollableList expects and
    // hosts it through a Loader. Sizing/sibling tasks live here too.
    readonly property string wrapperQml:
        'import QtQuick\n' +
        'import org.kde.plasma.core 2.0 as PlasmaCore\n' +
        'import org.kde.latte.core 0.2 as LatteCore\n' +
        'Item {\n' +
        '    id: scrollableList\n' +
        '    width: 200; height: 60\n' +
        '    property QtObject root: QtObject {\n' +
        '        property bool vertical: false\n' +
        '        property bool scrollingEnabled: true\n' +
        '        property bool autoScrollTasksEnabled: true\n' +
        '        property int tasksCount: 6\n' +
        '        property real tasksLength: 600\n' +
        '        property int location: PlasmaCore.Types.BottomEdge\n' +
        '        property int alignment: LatteCore.Types.Center\n' +
        '    }\n' +
        '    property QtObject appletAbilities: QtObject {\n' +
        '        property QtObject metrics: QtObject {\n' +
        '            property real iconSize: 48\n' +
        '            property QtObject totals: QtObject { property real length: 50; property real lengthEdge: 4 }\n' +
        '        }\n' +
        '        property QtObject animations: QtObject {\n' +
        '            property QtObject speedFactor: QtObject { property real current: 1.0 }\n' +
        '            property QtObject duration: QtObject { property real large: 250 }\n' +
        '        }\n' +
        '        property QtObject indexer: QtObject { property int lastVisibleItemIndex: 4 }\n' +
        '        property QtObject parabolic: QtObject {\n' +
        '            property QtObject factor: QtObject { property real zoom: 1.0 }\n' +
        '        }\n' +
        '    }\n' +
        '    // sibling fake tasks the focus/autoscroll helpers map against\n' +
        '    property Item nearTask: Item { x: -20; y: -20; width: 40; height: 40; property int itemIndex: 1 }\n' +
        '    property Item farTask: Item { x: 500; y: 500; width: 40; height: 40; property int itemIndex: 2 }\n' +
        '    property Item list: null\n' +
        '    property real lastMargin: 0\n' +
        '    Loader {\n' +
        '        id: ld\n' +
        '        anchors.fill: parent\n' +
        '        onLoaded: { scrollableList.list = item; item.contentWidth = 600; item.contentHeight = 600; }\n' +
        '    }\n' +
        '    function load(u) { ld.setSource(u); }\n' +
        '}\n'

    property var wrapper: null

    function makeWrapper() {
        const w = Qt.createQmlObject(wrapperQml, tc, "ScrollableListWrapper");
        verify(w, "wrapper creation failed");
        w.load(targetUrl);
        // Loader is synchronous by default; the list should be live now.
        verify(w.list, "ScrollableList did not load: see warnings");
        return w;
    }

    // shorthand: returns the live ScrollableList item from a fresh wrapper
    function makeList() {
        const w = makeWrapper();
        tc.wrapper = w;
        return w.list;
    }

    function settle(f) {
        tryVerify(function(){ return f.animationsFinished; }, 4000, "scroll animation did not settle");
    }

    function cleanup() {
        if (tc.wrapper) { tc.wrapper.destroy(); tc.wrapper = null; }
    }

    function test_01_instantiate_and_derived_props() {
        const f = makeList();
        verify(f, "no object");
        compare(f.scrollFirstPos, 0);
        // contentsExceed: scrollingEnabled && floor(tasksLength 600) > length(0)
        verify(f.contentsExceed === true);
        // scrollStep = metrics.totals.length(50) * 3.5 = 175
        compare(f.scrollStep, 175);
        // autoScrollTriggerLength = iconSize(48) + lengthEdge(4) = 52
        compare(f.autoScrollTriggerLength, 52);
        compare(f.currentPos, 0);
        // contentsExtraSpace = tasksLength(600) - length(0) = 600
        compare(f.contentsExtraSpace, 600);
        compare(f.scrollLastPos, 600);
    }

    // increasePos/decreasePos step by scrollStep (175) and clamp at the bounds.
    function test_02_increase_decrease_pos() {
        const f = makeList();
        // Each increasePos reads the (still moving) contentX, so settle between
        // steps. Six 175-steps over a 600-span reach and clamp at scrollLastPos.
        f.increasePos();
        tryVerify(function(){ return f.contentX > 0; }, 2000, "increasePos did not move contentX");
        for (var i = 0; i < 5; i++) { f.increasePos(); settle(f); }
        compare(f.contentX, f.scrollLastPos);   // clamped at last (600)
        for (var j = 0; j < 6; j++) { f.decreasePos(); settle(f); }
        compare(f.contentX, f.scrollFirstPos);  // clamped at 0
    }

    // increasePosWithStep/decreasePosWithStep on the vertical axis: each call
    // sets contentY = clamp(contentY ± step). Assert the converged contentY
    // against the value the function math produces.
    function test_03_step_helpers_vertical() {
        const w = makeWrapper(); tc.wrapper = w;
        w.root.vertical = true;
        const f = w.list;
        const last = f.scrollLastPos;

        f.increasePosWithStep(120); settle(f);
        compare(f.contentY, Math.min(last, 120));      // 120
        f.increasePosWithStep(40); settle(f);
        compare(f.contentY, Math.min(last, 160));      // 160
        f.decreasePosWithStep(50); settle(f);
        compare(f.contentY, Math.max(0, 110));         // 110
        f.decreasePosWithStep(500); settle(f);
        compare(f.contentY, f.scrollFirstPos);         // clamped at 0
        // horizontal axis must be untouched while vertical
        compare(f.contentX, 0);
    }

    // focusOn horizontal: a task left of the viewport (cP.x < 0) drives the
    // decrease branch, a task far right drives the increase branch. Assert the
    // direction of contentX movement for each.
    function test_04_focusOn_horizontal() {
        const w = makeWrapper(); tc.wrapper = w;
        const f = w.list;

        // Seed mid-range so the decrease branch has room to move down.
        f.contentX = 300; settle(f);
        f.focusOn(w.nearTask);   // cP.x < 0 -> decreasePosWithStep
        settle(f);
        verify(f.contentX < 300, "near task should scroll content back: " + f.contentX);

        const before = f.contentX;
        f.focusOn(w.farTask);    // far right -> increasePosWithStep
        settle(f);
        verify(f.contentX > before, "far task should scroll content forward: " + f.contentX);
    }

    // focusOn vertical: same two branches on the Y axis.
    function test_05_focusOn_vertical() {
        const w = makeWrapper(); tc.wrapper = w;
        w.root.vertical = true;
        const f = w.list;

        f.contentY = 300; settle(f);
        f.focusOn(w.nearTask);   // cP.y < 0 -> decrease branch
        settle(f);
        verify(f.contentY < 300, "near task should scroll content up: " + f.contentY);

        const before = f.contentY;
        f.focusOn(w.farTask);    // far bottom -> increase branch
        settle(f);
        verify(f.contentY > before, "far task should scroll content down: " + f.contentY);
    }

    // focusOn early-return when !contentsExceed: position must be left untouched.
    function test_06_focusOn_noExceed_earlyreturn() {
        const w = makeWrapper(); tc.wrapper = w;
        w.root.scrollingEnabled = false;   // contentsExceed false -> early return
        const f = w.list;
        verify(f.contentsExceed === false);
        f.contentX = 42; settle(f);
        f.focusOn(w.nearTask);
        f.focusOn(w.farTask);
        compare(f.contentX, 42);   // untouched by the early return
    }

    // autoScrollFor horizontal: near-start task triggers decrease, near-end task
    // (during dragging) triggers increase. Assert contentX direction per branch.
    function test_07_autoScrollFor_horizontal() {
        const w = makeWrapper(); tc.wrapper = w;
        const f = w.list;

        // currentPos !== scrollFirstPos so the near-start branch is allowed to fire.
        f.contentX = 200; settle(f);
        f.autoScrollFor(w.nearTask, false);   // cP.x < triggerLength -> decrease
        settle(f);
        verify(f.contentX < 200, "near-start autoscroll should decrease: " + f.contentX);

        const before = f.contentX;
        f.autoScrollFor(w.farTask, true);     // near-end during dragging -> increase
        settle(f);
        verify(f.contentX > before, "near-end autoscroll should increase: " + f.contentX);
    }

    // autoScrollFor vertical: same two branches on the Y axis.
    function test_08_autoScrollFor_vertical() {
        const w = makeWrapper(); tc.wrapper = w;
        w.root.vertical = true;
        const f = w.list;

        f.contentY = 200; settle(f);
        f.autoScrollFor(w.nearTask, true);    // near-start -> decrease
        settle(f);
        verify(f.contentY < 200, "near-start autoscroll should decrease: " + f.contentY);

        const before = f.contentY;
        f.autoScrollFor(w.farTask, false);    // near-end -> increase
        settle(f);
        verify(f.contentY > before, "near-end autoscroll should increase: " + f.contentY);
    }

    // autoScrollFor early-return guards: blocked (autoscroll off, not dragging)
    // and too-few-tasks both leave the position untouched.
    function test_09_autoScrollFor_blocked_and_fewtasks() {
        const w = makeWrapper(); tc.wrapper = w;
        const f = w.list;
        f.contentX = 200; settle(f);

        // block = !autoScrollTasksEnabled && !duringDragging -> early return
        w.root.autoScrollTasksEnabled = false;
        f.autoScrollFor(w.nearTask, false);
        settle(f);
        compare(f.contentX, 200);   // untouched

        // tasksCount < 3 -> early return
        w.root.autoScrollTasksEnabled = true;
        w.root.tasksCount = 2;
        f.autoScrollFor(w.nearTask, true);
        settle(f);
        compare(f.contentX, 200);   // still untouched
    }

    // autoScrollFor early-return for the last visible task under parabolic zoom.
    function test_10_autoScrollFor_lastVisible_parabolic() {
        const w = makeWrapper(); tc.wrapper = w;
        const f = w.list;
        f.contentX = 200; settle(f);

        // task.itemIndex === lastVisibleItemIndex && parabolic.zoom > 1 -> early return
        w.appletAbilities.parabolic.factor.zoom = 1.6;
        w.nearTask.itemIndex = w.appletAbilities.indexer.lastVisibleItemIndex;
        f.autoScrollFor(w.nearTask, true);
        settle(f);
        compare(f.contentX, 200);   // untouched by the early return
    }

    // NOTE on onContentsExtraSpaceChanged (the clamp handler): it runs its body
    // once at construction (when contentsExtraSpace first resolves to 600) — that
    // fire is asserted in test_01 via the contentsExtraSpace/scrollLastPos values
    // it gates on. Its actual clamp side-effect (re-firing on a later span change
    // and pulling contentX back inside bounds) does NOT reproduce headlessly: with
    // the QtObject-wrapped context the readonly contentsExtraSpace re-evaluates but
    // its change signal never re-invokes the handler, so contentX is left at the
    // out-of-bounds value. Asserting the clamp needs the real Binding-driven
    // length/tasksLength of a live dock, so that branch is left to a live test.

    // ----- alignment state machine: walk every edge/align combination ---------
    function test_11_alignment_all_edges() {
        const w = makeWrapper(); tc.wrapper = w;
        const f = w.list;
        const r = w.root;

        r.alignment = LatteCore.Types.Center;
        r.location = PlasmaCore.Types.LeftEdge;
        compare(f.alignment, LatteCore.Types.LeftEdgeCenterAlign);
        r.location = PlasmaCore.Types.RightEdge;
        compare(f.alignment, LatteCore.Types.RightEdgeCenterAlign);
        r.location = PlasmaCore.Types.BottomEdge;
        compare(f.alignment, LatteCore.Types.BottomEdgeCenterAlign);
        r.location = PlasmaCore.Types.TopEdge;
        compare(f.alignment, LatteCore.Types.TopEdgeCenterAlign);

        r.location = PlasmaCore.Types.LeftEdge;
        r.alignment = LatteCore.Types.Top;
        compare(f.alignment, LatteCore.Types.LeftEdgeTopAlign);
        r.alignment = LatteCore.Types.Bottom;
        compare(f.alignment, LatteCore.Types.LeftEdgeBottomAlign);

        r.location = PlasmaCore.Types.RightEdge;
        r.alignment = LatteCore.Types.Top;
        compare(f.alignment, LatteCore.Types.RightEdgeTopAlign);
        r.alignment = LatteCore.Types.Bottom;
        compare(f.alignment, LatteCore.Types.RightEdgeBottomAlign);

        r.location = PlasmaCore.Types.BottomEdge;
        r.alignment = LatteCore.Types.Left;
        compare(f.alignment, LatteCore.Types.BottomEdgeLeftAlign);
        r.alignment = LatteCore.Types.Right;
        compare(f.alignment, LatteCore.Types.BottomEdgeRightAlign);

        r.location = PlasmaCore.Types.TopEdge;
        r.alignment = LatteCore.Types.Left;
        compare(f.alignment, LatteCore.Types.TopEdgeLeftAlign);
        r.alignment = LatteCore.Types.Right;
        compare(f.alignment, LatteCore.Types.TopEdgeRightAlign);
    }

    // Derived boolean bindings reflect their inputs.
    function test_12_animationsFinished_centered_reversed() {
        const w = makeWrapper(); tc.wrapper = w;
        const f = w.list;
        // centered tracks root.alignment === Center
        w.root.alignment = LatteCore.Types.Center;
        verify(f.centered === true, "centered should follow Center alignment");
        w.root.alignment = LatteCore.Types.Left;
        verify(f.centered === false, "centered should clear off Center alignment");
        // reversed tracks the application layout direction (LTR here)
        compare(f.reversed, Qt.application.layoutDirection === Qt.RightToLeft);
        // no animation pending on a freshly settled list
        settle(f);
        compare(f.animationsFinished, true);
    }
}
