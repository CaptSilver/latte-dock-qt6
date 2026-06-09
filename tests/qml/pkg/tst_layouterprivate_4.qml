// Coverage spike for the containment LayouterPrivate. The component holds the
// fillWidth/fillHeight layout math: a set of pure-ish functions that take an
// explicit `layout` object (with .children) plus the size metrics. We load the
// instrumented staged copy by file URL and drive those functions directly with
// hand-built mock applet layouts, so each function call fires a Cov tick.
import QtQuick
import QtTest

TestCase {
    id: testCase
    name: "LayouterPrivate"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/privates/LayouterPrivate.qml")

    // A fake "AppletItem" with the metric properties the layout math reads.
    Component {
        id: appletComp
        QtObject {
            property bool isAutoFillApplet: true
            property bool isHidden: false
            property bool isInternalViewSplitter: false
            property var applet: ({})            // truthy stand-in for a real applet
            property bool inFillCalculations: false
            property int appletMinimumLength: 10
            property int appletPreferredLength: 30
            property int appletMaximumLength: 100
            property real minAutoFillLength: 0
            property real maxAutoFillLength: 0
            property string pluginName: "mock"
            property int index: 0
        }
    }

    // A fake "layout" object: only `.children` is read by the math.
    Component {
        id: layoutComp
        QtObject {
            property var children: []
        }
    }

    function makeApplet(props) {
        const a = appletComp.createObject(testCase, props ? props : {});
        verify(a, "applet mock instantiate failed");
        return a;
    }

    function makeLayout(applets) {
        const l = layoutComp.createObject(testCase);
        verify(l, "layout mock instantiate failed");
        l.children = applets ? applets : [];
        return l;
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, testCase);
        verify(obj, "instantiate failed: " + c.errorString());
        return obj;
    }

    // appletPreferredLength is a pure qBound-style helper; exercise its -1
    // shortcuts and the clamp.
    function test_appletPreferredLength() {
        const m = make();
        // pref valid, clamp into [min,max]
        compare(m.appletPreferredLength(10, 30, 100), 30);
        // max === -1 -> falls back to pref
        compare(m.appletPreferredLength(10, 50, -1), 50);
        // pref === -1 and max === -1 -> both fall to min branch
        compare(m.appletPreferredLength(20, -1, -1), 20);
        // pref below min -> clamped up to min
        compare(m.appletPreferredLength(40, 10, 100), 40);
        // pref above max -> clamped down to max
        compare(m.appletPreferredLength(10, 200, 80), 80);
    }

    // initLayoutForFillsCalculations flips inFillCalculations on autofill applets.
    function test_initLayoutForFillsCalculations() {
        const m = make();
        const a1 = makeApplet({ isAutoFillApplet: true, inFillCalculations: false });
        const a2 = makeApplet({ isAutoFillApplet: false, inFillCalculations: false });
        const layout = makeLayout([a1, a2]);
        m.initLayoutForFillsCalculations(layout);
        verify(a1.inFillCalculations === true);
        verify(a2.inFillCalculations === false);
    }

    // computeStep1ForLayout: an autofill applet with valid metrics and small
    // applied size gets assigned, advancing availableSpace/noOfApplets.
    function test_computeStep1_assigns() {
        const m = make();
        const a = makeApplet({
            isAutoFillApplet: true, isHidden: false,
            appletMinimumLength: 10, appletPreferredLength: 30, appletMaximumLength: 100
        });
        const layout = makeLayout([a]);
        // noOfApplets === 1 branch, sizePerApplet large enough to accept appliedSize
        const res = m.computeStep1ForLayout(layout, 500, 200, 1, true);
        compare(res.length, 3);
        // appliedSize = appletPreferredLength(10,30,min(100,200)) = 30, and 30 <= 200 -> assigned
        compare(a.maxAutoFillLength, 30);
        // min path left alone since inMaxAutoFillCalculations was true
        compare(a.minAutoFillLength, 0);
        // 500 - 30 = 470 free, applet consumed -> noOfApplets decremented to 0
        compare(res[0], 470);
        compare(res[2], 0);
        verify(a.inFillCalculations === false);
    }

    // computeStep1 with multiple applets, exercising the min pass (inMax=false).
    // a1 hits the noOfApplets>1 branch: appliedSize = appletPreferredLength(10,20,50) = 20,
    // assigned to minAutoFillLength. After a1, noOfApplets drops to 1, so a2 hits the
    // noOfApplets===1 branch and also lands at 20.
    // Quirk worth pinning: the availableSpace decrement always reads maxAutoFillLength
    // (LayouterPrivate.qml:133), which the min pass never writes, so availableSpace stays
    // at 400 across both assignments while noOfApplets still counts down to 0.
    function test_computeStep1_multi_mincalc() {
        const m = make();
        const a1 = makeApplet({ appletMinimumLength: 10, appletPreferredLength: 20, appletMaximumLength: 50 });
        const a2 = makeApplet({ appletMinimumLength: 10, appletPreferredLength: 20, appletMaximumLength: 50 });
        const layout = makeLayout([a1, a2]);
        const res = m.computeStep1ForLayout(layout, 400, 100, 2, false);
        compare(res.length, 3);
        compare(a1.minAutoFillLength, 20);
        compare(a2.minAutoFillLength, 20);
        // both assigned via the min path, maxAutoFillLength must stay untouched
        compare(a1.maxAutoFillLength, 0);
        // availableSpace untouched (decrement reads the still-zero maxAutoFillLength)
        compare(res[0], 400);
        // both applets consumed -> noOfApplets counted down to 0
        compare(res[2], 0);
        // assignment clears the in-progress flag
        verify(a1.inFillCalculations === false);
        verify(a2.inFillCalculations === false);
    }

    // computeStep1: systemDecide path (no pref, not static) leaves applet unassigned.
    function test_computeStep1_systemDecide() {
        const m = make();
        const a = makeApplet({
            isAutoFillApplet: true,
            appletMinimumLength: -1, appletPreferredLength: -1, appletMaximumLength: -1
        });
        const layout = makeLayout([a]);
        const res = m.computeStep1ForLayout(layout, 300, 100, 1, true);
        // systemDecide true (no pref, not static) -> not assigned this pass, tuple unchanged
        compare(res[0], 300);
        compare(res[1], 100);
        compare(res[2], 1);
        // applet got no space and its calc flag is untouched (still the default false)
        compare(a.maxAutoFillLength, 0);
    }

    // computeStep2: noOfApplets===0 with a most-demanding applet -> gains remaining space.
    function test_computeStep2_mostDemanding() {
        const m = make();
        const a = makeApplet({
            isAutoFillApplet: true,
            appletMinimumLength: 10, appletPreferredLength: 30, appletMaximumLength: 100,
            maxAutoFillLength: 40
        });
        const layout = makeLayout([a]);
        m.computeStep2ForLayout(layout, 60, 0, true);
        compare(a.maxAutoFillLength, 100); // 40 + 60
    }

    // computeStep2: noOfApplets===0 with only neutral applets -> split equally.
    function test_computeStep2_neutral() {
        const m = make();
        const a1 = makeApplet({
            appletMinimumLength: 0, appletPreferredLength: 0, appletMaximumLength: 100,
            maxAutoFillLength: 0
        });
        const a2 = makeApplet({
            appletMinimumLength: 0, appletPreferredLength: 0, appletMaximumLength: 100,
            maxAutoFillLength: 0
        });
        const layout = makeLayout([a1, a2]);
        m.computeStep2ForLayout(layout, 80, 0, true);
        // 80 / 2 neutral applets = 40 each
        compare(a1.maxAutoFillLength, 40);
        compare(a2.maxAutoFillLength, 40);
    }

    // computeStep2: noOfApplets>0 branch -> applets still inFillCalculations get sizePerApplet.
    function test_computeStep2_remaining() {
        const m = make();
        const a = makeApplet({
            isAutoFillApplet: true, inFillCalculations: true,
            appletMinimumLength: 5
        });
        const layout = makeLayout([a]);
        m.computeStep2ForLayout(layout, 25, 1, false);
        compare(a.minAutoFillLength, 25);
        verify(a.inFillCalculations === false);
    }

    // computeStep2: a negative sizePerApplet fails the (sizePerApplet>=0) guard, so the
    // function leaves every applet's fill lengths untouched. Seed a most-demanding applet
    // that the noOfApplets===0 branch *would* grow if the guard passed, then assert it
    // didn't move.
    function test_computeStep2_guard_skips() {
        const m = make();
        const a = makeApplet({
            isAutoFillApplet: true,
            appletMinimumLength: 10, appletPreferredLength: 30, appletMaximumLength: 100,
            maxAutoFillLength: 40
        });
        const layout = makeLayout([a]);
        m.computeStep2ForLayout(layout, -1, 0, true);
        // guard short-circuits: the seeded size survives unchanged
        compare(a.maxAutoFillLength, 40);
        compare(a.minAutoFillLength, 0);
    }

    // The remaining functions (initializationPhase, updateFillAppletsWithOneStep,
    // updateFillAppletsWithTwoSteps, _updateSizeForAppletsInFill) read the
    // unqualified `root`/`background`/`layouter` context ids that the real
    // containment injects as context properties. There's no QML-only way to
    // supply those in qmltestrunner, so they can't be exercised headlessly.
}
