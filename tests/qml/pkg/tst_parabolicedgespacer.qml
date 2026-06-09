// Coverage for the containment's ParabolicEdgeSpacer. The component is an
// anonymous Item that reads a handful of unqualified context names
// (animations, parabolic, metrics, myView, debug) plus parent.beginIndex.
// QML resolves the unqualified names against the creation context, i.e. this
// test file's root object, so we name the TestCase `root` and declare every
// name the component touches as a property / lowercase-id'd QtObject here.
// parent.beginIndex is the spacer's *visual parent*, so each spacer is created
// inside a small wrapper Item that carries a mutable beginIndex; index<beginIndex
// makes a tail spacer, index>=beginIndex a head spacer.
//
// Every test drives a real instrumented unit and asserts an observable effect:
// the length the scale math writes, the re-emitted parabolic signal a slot
// forwards, or the restore animation draining length back to 0.
import QtQuick
import QtTest
import org.kde.latte.core 0.2 as LatteCore

TestCase {
    id: root
    name: "ParabolicEdgeSpacer"
    when: windowShown

    // ---- bare context names the component reads unqualified ----

    // animationTime binding = speedFactor.normal * 1.2 * duration.small.
    // Keep it tiny so sltClearZoom's restore animation finishes fast.
    QtObject {
        id: animations
        property QtObject speedFactor: QtObject { property real normal: 1.0 }
        property QtObject duration: QtObject { property int small: 10 }
    }

    // The parabolic ability: spread drives hiddenItemsCount, the two scale
    // signals + sglClearZoom are what Component.onCompleted connects to, and
    // directRenderingEnabled gates the length Behavior (true => instant writes,
    // so the updateScale assertions don't race an animation).
    QtObject {
        id: parabolic
        property int spread: 3            // hiddenItemsCount = (3-1)/2 = 1
        property bool directRenderingEnabled: true
        signal sglClearZoom()
        signal sglUpdateLowerItemScale(int delegateIndex, var newScales)
        signal sglUpdateHigherItemScale(int delegateIndex, var newScales)
        // Record every index seen on each signal so a test can assert the exact
        // side-neighbour-clear index the slot forwarded (index-1 for lower,
        // index+1 for higher) as distinct from the index the test drove with.
        property var lowerIndexes: []
        property var higherIndexes: []
        onSglUpdateLowerItemScale: (delegateIndex, newScales) => { lowerIndexes.push(delegateIndex); }
        onSglUpdateHigherItemScale: (delegateIndex, newScales) => { higherIndexes.push(delegateIndex); }
    }

    // updateScale multiplies the summed (scale-1) factor by metrics.totals.length.
    QtObject {
        id: metrics
        property QtObject totals: QtObject { property real length: 50 }
    }

    // myView.alignment selects the scale path (Center/Justify) vs the length=0
    // path (Left/Right). Center=0, Left=1, Right=2, Justify=10.
    QtObject {
        id: myView
        property int alignment: LatteCore.Types.Center
    }

    QtObject {
        id: debug
        property bool spacersEnabled: false
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/layouts/ParabolicEdgeSpacer.qml")

    // Each spacer lives in its own wrapper so parent.beginIndex is controllable
    // per spacer; index<beginIndex => tail, index>=beginIndex => head.
    Component {
        id: wrapperComponent
        Item { property int beginIndex: 0 }
    }

    function make(beginIndex, index) {
        const wrapper = createTemporaryObject(wrapperComponent, root, {beginIndex: beginIndex});
        verify(wrapper, "wrapper instantiate failed");
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, wrapper, {index: index});
        verify(obj, "instantiate failed");
        return obj;
    }

    // Component.onCompleted connects sltClearZoom/sltUpdateLowerItemScale/
    // sltUpdateHigherItemScale to the parabolic signals. We can only observe the
    // connect by emitting a signal and seeing the slot run; that is exactly what
    // the per-slot tests below do. This test pins the connect happened at all:
    // emitting sglClearZoom drains a non-zero length to 0 via restoreAnimation,
    // which proves both onCompleted (the connect) and sltClearZoom (the body).
    function test_onCompleted_and_clearZoom() {
        const m = make(0, 0);
        m.length = 80;
        compare(m.length, 80);
        // directRendering true => no length Behavior animation interferes with
        // the seed; the restore animation below is the only thing that moves it.
        parabolic.sglClearZoom();
        // restoreAnimation.start() animates length -> 0 over 4*animationTime.
        // animationTime = 1.0 * 1.2 * 10 = 12ms, so ~48ms; wait it out.
        tryVerify(function() { return m.length === 0; }, 3000,
                  "sltClearZoom did not drain length to 0 (onCompleted connect or restore animation failed)");
    }

    // Tail spacer, Center alignment: sltUpdateLowerItemScale -> updateScale.
    // hiddenItemsCount=1, newScales=[2.0] -> nextFactor=1.0, metrics.totals.length=50
    // -> length=50. The slot then forwards sglUpdateLowerItemScale(index-1,[1]).
    function test_lowerItemScale_tail_center_updatesLength() {
        const m = make(5, 4);            // index 4 < beginIndex 5 => tail
        verify(m.isTailSpacer);
        verify(!m.isHeadSpacer);
        myView.alignment = LatteCore.Types.Center;
        parabolic.lowerIndexes = [];

        parabolic.sglUpdateLowerItemScale(4, [2.0]);   // delegateIndex === index 4
        compare(m.length, 50);                          // updateScale wrote it
        // slot forwarded the side-neighbour clear at index-1 = 3 (distinct from
        // the driving index 4), so the forward is observable, not inferred.
        verify(parabolic.lowerIndexes.indexOf(3) !== -1,
               "slot did not forward sglUpdateLowerItemScale(index-1)");
    }

    // Tail spacer, Left alignment: the else-branch sets length=0 (no updateScale).
    function test_lowerItemScale_tail_left_zeroesLength() {
        const m = make(5, 0);
        myView.alignment = LatteCore.Types.Left;
        m.length = 33;
        compare(m.length, 33);
        parabolic.sglUpdateLowerItemScale(0, [2.0]);
        compare(m.length, 0);
    }

    // Guard branch: a tail spacer ignores a mismatched delegateIndex (no write).
    function test_lowerItemScale_mismatchedIndex_noop() {
        const m = make(5, 0);
        myView.alignment = LatteCore.Types.Center;
        m.length = 77;
        parabolic.lowerIndexes = [];
        parabolic.sglUpdateLowerItemScale(3, [2.0]);   // 3 !== index 0 -> early return
        compare(m.length, 77);                          // untouched
        // the only index recorded is the driving 3; no forwarded index-1 = -1.
        verify(parabolic.lowerIndexes.indexOf(-1) === -1, "early-return slot still forwarded");
    }

    // Head spacer, Justify alignment: sltUpdateHigherItemScale -> updateScale.
    // Justify takes the same scale path as Center -> length=50.
    function test_higherItemScale_head_center_updatesLength() {
        const m = make(0, 5);            // index 5 >= beginIndex 0 => head
        verify(m.isHeadSpacer);
        verify(!m.isTailSpacer);
        myView.alignment = LatteCore.Types.Justify;     // Justify also takes scale path
        parabolic.sglUpdateHigherItemScale(5, [2.0]);   // delegateIndex === index
        compare(m.length, 50);
    }

    // The head slot forwards sglUpdateHigherItemScale(index+1,[1]) to clear the
    // far-side neighbour. Drive with index 5; the forward lands on 6 (distinct
    // from the driving 5), so its presence in the recorded indexes proves the
    // forward really ran, not the driving emit echoing back.
    function test_higherItemScale_head_forwardsNeighbour() {
        const m = make(0, 5);            // index 5 >= beginIndex 0 => head
        verify(m.isHeadSpacer);
        myView.alignment = LatteCore.Types.Center;
        parabolic.higherIndexes = [];
        parabolic.sglUpdateHigherItemScale(5, [2.0]);   // delegateIndex === index 5
        compare(m.length, 50);
        verify(parabolic.higherIndexes.indexOf(6) !== -1,
               "slot did not forward sglUpdateHigherItemScale(index+1)");
    }

    // Head spacer, Right alignment: else-branch zeroes length.
    function test_higherItemScale_head_right_zeroesLength() {
        const m = make(0, 5);
        myView.alignment = LatteCore.Types.Right;
        m.length = 21;
        parabolic.sglUpdateHigherItemScale(5, [2.0]);
        compare(m.length, 0);
    }

    // updateScale's loop clamps to newScales.length: with hiddenItemsCount=1 but
    // an empty newScales, the i<newScales.length guard skips, nextFactor stays 0,
    // length=0. Exercises the in-loop bounds branch on a tail spacer.
    function test_updateScale_emptyScales_zeroFactor() {
        const m = make(5, 0);
        myView.alignment = LatteCore.Types.Center;
        m.length = 99;
        parabolic.sglUpdateLowerItemScale(0, []);       // newScales empty
        compare(m.length, 0);                            // nextFactor 0 * 50
    }
}
