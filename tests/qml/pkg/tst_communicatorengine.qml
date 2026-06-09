// Drives the containment applet Communicator (communicator/Engine.qml) through
// the two instrumented units whose effect is observable headlessly: the
// appletItem.appletChanged Connections handler and the overlayInitTimer's
// onTriggered. The component is loaded from the staged (instrumented) package by
// file URL so the Cov.tick calls fire, and every assertion pins an observable
// effect: a property change or a timer-running flip.
//
// reconsiderAppletIconItem() runs as part of the onTriggered init pass (so it
// ticks via that asserted flow), but its own observable effect — setting
// appletIconItem to a discovered IconItem — needs a mock applet child tree whose
// toString() matches 'CompactRepresentation('/'IconItem('. A generic applet finds
// nothing, so that positive effect is live-only; we don't claim it directly.
//
// Unqualified creation-context names the component (and its imported
// AppletIdentifier.js) read:
//   - communicator : in the real containment this is AppletItem's alias back to
//                    the Engine itself; the JS reads communicator.appletIconItem
//                    and communicator.indexerIsSupported and writes appletIconItem.
//                    We declare it on root and point it at the Engine instance.
//   - applet       : the underlying Plasma applet. The JS guards/loops over
//                    applet.children + applet.pluginName.
//   - appletItem   : the Connections target that emits appletChanged; also read
//                    inside onTriggered as appletItem.debug.timersEnabled.
// All three are shaped on root (the Engine's creation context) like the real
// objects — never a catch-all.
//
// The applet mocks carry NO latteBridge and empty children on purpose: that keeps
// appletContainsLatteBridge false so bridgeLoader never activates LatteBridge
// (which reads a live containment and would throw). The discovery branch that
// records appletDiscoveredRootItem via a real latteBridge necessarily activates
// that loader, so it is live-only.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "CommunicatorEngine"
    when: windowShown

    // Engine's creation-context names. communicator is wired to the instance in
    // make(); applet is swapped per-test to drive the truthy/falsy branches.
    property var communicator: null
    property Item applet: null

    // The Connections target. appletChanged() drives onAppletChanged@68; debug
    // .timersEnabled is read (false) by onTriggered so its console.log is skipped.
    QtObject {
        id: appletItem
        signal appletChanged()
        property QtObject debug: QtObject { property bool timersEnabled: false }
    }

    // A generic applet: empty children, no latteBridge, a non-special pluginName.
    // checkAndUpdateAppletRootItem loops over zero children and returns; the icon
    // identification dispatches to identifyGeneric, finds no IconItem, returns.
    // Neither throws, and the bridge stays inactive.
    Component {
        id: appletGeneric
        Item { property string pluginName: "org.kde.someapplet" }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/applet/communicator/Engine.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        // communicator resolves to the Engine itself in the real containment;
        // point the creation-context name at the instance so the JS reads/writes
        // (appletIconItem, indexerIsSupported) hit observable instance state.
        root.communicator = obj;
        return obj;
    }

    // overlayInitTimer is the lone Timer (interval 1000); it lives in resources,
    // not children. Match on the default interval rather than an id string.
    function overlayTimer(m) {
        const res = m.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].interval === 1000)
                return res[i];
        }
        return null;
    }

    // onAppletChanged@68: when applet is truthy the handler runs all three
    // statements and arms overlayInitTimer. Assert the timer flipped to running —
    // the handler's observable side-effect.
    function test_onAppletChanged_armsTimer() {
        const m = make();
        const a = createTemporaryObject(appletGeneric, root, {});
        verify(a, "applet mock failed to instantiate");
        root.applet = a;
        const t = overlayTimer(m);
        verify(t, "could not find overlayInitTimer");
        compare(t.running, false);
        appletItem.appletChanged();
        compare(t.running, true);
    }

    // onAppletChanged@68 with applet falsy: the if(applet) guard is false, so the
    // body does nothing and the timer is NOT armed. The handler still ticks; assert
    // the timer stayed idle.
    function test_onAppletChanged_noAppletNoTimer() {
        const m = make();
        root.applet = null;
        const t = overlayTimer(m);
        verify(t, "could not find overlayInitTimer");
        compare(t.running, false);
        appletItem.appletChanged();
        compare(t.running, false);
    }

    // onTriggered@92: the timer fires its init pass — re-checks the applet root
    // item, reconsiders the icon, and clears inStartup. With a generic applet none
    // of the JS throws, so the handler reaches mainCommunicator.inStartup=false.
    // Shrink the interval and assert inStartup flipped true -> false.
    function test_overlayInitTimer_clearsStartup() {
        const m = make();
        const a = createTemporaryObject(appletGeneric, root, {});
        verify(a, "applet mock failed to instantiate");
        root.applet = a;
        compare(m.inStartup, true);
        const t = overlayTimer(m);
        verify(t, "could not find overlayInitTimer");
        t.interval = 1;
        t.start();
        tryVerify(function() { return m.inStartup === false; }, 2000,
                  "overlayInitTimer onTriggered never cleared inStartup");
    }
}
