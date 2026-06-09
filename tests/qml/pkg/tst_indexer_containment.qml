// Coverage for the containment's Indexer ability. The component is an
// Ability.IndexerPrivate subclass that adds two instrumented functions:
// getClientBridge(index) — a linear scan over the clientsBridges array — and
// appletIdForVisibleIndex(itemVisibleIndex) — which walks the three layout
// grids, calls the inherited visibleIndexBelongsAtApplet/visibleIndex helpers,
// and maps a 1-based visible index back to an applet's plasmoid id.
//
// Unqualified context names the target + its parent read:
//   root      — Indexer.qml's updateIsBlocked binding reads root.dragOverlay
//   layouter  — same binding reads layouter.appletsInParentChange
//   layouts   — IndexerPrivate reads layouts.startLayout/mainLayout/endLayout
// We declare each on the TestCase (the component's creation context) shaped
// like the real object. layouter.appletsInParentChange is held true so
// updateIsBlocked stays true: that disables the five `when: !updateIsBlocked`
// Bindings in IndexerPrivate, so our imperative clientsBridges / separators /
// hidden assignments are NOT clobbered by a re-evaluation. Each test builds a
// fresh layouts Item (no cross-test child leakage from deferred destroy()) and
// pins an observable effect — a return value.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "IndexerContainment"
    when: windowShown

    // Indexer.qml's updateIsBlocked binding reads root.dragOverlay (here, this
    // TestCase). dragOverlay null -> the (root.dragOverlay && ...) term is
    // falsy, so updateIsBlocked falls through to layouter.appletsInParentChange.
    property var dragOverlay: null

    // layouter.appletsInParentChange held true -> updateIsBlocked == true ->
    // the IndexerPrivate Bindings (separators/hidden/clients/clientsBridges/...)
    // stay disabled and never overwrite what we set imperatively.
    QtObject {
        id: layouter
        property bool appletsInParentChange: true
    }

    // A fresh layouts mock per test. IndexerPrivate declares `property Item
    // layouts`, so it must be an Item (a QtObject is silently rejected and the
    // property stays null). Its three grids hold the mock applets the walk
    // iterates over.
    Component {
        id: layoutsComponent
        Item {
            property Item startLayout: startLayoutItem
            property Item mainLayout: mainLayoutItem
            property Item endLayout: endLayoutItem
            Item { id: startLayoutItem }
            Item { id: mainLayoutItem }
            Item { id: endLayoutItem }
        }
    }

    // A mock applet item shaped like AppletItem for the indexer's reads:
    // index, a communicator (null here so each is a single-item applet -> one
    // visible slot), and applet.plasmoid.id resolved through a nested chain.
    Component {
        id: appletItemComponent
        Item {
            property int index: -1
            property var communicator: null
            property bool isSeparator: false
            property bool isHidden: false
            property bool isMarginsAreaSeparator: false
            property QtObject applet: null
        }
    }

    Component {
        id: appletComponent
        QtObject {
            property QtObject plasmoid: QtObject { property int id: -1 }
        }
    }

    // A bridge entry for getClientBridge: it matches on appletIndex.
    Component {
        id: bridgeComponent
        QtObject { property int appletIndex: -1 }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/Indexer.qml")

    // Build a fresh Indexer over a freshly-built layouts mock so no test shares
    // grid children with another (QML destroy() is deferred, so reusing one
    // grid across tests leaks stale applets).
    function make() {
        const layouts = createTemporaryObject(layoutsComponent, root);
        verify(layouts, "layouts mock failed");
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {"layouts": layouts});
        verify(obj, "instantiate failed");
        // updateIsBlocked must be true (layouter.appletsInParentChange) so the
        // inherited array Bindings stay off and our seeded values survive.
        verify(obj.updateIsBlocked, "updateIsBlocked must be true to freeze bindings");
        return obj;
    }

    // Build a mock applet item with a single-item identity (no communicator) and
    // a real plasmoid id, parented into the given grid.
    function makeApplet(parentGrid, index, plasmoidId) {
        const appletObj = appletComponent.createObject(root);
        appletObj.plasmoid.id = plasmoidId;
        const item = appletItemComponent.createObject(parentGrid, {"index": index});
        item.applet = appletObj;
        return item;
    }

    // getClientBridge: empty array -> false; populated -> returns the entry whose
    // appletIndex matches, false when none matches.
    function test_getClientBridge() {
        const m = make();

        // length<=0 early-return branch
        m.clientsBridges = [];
        compare(m.getClientBridge(0), false);

        // populated: build three bridges, two non-matching + one match at idx 7
        const b3 = bridgeComponent.createObject(root, {"appletIndex": 3});
        const b7 = bridgeComponent.createObject(root, {"appletIndex": 7});
        const b9 = bridgeComponent.createObject(root, {"appletIndex": 9});
        m.clientsBridges = [b3, b7, b9];

        // updateIsBlocked freezes the binding, so the assignment held.
        compare(m.clientsBridges.length, 3);

        // first-element match (loop body, i==0)
        compare(m.getClientBridge(3), b3);
        // mid-loop match
        compare(m.getClientBridge(7), b7);
        // last-element match
        compare(m.getClientBridge(9), b9);
        // walks the whole array, no match -> false
        compare(m.getClientBridge(42), false);

        b3.destroy(); b7.destroy(); b9.destroy();
    }

    // appletIdForVisibleIndex: maps a 1-based visible index to the plasmoid id of
    // the matching applet. With three single-item applets across the grids and no
    // separators/hidden, visible indices are 1,2,3 in layout order.
    function test_appletIdForVisibleIndex_resolvesId() {
        const m = make();

        // No separators/hidden so every applet contributes one visible slot.
        m.separators = [];
        m.hidden = [];
        m.marginsAreaSeparators = [];

        // start grid: applet index 0 -> plasmoid id 101 (visible index 1)
        makeApplet(m.layouts.startLayout, 0, 101);
        // main grid: applet index 1 -> plasmoid id 202 (visible index 2)
        makeApplet(m.layouts.mainLayout, 1, 202);
        // end grid: applet index 2 -> plasmoid id 303 (visible index 3)
        makeApplet(m.layouts.endLayout, 2, 303);

        // visibleIndexBelongsAtApplet(applet,1) hits the start-grid applet whose
        // visibleIndex(0) == 1 -> returns its plasmoid id.
        compare(m.appletIdForVisibleIndex(1), 101);
        // main-grid applet, visibleIndex(1) == 2
        compare(m.appletIdForVisibleIndex(2), 202);
        // end-grid applet, visibleIndex(2) == 3
        compare(m.appletIdForVisibleIndex(3), 303);
    }

    // appletIdForVisibleIndex: a visible index past the last applet walks all
    // three grids without a match and returns -1 (the fall-through).
    function test_appletIdForVisibleIndex_noMatch() {
        const m = make();
        m.separators = [];
        m.hidden = [];
        m.marginsAreaSeparators = [];

        makeApplet(m.layouts.startLayout, 0, 101);
        makeApplet(m.layouts.mainLayout, 1, 202);

        // Only visible indices 1 and 2 exist; 5 belongs to no applet -> -1.
        compare(m.appletIdForVisibleIndex(5), -1);
        // Negative index: visibleIndexBelongsAtApplet rejects itemVisibleIndex<0
        // for every applet -> still -1.
        compare(m.appletIdForVisibleIndex(-1), -1);
    }

    // appletIdForVisibleIndex: when the matching applet has a null `applet`
    // (the applet-loaded race), the ternary returns -1 instead of dereferencing.
    function test_appletIdForVisibleIndex_nullApplet() {
        const m = make();
        m.separators = [];
        m.hidden = [];
        m.marginsAreaSeparators = [];

        // start-grid item at visible index 1 but with applet still null.
        const item = appletItemComponent.createObject(m.layouts.startLayout, {"index": 0});
        item.applet = null;

        // visibleIndexBelongsAtApplet matches at index 1, but applet is null ->
        // (appletItem.applet ? ... : -1) yields -1.
        compare(m.appletIdForVisibleIndex(1), -1);
    }
}
