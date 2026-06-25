// Drives the client Animations ability (abilities/client/Animations.qml), an
// AbilityDefinition.Animations subclass that either forwards to a host through a
// `bridge` or falls back to its own `local` AbilityDefinition.Animations.
//
// Unqualified creation-context names: none that we claim. `bridge` is the
// component's OWN `property Item bridge` (settable per instance); `ref`/`local`
// are ids it declares itself; `LatteCore.Environment` is the C++ core singleton
// the import brings in. So the mock host is passed as the `bridge` instance
// property at createTemporaryObject time, shaped like the real host.
//
// `ref.animations` is `bridge ? bridge.animations.host : local` and is a
// `property Item`, so the host mock MUST be an Item (a QtObject can't assign into
// the Item slot — ref.animations would deref null). animations.client is the
// writable sink onBridgeIsActiveChanged@49 / Component.onCompleted@55 fill.
//
// Component.onDestruction@61 is teardown-only (its sole effect,
// bridge.animations.client=null, fires during temp-object incubation cleanup
// where no assertion can observe it) — live-only, mirroring the
// PositionShortcuts/ParabolicEffect destruction entries.
import QtQuick
import QtTest

import org.kde.latte.abilities.definition 0.1 as AbilityDefinition

TestCase {
    id: root
    name: "AnimationsClient"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/client/Animations.qml")

    // A bridge host shaped like the real one. The client binds its typed
    // duration/speedFactor/needBothAxis/... properties (AbilityDefinition.Duration
    // /SpeedFactor/Tracker) to ref.animations.*, so the host's sub-objects MUST be
    // those exact types — a plain QtObject won't assign into the typed slots. The
    // simplest correctly-typed host is a real AbilityDefinition.Animations with
    // values set distinct from the local-definition defaults, so a delegation test
    // can tell which path is live. animations.client is the writable sink the
    // active-wiring handlers fill.
    Component {
        id: bridgeComponent
        Item {
            property QtObject animations: QtObject {
                property var client: null
                property Item host: AbilityDefinition.Animations {
                    hoverPixelSensitivity: 9
                    duration.small: 11
                    duration.large: 22
                    speedFactor.normal: 2.0
                    speedFactor.current: 3.0
                }
            }
        }
    }

    function makeWith(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, props);
        verify(obj, "instantiate failed");
        return obj;
    }

    // No bridge -> bridgeIsActive false. ref.animations === local, so the client
    // tracks the local AbilityDefinition fallback. local.active is
    // `speedFactor.current !== 0`; the definition default current is 1.0, so the
    // client reports active and the local speedFactor/duration defaults flow up.
    // Component.onCompleted@55 ticks here too; its body is gated on bridgeIsActive
    // (false), so no client write happens.
    function test_noBridge_tracksLocalFallback() {
        const m = makeWith({});
        compare(m.bridgeIsActive, false);
        // local.active = (speedFactor.current !== 0); definition default current=1.0
        compare(m.active, true);
        compare(m.speedFactor.current, 1.0);
        // local sets hoverPixelSensitivity:1
        compare(m.hoverPixelSensitivity, 1);
        // duration.large/small come from LatteCore.Environment via the local def;
        // both are real (non-negative) durations, distinct from the host's 11/22.
        verify(m.duration.large >= 0);
        verify(m.duration.small >= 0);
        compare(m.duration.large !== 22, true);
    }

    // With a bridge at construction, bridgeIsActive is true so ref.animations
    // becomes bridge.animations.host: every tracked property delegates to the
    // host's values, NOT the local defaults.
    function test_bridge_delegatesToHost() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        verify(bridge, "bridge mock failed");
        const m = makeWith({bridge: bridge});
        compare(m.bridgeIsActive, true);
        compare(m.hoverPixelSensitivity, 9);
        compare(m.duration.small, 11);
        compare(m.duration.large, 22);
        compare(m.speedFactor.current, 3.0);
        compare(m.speedFactor.normal, 2.0);
        // host base-definition active default is false; the client delegates it,
        // so the no-bridge "active==true" local fallback is NOT in effect here.
        compare(m.active, false);
        // host trackers default count 0 -> hasThicknessAnimation false (delegated).
        compare(m.hasThicknessAnimation, false);
    }

    // Component.onCompleted@55 active branch: a bridge present at construction
    // makes bridgeIsActive true, so onCompleted writes
    // bridge.animations.client = _animations. Assert the host sink got the
    // component instance.
    function test_onCompleted_active_writesClient() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        verify(bridge, "bridge mock failed");
        bridge.animations.client = null;
        const m = makeWith({bridge: bridge});
        compare(m.bridgeIsActive, true);
        compare(bridge.animations.client, m);
    }

    // onBridgeIsActiveChanged@49: flip bridgeIsActive false -> true by assigning a
    // bridge after construction. Start with no bridge (client untouched), then set
    // it and assert the handler's active branch wired the client sink.
    function test_onBridgeIsActiveChanged_writesClientOnActivation() {
        const m = makeWith({});
        compare(m.bridgeIsActive, false);

        const bridge = createTemporaryObject(bridgeComponent, root, {});
        verify(bridge, "bridge mock failed");
        bridge.animations.client = null;

        m.bridge = bridge;            // bridgeIsActive false -> true, fires handler
        compare(m.bridgeIsActive, true);
        compare(bridge.animations.client, m);
        // and the delegation switched over too
        compare(m.duration.small, 11);
    }
}
