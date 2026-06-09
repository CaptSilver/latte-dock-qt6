// Coverage for the client-side Indexer ability
// (declarativeimports/abilities/client/Indexer.qml). The component derives from
// AbilityDefinition.Indexer (an Item carrying `separators`/`hidden` arrays) and
// reads three creation-context names unqualified: `bridge`, `layout`, and
// `indexer`. In the real AppletAbilities the `indexer` alias points back at this
// same client object; headless we shadow it with a sentinel on `root` and assert
// the activate/destroy handlers write it into bridge.indexer.client.
//
// Instrumented units (the ones the staged copy injects Cov.tick into):
//   onIsActiveChanged@40, Component.onCompleted@46, Component.onDestruction@52,
//   visibleIndex@217. The eight Bindings are NOT instrumented, but they run for
//   real off `layout.children` to populate the *VisibleItemIndex/hidden/separators
//   state that visibleIndex consumes, so visibleIndex is exercised against honest
//   bound values.
//
// Every test asserts an observable effect: a returned value from visibleIndex, or
// the bridge.indexer.client mock side-effect the lifecycle handlers produce.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "IndexerClient"
    when: windowShown

    // The bridge mock. Shaped like the real containment bridge: only the one
    // path the component touches — bridge.indexer.{client, host, appletIndex,
    // inMarginsArea, ...}. `client` is writable so the lifecycle handlers' write
    // is observable; host.visibleIndex feeds the visibleIndex base offset.
    QtObject {
        id: bridgeIndexer
        property var client: undefined
        property int appletIndex: 5
        property bool inMarginsArea: false
        property bool tailAppletIsSeparator: false
        property bool headAppletIsSeparator: false
        property QtObject host: QtObject {
            // Records the appletIndex it was asked about, returns a fixed base.
            property int lastAskedIndex: -999
            function visibleIndex(appletIndex) { lastAskedIndex = appletIndex; return 10; }
        }
    }

    // bridge must be an Item — the component's `bridge` slot is `property Item`,
    // and a QtObject silently stays null there (README rule 2).
    Item {
        id: bridgeMock
        property QtObject indexer: bridgeIndexer
    }

    // `indexer` creation-context name: in production it aliases the client object
    // itself. We hand the handlers a recognizable sentinel and assert the bridge
    // received exactly it.
    property var indexerSentinel: ({ tag: "client-sentinel" })
    property var indexer: indexerSentinel

    // The `layout` slot is also `property Item`; its children drive every Binding
    // and the visibleIndex loop. Each child carries the itemIndex/isSeparator/
    // isHidden/isSeparatorHidden fields the bindings read.
    component LayoutChild: Item {
        property int itemIndex: -1
        property bool isSeparator: false
        property bool isHidden: false
        property bool isSeparatorHidden: false
    }

    // Layout with 4 children, itemIndex 0..3, index 2 a separator. So the visible
    // set is {0,1,3}; firstVisibleItemIndex=0, lastVisibleItemIndex=3,
    // separators=[2], hidden=[].
    Item {
        id: layoutMock
        LayoutChild { itemIndex: 0 }
        LayoutChild { itemIndex: 1 }
        LayoutChild { itemIndex: 2; isSeparator: true }
        LayoutChild { itemIndex: 3 }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/client/Indexer.qml")

    function make(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, props || {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // Reset the bridge's recorded client so each lifecycle assertion is absolute.
    function resetBridge() {
        bridgeIndexer.client = undefined;
        bridgeIndexer.host.lastAskedIndex = -999;
    }

    // Component.onCompleted@46: created with bridge already set -> isActive is
    // true at completion, so the handler writes bridge.indexer.client = indexer.
    function test_onCompleted_setsClientWhenActive() {
        resetBridge();
        const m = make({bridge: bridgeMock, layout: layoutMock});
        compare(m.isActive, true);
        // the completed handler ran and stored our sentinel
        compare(bridgeIndexer.client, root.indexerSentinel);
    }

    // Component.onCompleted@46, inactive branch: created with bridge null ->
    // isActive false, the handler runs but its if-guard is false so the client
    // stays untouched. (The tick still fires; the asserted effect is the absence
    // of the write plus the false isActive.)
    function test_onCompleted_inactiveLeavesClient() {
        resetBridge();
        const m = make({bridge: null, layout: layoutMock});
        compare(m.isActive, false);
        compare(bridgeIndexer.client, undefined);
    }

    // onIsActiveChanged@40: start inactive, then assign a non-null bridge.
    // isActive flips false->true, the changed handler runs the active branch and
    // writes the client.
    function test_onIsActiveChanged_activate() {
        resetBridge();
        const m = make({bridge: null, layout: layoutMock});
        compare(m.isActive, false);
        compare(bridgeIndexer.client, undefined);

        m.bridge = bridgeMock;          // null -> non-null toggles isActive
        compare(m.isActive, true);
        compare(bridgeIndexer.client, root.indexerSentinel);
    }

    // Component.onDestruction@52: with a live bridge, destroying the instance runs
    // the destruction handler's active branch, clearing bridge.indexer.client back
    // to null. The bridge outlives the component (it's retained on root), so the
    // cleared value is observable after teardown.
    function test_onDestruction_clearsClient() {
        resetBridge();
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = c.createObject(root, {bridge: bridgeMock, layout: layoutMock});
        verify(obj, "instantiate failed");
        // onCompleted set it
        compare(bridgeIndexer.client, root.indexerSentinel);

        obj.destroy();
        // onDestruction's active branch nulls the retained bridge's client
        tryCompare(bridgeIndexer, "client", null, 3000,
                   "onDestruction did not clear bridge.indexer.client");
    }

    // visibleIndex@217, in-range visible task: layout {0,1,sep2,3}, ask for
    // taskIndex 3. base = host.visibleIndex(appletIndex=5) = 10; the loop adds 1
    // for each non-hidden non-separator child with itemIndex<3 -> items 0 and 1
    // (2 is a separator, skipped) -> +2 -> 12. Asserts the return value AND that
    // the host was queried with the bridge's appletIndex.
    function test_visibleIndex_inRange() {
        resetBridge();
        const m = make({bridge: bridgeMock, layout: layoutMock});
        // bindings populated from layoutMock
        compare(m.firstVisibleItemIndex, 0);
        compare(m.lastVisibleItemIndex, 3);
        compare(m.visibleItemsCount, 3);
        compare(m.itemsCount, 4);

        compare(m.visibleIndex(3), 12);
        compare(bridgeIndexer.host.lastAskedIndex, 5);
    }

    // visibleIndex@217, out-of-range / separator / past-last branches all return
    // -1 via the early guard. Drives every disjunct of the guard.
    function test_visibleIndex_outOfRange() {
        resetBridge();
        const m = make({bridge: bridgeMock, layout: layoutMock});
        compare(m.firstVisibleItemIndex, 0);
        compare(m.lastVisibleItemIndex, 3);

        // taskIndex is a separator (in separators[]) -> -1
        compare(m.visibleIndex(2), -1);
        // taskIndex past lastVisibleItemIndex -> -1
        compare(m.visibleIndex(99), -1);
        // taskIndex below firstVisibleItemIndex -> -1
        compare(m.visibleIndex(-1), -1);
        // the early guard returned before touching the host
        compare(bridgeIndexer.host.lastAskedIndex, -999);
    }

    // visibleIndex@217 without a bridge: the `if (bridge)` host-offset branch is
    // skipped (vindex starts at -1), only the loop contributes. Asks for the first
    // visible item (0): no child has itemIndex<0, so vindex stays -1.
    function test_visibleIndex_noBridge() {
        resetBridge();
        const m = make({bridge: null, layout: layoutMock});
        compare(m.isActive, false);
        // first visible index is 0 here; visibleIndex(0) walks the loop, nothing
        // is < 0, base stayed -1 (no bridge) -> -1.
        compare(m.visibleIndex(0), -1);
        // visibleIndex(1): item 0 is < 1 and visible -> -1 + 1 = 0
        compare(m.visibleIndex(1), 0);
        // host was never consulted (no bridge)
        compare(bridgeIndexer.host.lastAskedIndex, -999);
    }
}
