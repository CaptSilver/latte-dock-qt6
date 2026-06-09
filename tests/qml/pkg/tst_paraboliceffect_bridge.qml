// Coverage for the parabolic bridge ability (abilities/bridge/ParabolicEffect.qml),
// a BridgeItem subclass. BridgeItem gives it three of its own properties —
// appletIndex (int), host (Item), client (Item) — so the component never reads
// any unqualified creation-context name; every value it touches is settable
// directly on the instance. We give host/client real Item mocks (the slots are
// `property Item`, so a QtObject would silently stay null and the calls would
// no-op) shaped with exactly the signals/methods the bridge invokes.
//
// The two client-request functions forward a neighbouring index + scales to the
// host's sgl* methods; the Connections handler relays the host's sglClearZoom
// down to the client. Each test asserts the mock recorded the forwarded call
// with the right argument.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "ParabolicEffectBridge"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/bridge/ParabolicEffect.qml")

    // host mock: the bridge's client-request functions call sglUpdateLowerItemScale
    // / sglUpdateHigherItemScale on it, and its sglClearZoom signal is the
    // Connections target. Item (not QtObject) so the `property Item host` slot
    // actually binds. Records the index + scales it was forwarded.
    Component {
        id: hostComponent
        Item {
            signal sglClearZoom()
            property int lowerIndex: -999
            property var lowerScales: undefined
            property int lowerCalls: 0
            property int higherIndex: -999
            property var higherScales: undefined
            property int higherCalls: 0
            function sglUpdateLowerItemScale(index, scales) {
                lowerIndex = index; lowerScales = scales; lowerCalls++;
            }
            function sglUpdateHigherItemScale(index, scales) {
                higherIndex = index; higherScales = scales; higherCalls++;
            }
        }
    }

    // client mock: the Connections handler calls client.sglClearZoom() when the
    // host fires sglClearZoom and a client is present. Records the relayed call.
    Component {
        id: clientComponent
        Item {
            property int clearCalls: 0
            function sglClearZoom() { clearCalls++; }
        }
    }

    function make(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, props === undefined ? {} : props);
        verify(obj, "instantiate failed");
        return obj;
    }

    // clientRequestUpdateLowerItemScale forwards (appletIndex-1, newScales) to
    // host.sglUpdateLowerItemScale. Assert the host saw the decremented index
    // and the same scales payload.
    function test_clientRequestUpdateLowerItemScale() {
        const host = createTemporaryObject(hostComponent, root, {});
        const m = make({appletIndex: 5, host: host});

        const scales = {zoom: 1.6};
        m.clientRequestUpdateLowerItemScale(scales);

        compare(host.lowerCalls, 1);
        compare(host.lowerIndex, 4); // appletIndex - 1
        compare(host.lowerScales, scales);
    }

    // clientRequestUpdateHigherItemScale forwards (appletIndex+1, newScales) to
    // host.sglUpdateHigherItemScale. Assert the incremented index + scales.
    function test_clientRequestUpdateHigherItemScale() {
        const host = createTemporaryObject(hostComponent, root, {});
        const m = make({appletIndex: 5, host: host});

        const scales = {zoom: 2.3};
        m.clientRequestUpdateHigherItemScale(scales);

        compare(host.higherCalls, 1);
        compare(host.higherIndex, 6); // appletIndex + 1
        compare(host.higherScales, scales);
    }

    // The Connections{ target: host; onSglClearZoom } relay: emitting the host's
    // sglClearZoom while a client is set drives client.sglClearZoom(). Assert the
    // client recorded exactly one relayed call.
    function test_onSglClearZoom_relaysToClient() {
        const host = createTemporaryObject(hostComponent, root, {});
        const client = createTemporaryObject(clientComponent, root, {});
        const m = make({host: host, client: client});

        compare(client.clearCalls, 0);
        host.sglClearZoom();
        compare(client.clearCalls, 1);

        // fires again on a second emission
        host.sglClearZoom();
        compare(client.clearCalls, 2);
    }
}
