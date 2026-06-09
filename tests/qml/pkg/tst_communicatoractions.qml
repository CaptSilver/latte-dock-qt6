// Coverage for the containment communicator's Actions component. It is an
// anonymous Item exposing four functions:
//   setProperty / getProperty  - read+write mainCommunicator.requires.<flag>
//   broadcastToApplet          - emits root.broadcastedToApplet(...)
//   version                    - LatteCore.Environment.makeVersion(maj,min,rel)
// setProperty/getProperty read the unqualified name `mainCommunicator`, and
// broadcastToApplet reads the unqualified `root` plus its broadcastedToApplet
// signal. QML resolves both against the component's creation context, so the
// TestCase is named `root`, carries a `broadcastedToApplet` signal, and
// declares a `mainCommunicator` mock shaped like the real communicator
// (a `requires` object with the six boolean flags the functions touch).
// version() uses the real org.kde.latte.core import, available in the staged
// import path. Every test asserts an observable effect: a returned value, a
// mock property mutation, or a signal emission captured by SignalSpy.
import QtQuick
import QtTest
import org.kde.latte.core 0.2 as LatteCore

TestCase {
    id: root
    name: "CommunicatorActions"
    when: windowShown

    // broadcastToApplet@49 forwards into this signal on the creation context.
    signal broadcastedToApplet(string receiverPluginId, string action, var value)

    // Mock of the main communicator. setProperty/getProperty only ever read
    // and write mainCommunicator.requires.<flag>; nothing else is touched, so
    // this carries exactly the six boolean requirements the functions map.
    QtObject {
        id: mainCommunicator
        property QtObject requires: QtObject {
            property bool latteSideColoringEnabled: false
            property bool activeIndicatorEnabled: false
            property bool lengthMarginsEnabled: false
            property bool parabolicEffectLocked: false
            property bool screenEdgeMarginSupported: false
            property bool windowsTrackingEnabled: false
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/applet/communicator/Actions.qml")

    function resetRequires() {
        mainCommunicator.requires.latteSideColoringEnabled = false;
        mainCommunicator.requires.activeIndicatorEnabled = false;
        mainCommunicator.requires.lengthMarginsEnabled = false;
        mainCommunicator.requires.parabolicEffectLocked = false;
        mainCommunicator.requires.screenEdgeMarginSupported = false;
        mainCommunicator.requires.windowsTrackingEnabled = false;
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // setProperty@11 maps each parameter name to a requires flag. Drive every
    // branch and assert the matching flag flipped (and only that one), then a
    // no-match name to walk the final fall-through with no side-effect.
    function test_setProperty_writesEachFlag() {
        resetRequires();
        const m = make();
        const r = mainCommunicator.requires;

        m.setProperty(0, "latteSideColoringEnabled", true);
        compare(r.latteSideColoringEnabled, true);
        compare(r.activeIndicatorEnabled, false);

        m.setProperty(0, "activeIndicatorEnabled", true);
        compare(r.activeIndicatorEnabled, true);

        m.setProperty(0, "lengthMarginsEnabled", true);
        compare(r.lengthMarginsEnabled, true);

        m.setProperty(0, "parabolicEffectLocked", true);
        compare(r.parabolicEffectLocked, true);

        m.setProperty(0, "screenEdgeMarginSupported", true);
        compare(r.screenEdgeMarginSupported, true);

        m.setProperty(0, "windowsTrackingEnabled", true);
        compare(r.windowsTrackingEnabled, true);

        // unknown parameter: no branch matches, nothing changes.
        m.setProperty(0, "doesNotExist", false);
        compare(r.latteSideColoringEnabled, true);
        compare(r.windowsTrackingEnabled, true);
    }

    // getProperty@27 returns the requires flag for each known parameter and
    // null for anything else. Seed distinct values so the returned value
    // proves the right flag was read, not a constant.
    function test_getProperty_returnsEachFlag() {
        resetRequires();
        const m = make();
        const r = mainCommunicator.requires;

        r.latteSideColoringEnabled = true;
        compare(m.getProperty(0, "latteSideColoringEnabled"), true);

        r.activeIndicatorEnabled = true;
        compare(m.getProperty(0, "activeIndicatorEnabled"), true);

        r.lengthMarginsEnabled = true;
        compare(m.getProperty(0, "lengthMarginsEnabled"), true);

        r.parabolicEffectLocked = true;
        compare(m.getProperty(0, "parabolicEffectLocked"), true);

        r.screenEdgeMarginSupported = true;
        compare(m.getProperty(0, "screenEdgeMarginSupported"), true);

        r.windowsTrackingEnabled = true;
        compare(m.getProperty(0, "windowsTrackingEnabled"), true);

        // a still-false flag returns false (not the null fall-through)...
        r.activeIndicatorEnabled = false;
        compare(m.getProperty(0, "activeIndicatorEnabled"), false);
        // ...and an unknown parameter hits the final `return null`.
        compare(m.getProperty(0, "doesNotExist"), null);
    }

    // broadcastToApplet@49 just forwards its three arguments into the
    // creation-context broadcastedToApplet signal. Capture it with a SignalSpy
    // and assert the arguments arrived intact.
    function test_broadcastToApplet_emitsSignal() {
        const m = make();
        const spy = createTemporaryObject(signalSpyComponent, root,
                                          {target: root, signalName: "broadcastedToApplet"});
        m.broadcastToApplet("org.kde.someApplet", "doThing", 42);
        compare(spy.count, 1);
        compare(spy.signalArguments[0][0], "org.kde.someApplet");
        compare(spy.signalArguments[0][1], "doThing");
        compare(spy.signalArguments[0][2], 42);
    }

    // version@53 packs major/minor/patch into a single int via the real
    // LatteCore.Environment.makeVersion: (major<<16)|(minor<<8)|patch.
    function test_version_packsComponents() {
        const m = make();
        compare(m.version(0, 10, 77), (0 << 16) | (10 << 8) | 77); // 2637
        compare(m.version(1, 2, 3), (1 << 16) | (2 << 8) | 3);     // 65539
        compare(m.version(0, 0, 0), 0);
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
