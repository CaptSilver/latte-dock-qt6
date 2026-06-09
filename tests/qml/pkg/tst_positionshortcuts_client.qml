// Drives the client PositionShortcuts ability (abilities/client/PositionShortcuts.qml),
// an AbilityDefinition.PositionShortcuts subclass that wires a position-shortcut
// client into its host through a `bridge`.
//
// The component reads no unqualified creation-context names in the units we
// claim: `bridge` and `indexer` are its OWN declared `property Item`s (settable
// on the instance), and `shortcuts`/`isActive`/`ref`/`local` are ids/props it
// declares itself. So the mocks are passed as instance properties at
// createTemporaryObject time, shaped like the real host objects.
//
// Bridge is `property Item bridge`, so the mock MUST be an Item (a QtObject
// would silently stay null and isActive would never flip). The host shape we
// give it carries exactly what the claimed units read:
//   - isActive/onIsActiveChanged/onCompleted write  bridge.shortcuts.client
//   - shortcutIndex reads  bridge.shortcuts.host.unifiedGlobalShortcuts,
//     bridge.indexer.host.visibleIndex, bridge.shortcuts.appletIndex
// indexer is `property Item indexer`; shortcutIndex calls indexer.visibleIndex.
//
// Component.onDestruction@52 is live-only (teardown-only, its sole effect —
// bridge.shortcuts.client=null — fires during temp-object incubation cleanup
// where no assertion can observe it), mirroring the ParabolicEffect/ThinTooltip
// destruction entries already in live-only.md.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "PositionShortcutsClient"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/client/PositionShortcuts.qml")

    // A bridge host shaped like the real one. The component's `ref.shortcuts` is
    // a `property Item` bound to `bridge.shortcuts.host`, so that host MUST be an
    // Item (a QtObject can't assign into the Item slot and would leave it null,
    // making the badge bindings deref null). shortcuts.host carries the badge
    // defaults the component binds to plus the unifiedGlobalShortcuts flag and the
    // appletIdStealing field isEnabled/shortcutIndex read; shortcuts.client is the
    // writable sink the active-wiring handlers fill. indexer.host.visibleIndex is
    // the host-side index lookup shortcutIndex uses in its non-unified branch; it
    // records its argument and returns a fixed base.
    Component {
        id: bridgeComponent
        Item {
            property QtObject shortcuts: QtObject {
                property var client: null
                property int appletIndex: 0
                property int appletIdStealingPositionShortcuts: -1
                property Item host: Item {
                    property bool unifiedGlobalShortcuts: true
                    property bool showPositionShortcutBadges: false
                    property var badges: ['a', 'b', 'c']
                    property int appletIdStealingPositionShortcuts: -1
                }
            }
            property QtObject indexer: QtObject {
                property Item host: Item {
                    property int lastVisibleArg: -999
                    function visibleIndex(i) { lastVisibleArg = i; return 100; }
                }
            }
        }
    }

    // The client-side indexer the component owns (`property Item indexer`).
    // shortcutIndex forwards entryIndex to indexer.visibleIndex; record the
    // argument and return a value we can pin in the assertions.
    Component {
        id: indexerComponent
        Item {
            property int lastVisibleArg: -999
            function visibleIndex(i) { lastVisibleArg = i; return 7 + i; }
        }
    }

    function makeWith(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, props);
        verify(obj, "instantiate failed");
        return obj;
    }

    // No bridge -> isActive is false. Component.onCompleted@46 ticks at
    // construction; its body is gated on isActive, so the write is NOT performed.
    // isActive==false and the badge bindings fall back to the local definition
    // defaults (showPositionShortcutBadges=false). Asserts the inactive entry path.
    function test_onCompleted_inactive_noClientWrite() {
        const m = makeWith({});
        compare(m.isActive, false);
        // local-definition fallback: ref.shortcuts === local, default badges off.
        compare(m.showPositionShortcutBadges, false);
        // isEnabled returns true when there is no bridge.
        compare(m.isEnabled, true);
    }

    // With a bridge present at construction, isActive is true, so
    // Component.onCompleted@46 runs its active branch and writes
    // bridge.shortcuts.client = shortcuts. Assert the host sink received the
    // component instance.
    function test_onCompleted_active_writesClient() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        verify(bridge, "bridge mock failed");
        const m = makeWith({bridge: bridge});
        compare(m.isActive, true);
        // onCompleted's active branch wired the client into the host.
        compare(bridge.shortcuts.client, m);
    }

    // onIsActiveChanged@40: flip isActive false -> true by assigning a bridge
    // after construction. The handler's active branch writes the client sink.
    // Start with no bridge (client untouched), then set it and assert the write.
    function test_onIsActiveChanged_writesClientOnActivation() {
        const m = makeWith({});
        compare(m.isActive, false);

        const bridge = createTemporaryObject(bridgeComponent, root, {});
        verify(bridge, "bridge mock failed");
        bridge.shortcuts.client = null;

        m.bridge = bridge;            // isActive false -> true, fires onIsActiveChanged
        compare(m.isActive, true);
        compare(bridge.shortcuts.client, m);
    }

    // shortcutIndex@58, unified branch: with no bridge the `!bridge` leg short-
    // circuits and it returns indexer.visibleIndex(entryIndex) directly. Assert
    // the client indexer was called with the right argument and its value passed
    // straight through.
    function test_shortcutIndex_noBridge_passesThrough() {
        const idx = createTemporaryObject(indexerComponent, root, {});
        verify(idx, "indexer mock failed");
        const m = makeWith({indexer: idx});

        const result = m.shortcutIndex(3);
        compare(idx.lastVisibleArg, 3);
        compare(result, 7 + 3);       // indexer.visibleIndex(3) returned 10
    }

    // shortcutIndex@58, unified branch with a bridge whose host has
    // unifiedGlobalShortcuts=true: the OR short-circuits on the host flag and it
    // again returns indexer.visibleIndex(entryIndex) without touching the host
    // indexer. Assert the client indexer ran and the host index lookup did not.
    function test_shortcutIndex_unifiedBridge_passesThrough() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        bridge.shortcuts.host.unifiedGlobalShortcuts = true;
        const idx = createTemporaryObject(indexerComponent, root, {});
        const m = makeWith({bridge: bridge, indexer: idx});

        const result = m.shortcutIndex(2);
        compare(idx.lastVisibleArg, 2);
        compare(result, 7 + 2);                       // 9
        // unified leg returned early, host-side visibleIndex was never consulted.
        compare(bridge.indexer.host.lastVisibleArg, -999);
    }

    // shortcutIndex@58, non-unified branch: bridge present and
    // unifiedGlobalShortcuts=false forces the second path. It computes
    //   base = bridge.indexer.host.visibleIndex(bridge.shortcuts.appletIndex)
    //   return indexer.visibleIndex(entryIndex) - base + 1
    // Pin both indexer calls and the arithmetic.
    function test_shortcutIndex_nonUnified_offsetsByBase() {
        const bridge = createTemporaryObject(bridgeComponent, root, {});
        bridge.shortcuts.host.unifiedGlobalShortcuts = false;
        bridge.shortcuts.appletIndex = 5;
        const idx = createTemporaryObject(indexerComponent, root, {});
        const m = makeWith({bridge: bridge, indexer: idx});

        const result = m.shortcutIndex(4);
        // client indexer called with the entry index
        compare(idx.lastVisibleArg, 4);
        // host indexer called with the bridge's appletIndex -> base = 100
        compare(bridge.indexer.host.lastVisibleArg, 5);
        // (7 + 4) - 100 + 1 = -88
        compare(result, (7 + 4) - 100 + 1);
    }
}
