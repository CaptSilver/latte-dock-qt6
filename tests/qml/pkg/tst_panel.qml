// Coverage for the Latte shell's Panel.qml — the wrapper KSvg.FrameSvgItem
// that hosts the containment graphic item inside a Latte view. The component
// is loaded from the staged (instrumented) shell package by file URL so the
// Cov.tick markers fire.
//
// Panel.qml has id:root and reads its OWN properties (containment, viewLayout,
// containmentApplet) — those resolve against the component itself, not this
// test's creation context — so we drive them by assigning the component's
// properties after construction. The units exercised:
//   - adjustPrefix@41         : the no-applet early return and the edge path
//   - onContainmentChanged@80 : reparent + viewLayout discovery loop
//   - onLocationChanged@75    : the Connections handler on containmentApplet
//   - appletContainsPos@107   : the viewLayout-present and -absent branches
// Every test pins an observable effect: a return value, the prefix property,
// the viewLayout property, or a mock side-effect.
//
// Probed offscreen reality: with imagePath:"" the FrameSvgItem has no theme
// loaded, so hasElementPrefix(pre) is always false; thus the edge branch of
// adjustPrefix always lands on `prefix = ""`. We pre-seed prefix:"north" and
// assert it gets reset, which proves the switch reached a real edge case and
// ran the hasElementPrefix(false) tail. PlasmaCore.Types edge enum values here
// are Top=3 Bottom=4 Left=5 Right=6.
import QtQuick
import QtTest
import org.kde.plasma.core 2.0 as PlasmaCore

TestCase {
    id: root
    name: "Panel"
    when: windowShown
    visible: true
    width: 500
    height: 60

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/views/Panel.qml")

    // A stand-in for the containment graphic object. It is an Item (the
    // component's `property Item containment` slot requires one) carrying a
    // `location` int so containmentApplet.location drives adjustPrefix's switch
    // and changing it fires locationChanged for the Connections handler. It has
    // NO `plasmoid` member, so containmentApplet resolves to the containment
    // itself. The child Item named "containmentViewLayout" is what the
    // onContainmentChanged discovery loop must latch onto as viewLayout; it also
    // answers appletContainsPos so the viewLayout-present branch is assertable.
    Component {
        id: containmentComponent
        Item {
            id: cont
            property int location: PlasmaCore.Types.TopEdge
            property int posCalls: 0
            property var lastAppletId
            property var lastPos
            Item {
                objectName: "containmentViewLayout"
                function appletContainsPos(appletId, pos) {
                    cont.posCalls++;
                    cont.lastAppletId = appletId;
                    cont.lastPos = pos;
                    return true;
                }
            }
        }
    }

    function makePanel() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    function makeContainment() {
        const obj = createTemporaryObject(containmentComponent, root, {});
        verify(obj, "containment mock failed");
        return obj;
    }

    // adjustPrefix with no containment: containmentApplet is null, so the
    // function takes the early return and hands back "" without touching prefix.
    // KSvg's prefix getter returns a QStringList-backed value (e.g. ["north"]),
    // not a plain string, so compare via String() — strict === against a string
    // never matches even when they read equal.
    function test_adjustPrefix_noApplet() {
        const m = makePanel();
        m.prefix = "north";
        compare(m.containmentApplet, null);
        compare(m.adjustPrefix(), "");
        // early return must not rewrite prefix
        compare(String(m.prefix), "north");
    }

    // adjustPrefix with a containment on a real edge: the switch hits a case,
    // hasElementPrefix is false (no theme), so the tail resets prefix to "".
    // Seed prefix:"north" first so the reset is an observable change.
    function test_adjustPrefix_edge_resetsPrefix() {
        const m = makePanel();
        const cont = makeContainment();
        m.containment = cont; // also fires onContainmentChanged (covered below)
        compare(m.containmentApplet, cont);

        cont.location = PlasmaCore.Types.LeftEdge;
        m.prefix = "north";
        m.adjustPrefix();
        compare(String(m.prefix), "");

        // a non-edge location lands on the switch default, which also sets prefix "".
        cont.location = 999;
        m.prefix = "north";
        m.adjustPrefix();
        compare(String(m.prefix), "");
    }

    // Assigning containment runs onContainmentChanged: it reparents the
    // containment, then scans its children for objectName "containmentViewLayout"
    // and assigns the match to viewLayout. Assert viewLayout latched onto the
    // expected child (observable property change) and the reparent happened.
    function test_onContainmentChanged_discoversViewLayout() {
        const m = makePanel();
        const cont = makeContainment();
        compare(m.viewLayout, null);

        m.containment = cont;
        verify(m.viewLayout !== null, "viewLayout not discovered");
        compare(m.viewLayout.objectName, "containmentViewLayout");
        // the handler reparented the containment into the panel and showed it
        compare(cont.visible, true);
        verify(cont.parent !== null);

        // Assigning a null containment re-runs the handler and hits its early
        // return (no crash, viewLayout left as-is from the prior assignment).
        m.containment = null;
        compare(m.viewLayout.objectName, "containmentViewLayout");
    }

    // The Connections handler targets containmentApplet and fires on its
    // locationChanged, calling adjustPrefix(). Change the mock's location and
    // assert prefix was reset by the handler-driven adjustPrefix.
    function test_onLocationChanged_callsAdjustPrefix() {
        const m = makePanel();
        const cont = makeContainment();
        m.containment = cont;
        cont.location = PlasmaCore.Types.TopEdge;

        m.prefix = "north";
        // toggling location emits locationChanged -> Connections -> adjustPrefix
        cont.location = PlasmaCore.Types.BottomEdge;
        compare(String(m.prefix), "");
    }

    // appletContainsPos: with viewLayout present it forwards to the child's
    // appletContainsPos (assert the return value AND the mock side-effect);
    // with viewLayout absent it returns false.
    function test_appletContainsPos() {
        const m = makePanel();

        // no viewLayout yet -> false branch
        compare(m.viewLayout, null);
        compare(m.appletContainsPos("a", Qt.point(1, 2)), false);

        // wire up viewLayout via the containment discovery, then forward
        const cont = makeContainment();
        m.containment = cont;
        verify(m.viewLayout !== null);

        const before = cont.posCalls;
        const r = m.appletContainsPos("applet-7", Qt.point(3, 4));
        compare(r, true);
        compare(cont.posCalls, before + 1);
        compare(cont.lastAppletId, "applet-7");
        compare(cont.lastPos.x, 3);
        compare(cont.lastPos.y, 4);
    }
}
