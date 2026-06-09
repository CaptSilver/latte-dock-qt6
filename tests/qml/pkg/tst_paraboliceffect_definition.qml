// Drives the abilities ParabolicEffect definition through its three pure
// scale functions: scaleLinear, scaleForItem, and applyParabolicEffect.
// The component is loaded from the staged (instrumented) package by file URL,
// and every assertion pins an observable effect: a return value, a computed
// scale, or a signal emission with its arguments.
//
// applyParabolicEffect reads two qualified globals (Qt.application and
// PlasmaCore.Types) plus, on the reversed path only, the Plasmoid attached
// object's formFactor. Headlessly the default layout direction is LeftToRight,
// so `layoutDirection === RightToLeft && Plasmoid.formFactor === ...`
// short-circuits before ever touching Plasmoid. The non-reversed path (entry,
// both spread loops, both signal emissions, the return value) runs for real;
// the RightToLeft swap branch needs a live applet for Plasmoid.formFactor and
// is recorded live-only.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "ParabolicEffectDefinition"
    when: windowShown

    // Load the instrumented component from the staged install tree. From
    // tests/qml/pkg that's up three (to the repo root) then down into the
    // staged abilities/definition package.
    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/definition/ParabolicEffect.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root);
        verify(obj, "instantiate failed");
        return obj;
    }

    // scaleLinear(x) = (factor.zoom - 1) * x. Assert the default-zoom result
    // and that changing factor.zoom changes the slope (proves the read of
    // factor.zoom, not a constant).
    function test_scaleLinear() {
        const m = make();
        compare(m.factor.zoom, 1.6);
        fuzzyCompare(m.scaleLinear(0.0), 0.0, 1e-9);
        fuzzyCompare(m.scaleLinear(0.5), 0.6 * 0.5, 1e-9);
        fuzzyCompare(m.scaleLinear(1.0), 0.6, 1e-9);

        m.factor.zoom = 2.0;
        fuzzyCompare(m.scaleLinear(1.0), 1.0, 1e-9);
        fuzzyCompare(m.scaleLinear(0.25), 0.25, 1e-9);
    }

    // scaleForItem maps a slice of the x axis and returns 1 + scaleLinear(curX).
    // For a single-item slice (itemsCount=1, itemIndex=1) curX == the mouse
    // percentage, so the result is 1 + 0.6*percentage at the default zoom.
    function test_scaleForItem() {
        const m = make();
        // itemsCount=1: xSliceLength=1, minX=0, maxX=1, curX=percentage.
        fuzzyCompare(m.scaleForItem(0.0, 1, 1), 1.0, 1e-9);
        fuzzyCompare(m.scaleForItem(0.5, 1, 1), 1.0 + 0.6 * 0.5, 1e-9);
        fuzzyCompare(m.scaleForItem(1.0, 1, 1), 1.6, 1e-9);

        // itemsCount=2, second slice (itemIndex=2): minX=0.5, maxX=1,
        // curX = 0.5 + 0.5*percentage. At percentage=0 -> curX=0.5.
        fuzzyCompare(m.scaleForItem(0.0, 2, 2), 1.0 + 0.6 * 0.5, 1e-9);
        // At percentage=1 -> curX=1.0 -> 1 + 0.6.
        fuzzyCompare(m.scaleForItem(1.0, 2, 2), 1.6, 1e-9);
    }

    // applyParabolicEffect drives both spread loops, emits the higher/lower
    // scale signals, and returns the first element of each scale array. With
    // the default spread=3, _spreadSteps=1: each loop runs once and produces
    // a two-element array [scaleForItem(...), 1].
    function test_applyParabolicEffect() {
        const m = make();
        compare(m.spread, 3);

        const higherSpy = createTemporaryObject(signalSpyComponent, root,
                                                {target: m, signalName: "sglUpdateHigherItemScale"});
        const lowerSpy = createTemporaryObject(signalSpyComponent, root,
                                               {target: m, signalName: "sglUpdateLowerItemScale"});

        // percentage = clamp(4/8) = 0.5; both loops call scaleForItem(0.5,1,1)=1.3.
        const res = m.applyParabolicEffect(5, 4, 8);

        verify(res !== undefined && res !== null, "applyParabolicEffect returned nothing");
        fuzzyCompare(res.leftScale, 1.3, 1e-9);
        fuzzyCompare(res.rightScale, 1.3, 1e-9);

        // Higher signal: itemIndex+1 = 6, scales = [1.3, 1].
        compare(higherSpy.count, 1);
        compare(higherSpy.signalArguments[0][0], 6);
        const hi = higherSpy.signalArguments[0][1];
        compare(hi.length, 2);
        fuzzyCompare(hi[0], 1.3, 1e-9);
        compare(hi[1], 1);

        // Lower signal: itemIndex-1 = 4, scales = [1.3, 1].
        compare(lowerSpy.count, 1);
        compare(lowerSpy.signalArguments[0][0], 4);
        const lo = lowerSpy.signalArguments[0][1];
        compare(lo.length, 2);
        fuzzyCompare(lo[0], 1.3, 1e-9);
        compare(lo[1], 1);
    }

    // The mouse position is clamped into [0,1] before use: positions past the
    // item length saturate at percentage=1, negative ones at 0. Assert via the
    // returned scales (percentage=1 -> right slice scaleForItem(1,1,1)=1.6,
    // left slice scaleForItem(0,1,1)=1.0).
    function test_applyParabolicEffect_clampsPercentage() {
        const m = make();

        const higherSpy = createTemporaryObject(signalSpyComponent, root,
                                                {target: m, signalName: "sglUpdateHigherItemScale"});
        const lowerSpy = createTemporaryObject(signalSpyComponent, root,
                                               {target: m, signalName: "sglUpdateLowerItemScale"});

        // itemMousePosition far past itemLength -> percentage clamps to 1.
        const res = m.applyParabolicEffect(2, 100, 10);

        // right (percentage=1): scaleForItem(1,1,1)=1.6 ; left (1-percentage=0): 1.0
        fuzzyCompare(res.rightScale, 1.6, 1e-9);
        fuzzyCompare(res.leftScale, 1.0, 1e-9);

        // Higher carries the right (saturated) scales, lower the left.
        compare(higherSpy.count, 1);
        fuzzyCompare(higherSpy.signalArguments[0][1][0], 1.6, 1e-9);
        compare(lowerSpy.count, 1);
        fuzzyCompare(lowerSpy.signalArguments[0][1][0], 1.0, 1e-9);
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
