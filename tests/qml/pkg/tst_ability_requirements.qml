// Coverage for the client Requirements ability (abilities/client/Requirements.qml),
// an AppletRequirements subclass that mirrors its six requirement booleans onto a
// host applet through a `bridge`. The component reads no unqualified context name —
// `bridge` is its own `property Item bridge`, and the six requirement booleans are
// inherited slots. So every value it touches is set directly on the instance.
//
// `isActive` is derived (`bridge !== null`). Going active runs onIsActiveChanged,
// which pushes all six current values to bridge.applet.*. Each requirement's own
// changed-handler then forwards just that one value while active. The bridge mock
// must be an Item (the slot is `property Item bridge`; a QtObject would stay null
// and every forward would silently no-op), carrying an `applet` sub-object with the
// six writable booleans the handlers assign.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "AbilityRequirements"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/client/Requirements.qml")

    // bridge mock: an Item so `property Item bridge` actually binds, exposing the
    // `applet` the handlers write to. The applet booleans start at the opposite of
    // what the tests push so an assertion can't pass by accident.
    Component {
        id: bridgeComponent
        Item {
            property QtObject applet: QtObject {
                property bool activeIndicatorEnabled: false
                property bool latteSideColoringEnabled: false
                property bool lengthMarginsEnabled: false
                property bool parabolicEffectLocked: true
                property bool screenEdgeMarginSupported: true
                property bool windowsTrackingEnabled: true
            }
        }
    }

    function make(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, props === undefined ? {} : props);
        verify(obj, "instantiate failed");
        return obj;
    }

    // Going active (bridge null -> set) runs onIsActiveChanged, which flushes all
    // six current requirement values onto the applet. Assert isActive flipped and
    // every applet boolean now matches the requirement's value.
    function test_onIsActiveChanged_flushesAllRequirements() {
        const m = make({
            activeIndicatorEnabled: true,
            latteSideColoringEnabled: true,
            lengthMarginsEnabled: true,
            parabolicEffectLocked: false,
            screenEdgeMarginSupported: false,
            windowsTrackingEnabled: false
        });
        compare(m.isActive, false); // no bridge yet

        const bridge = createTemporaryObject(bridgeComponent, root, {});
        // sanity: applet starts opposite, so the flush is observable
        compare(bridge.applet.activeIndicatorEnabled, false);
        compare(bridge.applet.parabolicEffectLocked, true);

        m.bridge = bridge;
        compare(m.isActive, true);

        compare(bridge.applet.activeIndicatorEnabled, true);
        compare(bridge.applet.latteSideColoringEnabled, true);
        compare(bridge.applet.lengthMarginsEnabled, true);
        compare(bridge.applet.parabolicEffectLocked, false);
        compare(bridge.applet.screenEdgeMarginSupported, false);
        compare(bridge.applet.windowsTrackingEnabled, false);
    }

    // onActiveIndicatorEnabledChanged forwards only that value while active.
    function test_onActiveIndicatorEnabledChanged_forwards() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        const m = make({activeIndicatorEnabled: false, bridge: bridge});
        compare(bridge.applet.activeIndicatorEnabled, false);

        m.activeIndicatorEnabled = true;
        compare(bridge.applet.activeIndicatorEnabled, true);

        m.activeIndicatorEnabled = false;
        compare(bridge.applet.activeIndicatorEnabled, false);
    }

    // onLatteSideColoringEnabledChanged forwards its value.
    function test_onLatteSideColoringEnabledChanged_forwards() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        const m = make({latteSideColoringEnabled: false, bridge: bridge});
        compare(bridge.applet.latteSideColoringEnabled, false);

        m.latteSideColoringEnabled = true;
        compare(bridge.applet.latteSideColoringEnabled, true);
    }

    // onLengthMarginsEnabledChanged forwards its value.
    function test_onLengthMarginsEnabledChanged_forwards() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        const m = make({lengthMarginsEnabled: false, bridge: bridge});
        compare(bridge.applet.lengthMarginsEnabled, false);

        m.lengthMarginsEnabled = true;
        compare(bridge.applet.lengthMarginsEnabled, true);
    }

    // onParabolicEffectLockedChanged forwards its value.
    function test_onParabolicEffectLockedChanged_forwards() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        const m = make({parabolicEffectLocked: false, bridge: bridge});
        compare(bridge.applet.parabolicEffectLocked, false);

        m.parabolicEffectLocked = true;
        compare(bridge.applet.parabolicEffectLocked, true);
    }

    // onScreenEdgeMarginSupportedChanged forwards its value.
    function test_onScreenEdgeMarginSupportedChanged_forwards() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        const m = make({screenEdgeMarginSupported: false, bridge: bridge});
        compare(bridge.applet.screenEdgeMarginSupported, false);

        m.screenEdgeMarginSupported = true;
        compare(bridge.applet.screenEdgeMarginSupported, true);
    }

    // onWindowsTrackingEnabledChanged forwards its value.
    function test_onWindowsTrackingEnabledChanged_forwards() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        const m = make({windowsTrackingEnabled: false, bridge: bridge});
        compare(bridge.applet.windowsTrackingEnabled, false);

        m.windowsTrackingEnabled = true;
        compare(bridge.applet.windowsTrackingEnabled, true);
    }

    // The guard: with no bridge, isActive is false and a requirement change must
    // NOT throw (the handler's `if (isActive)` short-circuits). Asserting the
    // property still took its new value confirms the inactive path executed cleanly.
    function test_inactive_changesDoNotThrow() {
        const m = make({windowsTrackingEnabled: false});
        compare(m.isActive, false);
        m.windowsTrackingEnabled = true;
        compare(m.windowsTrackingEnabled, true);
    }
}
