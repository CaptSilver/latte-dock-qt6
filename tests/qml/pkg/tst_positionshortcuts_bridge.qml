// Coverage for the PositionShortcuts bridge (abilities/bridge/PositionShortcuts.qml),
// a BridgeItem subclass that wires a host ability to a client ability. It reads
// no unqualified creation-context names — everything it touches comes through its
// own `host`, `client` and `appletIndex` slots (inherited from BridgeItem). Those
// slots are `property Item`, so a QtObject mock would silently stay null; we give
// each a real Item shaped exactly like the host/client abilities:
//
//   host  — sglActivateEntryAtIndex / sglNewInstanceForEntryAtIndex signals (the
//           connect() sources), an appletIdStealingPositionShortcuts int, and a
//           currentAppletStealingPositionShortcuts(id) signal.
//   client— sglActivateEntryAtIndex / sglNewInstanceForEntryAtIndex signals (the
//           connect() targets, spied on), a settable isStealingGlobalPositionShortcuts
//           bool, and a disabledIsStealingGlobalPositionShortcuts() recorder.
//
// Each test loads the instrumented staged copy by file URL and pins an observable
// effect: a forwarded signal arriving across the connected pair, or a recorder/
// signal firing on a mock with the right argument.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "PositionShortcutsBridge"
    when: windowShown

    // Host mock. The two sgl* signals are the sources the bridge .connect()s to
    // the client; appletIdStealingPositionShortcuts is read by onClientChanged;
    // currentAppletStealingPositionShortcuts is both emitted (to drive the host
    // Connections handler) and called by the client Connections handler.
    Component {
        id: hostComponent
        Item {
            property int appletIdStealingPositionShortcuts: -1
            signal sglActivateEntryAtIndex(int entryIndex)
            signal sglNewInstanceForEntryAtIndex(int entryIndex)
            signal currentAppletStealingPositionShortcuts(int id)
        }
    }

    // Client mock. The sgl* signals are the connect() targets (we spy on them to
    // prove the host->client wiring). isStealingGlobalPositionShortcuts is a
    // settable bool whose change drives the client Connections handler.
    // disabledIsStealingGlobalPositionShortcuts records the calls the bridge makes
    // (the real one is a signal; a recorder lets us assert the call landed).
    Component {
        id: clientComponent
        Item {
            property bool isStealingGlobalPositionShortcuts: false
            property int disabledCalls: 0
            signal sglActivateEntryAtIndex(int entryIndex)
            signal sglNewInstanceForEntryAtIndex(int entryIndex)
            function disabledIsStealingGlobalPositionShortcuts() { disabledCalls++; }
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/bridge/PositionShortcuts.qml")

    function makeBridge() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // onIsConnectedChanged@12: isConnected is host && client. Set host first
    // (no transition), then client -> isConnected goes false->true and the
    // connect branch runs, binding host.sgl* to client.sgl*. Prove the wiring by
    // emitting each host signal and watching the matching client signal fire with
    // the same argument across the connection.
    function test_isConnectedChanged_connectsSignals() {
        const b = makeBridge();
        const host = createTemporaryObject(hostComponent, root, {});
        const client = createTemporaryObject(clientComponent, root, {});
        verify(host && client, "host/client mocks failed");

        const activateSpy = createTemporaryObject(signalSpyComponent, root,
                                {target: client, signalName: "sglActivateEntryAtIndex"});
        const newInstSpy = createTemporaryObject(signalSpyComponent, root,
                                {target: client, signalName: "sglNewInstanceForEntryAtIndex"});

        b.host = host;        // client still null -> isConnected stays false
        compare(b.isConnected, false);
        b.client = client;    // isConnected false -> true, connect branch runs
        compare(b.isConnected, true);

        // host.sglActivateEntryAtIndex is now connected to client's; emitting the
        // host signal must re-emit the client signal with the same entry index.
        host.sglActivateEntryAtIndex(7);
        compare(activateSpy.count, 1);
        compare(activateSpy.signalArguments[0][0], 7);

        host.sglNewInstanceForEntryAtIndex(9);
        compare(newInstSpy.count, 1);
        compare(newInstSpy.signalArguments[0][0], 9);
    }

    // onClientChanged@22: when client is assigned and
    // host.appletIdStealingPositionShortcuts !== appletIndex, the bridge tells the
    // client to drop its stealing state. host is read unconditionally inside the
    // handler, so it must be set first. appletIndex defaults to -1; give the host
    // a different stealing id (5) so the inequality is true and the call fires.
    function test_clientChanged_disablesWhenIndexDiffers() {
        const b = makeBridge();
        const host = createTemporaryObject(hostComponent, root,
                                           {appletIdStealingPositionShortcuts: 5});
        const client = createTemporaryObject(clientComponent, root, {});
        verify(host && client, "host/client mocks failed");
        compare(b.appletIndex, -1);

        b.host = host;
        compare(client.disabledCalls, 0);
        b.client = client;   // index (-1) !== stealing id (5) -> disable called
        compare(client.disabledCalls, 1);
    }

    // onClientChanged@22 equal-index branch: when the host's stealing id matches
    // appletIndex, the inner if is false and the client is NOT disabled. Set both
    // to 3 and assert no call happened (the handler still ran -> entry tick).
    function test_clientChanged_noDisableWhenIndexMatches() {
        const b = makeBridge();
        b.appletIndex = 3;
        const host = createTemporaryObject(hostComponent, root,
                                           {appletIdStealingPositionShortcuts: 3});
        const client = createTemporaryObject(clientComponent, root, {});
        verify(host && client, "host/client mocks failed");

        b.host = host;
        b.client = client;   // 3 === 3 -> no disable call
        compare(client.disabledCalls, 0);
    }

    // onIsStealingGlobalPositionShortcutsChanged@31 (Connections target: client):
    // when connected and the client flips isStealingGlobalPositionShortcuts true,
    // the bridge reports the appletIndex up to the host via
    // currentAppletStealingPositionShortcuts. Spy on that host signal and assert
    // it fired once carrying appletIndex.
    function test_clientStealingChanged_reportsToHost() {
        const b = makeBridge();
        b.appletIndex = 4;
        const host = createTemporaryObject(hostComponent, root, {});
        const client = createTemporaryObject(clientComponent, root, {});
        verify(host && client, "host/client mocks failed");

        b.host = host;
        b.client = client;
        verify(b.isConnected);

        const hostSpy = createTemporaryObject(signalSpyComponent, root,
                            {target: host, signalName: "currentAppletStealingPositionShortcuts"});

        client.isStealingGlobalPositionShortcuts = true;   // drives the handler
        compare(hostSpy.count, 1);
        compare(hostSpy.signalArguments[0][0], 4);

        // flipping it back to false -> isStealingGlobalPositionShortcuts is false,
        // inner if guards out, no further host report.
        client.isStealingGlobalPositionShortcuts = false;
        compare(hostSpy.count, 1);
    }

    // onCurrentAppletStealingPositionShortcuts@40 (Connections target: host):
    // when the host announces some other applet (id) is stealing and this bridge
    // has a different appletIndex, the bridge disables its own client's stealing
    // state. Emit the host signal with a foreign id and assert the client was
    // disabled; emit it with this bridge's own index and assert it was NOT.
    function test_hostAnnouncesStealer_disablesForeignClient() {
        const b = makeBridge();
        b.appletIndex = 2;
        const host = createTemporaryObject(hostComponent, root, {});
        const client = createTemporaryObject(clientComponent, root, {});
        verify(host && client, "host/client mocks failed");

        b.host = host;
        b.client = client;
        const baseline = client.disabledCalls;   // onClientChanged may have run

        // another applet (id 8) is stealing -> 2 !== 8 && client -> disable.
        host.currentAppletStealingPositionShortcuts(8);
        compare(client.disabledCalls, baseline + 1);

        // host announces THIS bridge's index (2) -> 2 !== 2 is false -> no disable.
        host.currentAppletStealingPositionShortcuts(2);
        compare(client.disabledCalls, baseline + 1);
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
