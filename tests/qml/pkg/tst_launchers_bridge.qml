// Coverage for the launchers bridge ability (abilities/bridge/Launchers.qml),
// a BridgeItem subclass. BridgeItem only adds `appletIndex` (int) and the two
// `property Item host`/`client` slots, so every name this file reads — host,
// client, appletIndex — is a property on the component itself, not a creation-
// context name. We drive each instrumented handler by wiring real Item mocks
// for host/client and toggling the state each handler keys off.
//
// host and client are `property Item` (a QtObject would silently stay null and
// no handler would ever fire), so both mocks are Items carrying exactly the
// members the file touches: client.disabledIsStealingDroppedLaunchers() and
// client.isStealingDroppedLaunchers; host.appletIdStealingDroppedLaunchers and
// the host.currentAppletStealingDroppedLaunchers(id) signal — emitting it is
// both what handler@22 forwards to and what the host Connections binds to. No
// catch-all.
//
// Every test asserts an observable effect: a mock counter the handler bumped
// (the forwarded call landed) or the absence of that call on the no-op branch.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "LaunchersBridge"
    when: windowShown

    // client mock: an Item (the slot is `property Item`). Records the
    // disabledIsStealingDroppedLaunchers() calls onClientChanged@12 and
    // onCurrentAppletStealingDroppedLaunchers@31 make, and carries the
    // isStealingDroppedLaunchers property whose change drives
    // onIsStealingDroppedLaunchersChanged@22.
    Component {
        id: clientComponent
        Item {
            property int disableCalls: 0
            property bool isStealingDroppedLaunchers: false
            function disabledIsStealingDroppedLaunchers() { disableCalls++; }
        }
    }

    // host mock: an Item carrying the launcher-stealing applet id the
    // onClientChanged branch compares against, plus the
    // currentAppletStealingDroppedLaunchers(id) signal. That single name is both
    // what onIsStealingDroppedLaunchersChanged@22 forwards to (emitting it) and
    // what the host-targeted Connections handler@31 binds to — so a SignalSpy on
    // it observes the forward, and emitting it manually drives handler@31.
    Component {
        id: hostComponent
        Item {
            property int appletIdStealingDroppedLaunchers: -1
            signal currentAppletStealingDroppedLaunchers(int id)
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/bridge/Launchers.qml")

    // Build the bridge with host wired and client still null so a later client
    // assignment cleanly triggers onClientChanged. appletIndex defaults to -1
    // (from BridgeItem); each test sets it relative to the host/id values it
    // wants the branch to take.
    function make(hostObj, appletIndex) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {host: hostObj, appletIndex: appletIndex});
        verify(obj, "instantiate failed");
        return obj;
    }

    // onClientChanged@12: assigning a non-null client fires the handler. When
    // host.appletIdStealingDroppedLaunchers !== appletIndex the handler calls
    // client.disabledIsStealingDroppedLaunchers(). Wire them unequal and assert
    // the client mock saw exactly one call.
    function test_onClientChanged_disablesWhenNotStealer() {
        const hostObj = createTemporaryObject(hostComponent, root, {appletIdStealingDroppedLaunchers: 5});
        const clientObj = createTemporaryObject(clientComponent, root, {});
        const m = make(hostObj, 2); // appletIndex 2 != host's stealer id 5

        compare(clientObj.disableCalls, 0);
        m.client = clientObj; // triggers onClientChanged@12
        verify(m.isConnected); // host && client both set
        compare(clientObj.disableCalls, 1);
    }

    // onClientChanged@12, equal branch: when host.appletIdStealingDroppedLaunchers
    // === appletIndex this applet IS the stealer, so the handler must NOT disable.
    // The handler still ticks; assert the no-call branch left the client untouched.
    function test_onClientChanged_keepsWhenStealer() {
        const hostObj = createTemporaryObject(hostComponent, root, {appletIdStealingDroppedLaunchers: 3});
        const clientObj = createTemporaryObject(clientComponent, root, {});
        const m = make(hostObj, 3); // appletIndex 3 == host's stealer id 3

        m.client = clientObj; // triggers onClientChanged@12, equal branch
        compare(clientObj.disableCalls, 0);
    }

    // onIsStealingDroppedLaunchersChanged@22: the Connections{target:client}
    // handler. When isConnected && client.isStealingDroppedLaunchers it forwards
    // host.currentAppletStealingDroppedLaunchers(appletIndex). Set this applet as
    // the stealer (appletIndex == host id so onClientChanged doesn't disable),
    // then flip the client property and assert host saw the call with appletIndex.
    function test_onIsStealingChanged_notifiesHost() {
        const hostObj = createTemporaryObject(hostComponent, root, {appletIdStealingDroppedLaunchers: 8});
        const clientObj = createTemporaryObject(clientComponent, root, {});
        const m = make(hostObj, 8);
        m.client = clientObj;
        verify(m.isConnected);

        // Watch the host signal the handler forwards to.
        const spy = createTemporaryObject(signalSpyComponent, root,
                                          {target: hostObj, signalName: "currentAppletStealingDroppedLaunchers"});
        clientObj.isStealingDroppedLaunchers = true; // fires the handler@22
        compare(spy.count, 1);
        compare(spy.signalArguments[0][0], 8); // forwarded appletIndex
    }

    // onIsStealingDroppedLaunchersChanged@22, guard branch: when the client is
    // NOT stealing (isStealingDroppedLaunchers stays false on the toggle to
    // false) the handler ticks but the if-guard rejects, so host is not called.
    function test_onIsStealingChanged_guardNoNotify() {
        const hostObj = createTemporaryObject(hostComponent, root, {appletIdStealingDroppedLaunchers: 4});
        const clientObj = createTemporaryObject(clientComponent, root, {isStealingDroppedLaunchers: true});
        const m = make(hostObj, 4);
        m.client = clientObj;

        const spy = createTemporaryObject(signalSpyComponent, root,
                                          {target: hostObj, signalName: "currentAppletStealingDroppedLaunchers"});
        clientObj.isStealingDroppedLaunchers = false; // fires handler@22, guard false
        compare(spy.count, 0);
    }

    // onCurrentAppletStealingDroppedLaunchers@31: the Connections{target:host}
    // handler. When the incoming id !== this applet's appletIndex (a DIFFERENT
    // applet became the stealer) and a client is present, it disables this
    // applet's client. Emit the host signal with a foreign id and assert the
    // client was disabled.
    function test_onCurrentStealing_disablesForeignId() {
        const hostObj = createTemporaryObject(hostComponent, root, {appletIdStealingDroppedLaunchers: 1});
        const clientObj = createTemporaryObject(clientComponent, root, {});
        const m = make(hostObj, 7);
        m.client = clientObj;

        clientObj.disableCalls = 0;
        hostObj.currentAppletStealingDroppedLaunchers(2); // 2 != appletIndex 7
        compare(clientObj.disableCalls, 1);
    }

    // onCurrentAppletStealingDroppedLaunchers@31, self branch: when the incoming
    // id === appletIndex (this applet is the one that became the stealer) the
    // handler ticks but must NOT disable its own client. Assert no disable call.
    function test_onCurrentStealing_keepsOwnId() {
        const hostObj = createTemporaryObject(hostComponent, root, {appletIdStealingDroppedLaunchers: 1});
        const clientObj = createTemporaryObject(clientComponent, root, {});
        const m = make(hostObj, 6);
        m.client = clientObj;

        clientObj.disableCalls = 0;
        hostObj.currentAppletStealingDroppedLaunchers(6); // 6 == appletIndex 6
        compare(clientObj.disableCalls, 0);
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
