// Coverage for the client-side ThinTooltip ability. It subclasses the empty
// AbilityDefinition.ThinTooltip and forwards show()/hide() either to a live
// bridge host or to its own `local` definition, and wires its `client` back
// into the bridge whenever it becomes active (bridge !== null).
//
// The component reads NO unqualified creation-context names — everything it
// touches hangs off its own settable `property Item bridge` (or the inner
// `ref`/`local` objects it owns). So the honest mock is just an Item shaped
// like the real bridge: a `thinTooltip` sub-item carrying a `host` (the thing
// show/hide forward to) and a writable `client` slot the activation handlers
// assign. `host` must be an Item, not a QtObject, because the component's
// declarative bindings (ref.thinTooltip resolves to bridge.thinTooltip.host)
// feed properties typed `Item`/`string`/`bool` on the definition base.
//
// Headless verdict: show/hide (both branches), onIsActiveChanged, and
// Component.onCompleted's active branch are all driveable with observable
// effects (mock host call records, the bridge.client assignment). The
// Component.onDestruction handler is routed live-only — its only effect is
// clearing bridge.client during temp-object teardown, which no headless
// assertion can observe deterministically (same call as ParabolicEffect's
// destruction handler in live-only.md).
import QtQuick
import QtTest

TestCase {
    id: root
    name: "ThinTooltipClient"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/client/ThinTooltip.qml")

    // A target Item for show/hide's first argument, so the host mock can record
    // the exact object the component forwarded.
    Item { id: visualParentA }
    Item { id: visualParentB }

    // The bridge mock the client's `bridge` property points at. Shaped exactly
    // like the real bridge surface the client reads: bridge.thinTooltip.host
    // (the forward target / activation source) and a writable bridge.thinTooltip.client.
    // The host is a child Item built inline so bridge.thinTooltip.host is never
    // momentarily null during incubation (the client's ref.thinTooltip binding
    // dereferences it). The host is typed Item because the definition base's
    // currentVisualParent is Item and the client's bindings read it through
    // ref.thinTooltip; it records every show/hide call so the bridge-forward
    // path is asserted by a real side-effect.
    Component {
        id: bridgeComponent
        Item {
            id: bridgeRoot
            property alias host: hostItem
            property QtObject thinTooltip: QtObject {
                property Item host: hostItem
                property var client: null
            }
            Item {
                id: hostItem
                property int showCalls: 0
                property int hideCalls: 0
                property var lastShowParent: null
                property string lastShowText: ""
                property var lastHideParent: null
                property bool isEnabled: true
                property Item currentVisualParent: null
                property string currentText: "hosttext"
                function show(visualParent, text) {
                    showCalls++;
                    lastShowParent = visualParent;
                    lastShowText = text;
                }
                function hide(visualParent) {
                    hideCalls++;
                    lastHideParent = visualParent;
                }
            }
        }
    }

    function makeBridge() {
        return createTemporaryObject(bridgeComponent, root, {});
    }

    function make(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, props ? props : {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // show@27 / hide@35, NO-bridge branch: bridge stays null so each forwards to
    // `local.show()/hide()` (the empty definition base). Observable effect: the
    // call returns without throwing AND the (unattached) host was never touched.
    function test_show_hide_localBranch() {
        const bridge = makeBridge();
        const m = make({});           // bridge defaults to null
        compare(m.isActive, false);

        m.show(visualParentA, "hello");
        m.hide(visualParentA);
        // local branch must not have reached any bridge host.
        compare(bridge.host.showCalls, 0);
        compare(bridge.host.hideCalls, 0);
    }

    // show@27 / hide@35, WITH-bridge branch: forwards to bridge.thinTooltip.host.
    // Assert the host mock recorded the exact visualParent + text.
    function test_show_hide_bridgeBranch() {
        const bridge = makeBridge();
        const m = make({bridge: bridge});
        compare(m.isActive, true);

        m.show(visualParentB, "tipText");
        compare(bridge.host.showCalls, 1);
        compare(bridge.host.lastShowParent, visualParentB);
        compare(bridge.host.lastShowText, "tipText");

        m.hide(visualParentB);
        compare(bridge.host.hideCalls, 1);
        compare(bridge.host.lastHideParent, visualParentB);
    }

    // onIsActiveChanged@43: bridge null -> a real bridge flips isActive
    // false -> true, and the handler writes bridge.thinTooltip.client = thinTooltip.
    function test_onIsActiveChanged_wiresClient() {
        const bridge = makeBridge();
        const m = make({});           // start inactive
        compare(m.isActive, false);
        compare(bridge.thinTooltip.client, null);

        m.bridge = bridge;            // isActive false -> true, handler fires
        compare(m.isActive, true);
        compare(bridge.thinTooltip.client, m);
    }

    // Component.onCompleted@49: constructing WITH bridge already set means
    // isActive is true at completion, so onCompleted's active branch wires the
    // client. Build it directly with the bridge so the completion handler — not
    // the change handler — does the assignment.
    function test_onCompleted_wiresClient() {
        const bridge = makeBridge();
        bridge.thinTooltip.client = null;
        const m = make({bridge: bridge});
        compare(m.isActive, true);
        // set at completion (and, harmlessly, the same value the change handler
        // would have written); either way client now points at the instance.
        compare(bridge.thinTooltip.client, m);
    }
}
