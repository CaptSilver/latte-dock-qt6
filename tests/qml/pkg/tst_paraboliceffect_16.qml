// Coverage spike: drive the client-side ParabolicEffect ability through its
// public functions and signal handlers. Loaded from the staged (instrumented)
// package by file URL so every executed function fires a Cov tick.
//
// The component delegates almost every call either to a `bridge` host (when it
// sits inside a Latte dock) or to its own `local` fallback + restoreZoomTimer
// (when standalone). We instantiate it twice: once with bridge=null to walk the
// local/timer branches, once with a mock bridge to walk the host-delegation
// branches.
import QtQuick
import QtTest
import Stage 1.0

TestCase {
    id: tc
    name: "ParabolicEffect"
    when: windowShown

    readonly property url targetUrl: Stage.qmlModule("org/kde/latte/abilities/client/ParabolicEffect.qml")

    // ----- mock host plumbing -------------------------------------------------
    // Records of what the host received, so assertions can be real.
    property var hostLog: ({})

    function resetLog() {
        hostLog = {
            startTimer: 0, stopTimer: 0, directRendering: undefined,
            currentItem: undefined, currentIndex: undefined, clearZoom: 0,
            lowerScales: undefined, higherScales: undefined, client: undefined
        };
    }

    // A mock parabolic-host object exposing the methods/properties the client
    // reads through `bridge.parabolic.host` and `bridge.parabolic.*`. The host
    // must be an Item: the client binds `ref.parabolic` (typed Item) to
    // `bridge.parabolic.host`, and a QtObject won't assign to an Item property.
    Item {
        id: mockBridge
        property QtObject parabolic: QtObject {
            id: mockBridgeParabolic
            property Item host: hostObj
            property var client: null
            function clientRequestUpdateLowerItemScale(scales) { tc.hostLog.lowerScales = scales; }
            function clientRequestUpdateHigherItemScale(scales) { tc.hostLog.higherScales = scales; }
        }

        Item {
            id: hostObj
            // mirror the AbilityDefinition.ParabolicEffect surface the client reads
            property bool restoreZoomIsBlocked: false
            property var currentParabolicItem: null
            property bool isEnabled: true
            property int spread: 5
            // _privates.directRenderingEnabled is read for `directRenderingEnabled`
            property QtObject _privates: QtObject { property bool directRenderingEnabled: false }
            // factor is bound to by the client (`factor: ref.parabolic.factor`)
            property QtObject factor: QtObject {
                property real zoom: 1.6
                property real maxZoom: 1.6
                property real marginThicknessZoom: 1.0
                property real marginThicknessZoomInPercentage: 1.0
            }
            function startRestoreZoomTimer() { tc.hostLog.startTimer++; }
            function stopRestoreZoomTimer() { tc.hostLog.stopTimer++; }
            function setDirectRenderingEnabled(v) { tc.hostLog.directRendering = v; }
            function setCurrentParabolicItem(i) { tc.hostLog.currentItem = i; }
            function setCurrentParabolicItemIndex(i) { tc.hostLog.currentIndex = i; }
            function sglClearZoom() { tc.hostLog.clearZoom++; }
        }
    }

    // Mock indexer with a fixed item count, read by the host-request + track fns.
    // Must be an Item: the client's `indexer` property is typed Item.
    Item {
        id: mockIndexer
        property int itemsCount: 4
    }

    Item { id: mockLayout }

    // ----- factory ------------------------------------------------------------
    function makeLocal() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, tc, {
            indexer: mockIndexer,
            layout: mockLayout
        });
        verify(obj, "instantiate (local) failed");
        return obj;
    }

    function makeBridged() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, tc, {
            bridge: mockBridge,
            indexer: mockIndexer,
            layout: mockLayout
        });
        verify(obj, "instantiate (bridged) failed");
        return obj;
    }

    // ----- standalone (bridge === null) branches ------------------------------
    function test_local_isActive() {
        resetLog();
        const p = makeLocal();
        verify(!p.isActive);          // bridge is null
        // restoreZoomIsBlocked resolves to local's value (false)
        verify(!p.restoreZoomIsBlocked);
    }

    function test_local_timers_and_directRendering() {
        resetLog();
        const p = makeLocal();
        // setDirectRenderingEnabled -> local._privates.directRenderingEnabled
        p.setDirectRenderingEnabled(true);
        p.setDirectRenderingEnabled(false);
        // start/stop restoreZoomTimer take the local (timer) branch
        p.startRestoreZoomTimer();
        verify(restoreZoomTimerRunning(p));
        p.stopRestoreZoomTimer();
        verify(!restoreZoomTimerRunning(p));
    }

    function test_local_startTimer_blocked() {
        resetLog();
        const p = makeLocal();
        // Block via the local source: restoreZoomIsBlocked binds to local's value
        // when there is no bridge. With it true, startRestoreZoomTimer hits the
        // guard's `return` and never starts the timer.
        p.local.restoreZoomIsBlocked = true;
        verify(p.restoreZoomIsBlocked);
        p.startRestoreZoomTimer();
        verify(!restoreZoomTimerRunning(p));   // early-returned, timer stays stopped
    }

    function test_local_setCurrentParabolicItem() {
        resetLog();
        const p = makeLocal();
        // local branch: setCurrentParabolicItem assigns local.currentParabolicItem,
        // which the client's `currentParabolicItem: ref.parabolic.currentParabolicItem`
        // binding mirrors. Setting it re-evaluates that binding and fires the
        // onCurrentParabolicItemChanged handler.
        p.setCurrentParabolicItem(mockLayout);
        compare(p.local.currentParabolicItem, mockLayout);
        compare(p.currentParabolicItem, mockLayout);
        // The handler's else branch (item set, no bridge) calls stopRestoreZoomTimer,
        // so the local timer must not be running after assigning a non-null item.
        verify(!restoreZoomTimerRunning(p));
        // Clearing it takes the handler's true branch -> startRestoreZoomTimer.
        p.setCurrentParabolicItem(null);
        compare(p.currentParabolicItem, null);
        verify(restoreZoomTimerRunning(p));
    }

    function test_local_restoreZoomBlockedChanged() {
        resetLog();
        const p = makeLocal();
        verify(p.local, "local fallback ability missing");
        // With no bridge the client drives its own restore-zoom timer from this
        // handler: blocking stops it, unblocking starts it again.
        p.local.restoreZoomIsBlocked = true;
        verify(p.restoreZoomIsBlocked);
        verify(!restoreZoomTimerRunning(p), "blocking restore zoom must stop the timer");

        p.local.restoreZoomIsBlocked = false;
        verify(!p.restoreZoomIsBlocked);
        verify(restoreZoomTimerRunning(p), "unblocking restore zoom must start the timer");
    }

    // The timer restores every item to scale 1.0, so it must not fire while an item
    // is still hovered or while restore-zoom is blocked -- the host guards its own
    // copy of this timer the same way.
    function test_local_restoreZoomTimer_doesNotClearWhileAnItemIsCurrent() {
        resetLog();
        const p = makeLocal();

        p.setCurrentParabolicItem(mockLayout);
        p.local.restoreZoomIsBlocked = true;
        p.local.restoreZoomIsBlocked = false;

        var cleared = 0;
        p.sglClearZoom.connect(function() { cleared++; });

        const t = findRestoreZoomTimer(p);
        verify(t, "restore zoom timer not found");
        t.interval = 1;
        t.restart();
        wait(60);

        compare(cleared, 0, "the timer cleared zoom while an item was still current");
    }

    // The host runs its own identical handler, so a bridged client must not drive
    // the host's timer a second time.
    function test_bridged_doesNotRedriveTheHostTimer() {
        resetLog();
        const p = makeBridged();

        hostObj.currentParabolicItem = mockLayout;
        hostObj.currentParabolicItem = null;

        compare(tc.hostLog.startTimer, 0, "client re-drove the host's restore zoom timer");
        compare(tc.hostLog.stopTimer, 0, "client re-drove the host's restore zoom timer");
    }

    function test_local_invkClearZoom() {
        resetLog();
        const p = makeLocal();
        var fired = 0;
        p.sglClearZoom.connect(function() { fired++; });
        p.invkClearZoom();            // not blocked, no bridge -> emits sglClearZoom
        verify(fired >= 1);
    }

    function test_local_hostRequests_emit_signals() {
        resetLog();
        const p = makeLocal();
        var lower = null, higher = null;
        p.sglUpdateLowerItemScale.connect(function(idx, s) { lower = {idx: idx, s: s}; });
        p.sglUpdateHigherItemScale.connect(function(idx, s) { higher = {idx: idx, s: s}; });
        // hostRequestUpdateLowerItemScale -> sglUpdateLowerItemScale(itemsCount-1, ...)
        p.hostRequestUpdateLowerItemScale([1]);
        p.hostRequestUpdateHigherItemScale([1]);
        verify(lower !== null);
        compare(lower.idx, mockIndexer.itemsCount - 1);
        verify(higher !== null);
        compare(higher.idx, 0);
    }

    function test_local_restoreZoomTimer_fires() {
        resetLog();
        const p = makeLocal();
        const t = findRestoreZoomTimer(p);
        verify(t, "could not find restoreZoomTimer");
        var cleared = 0;
        p.sglClearZoom.connect(function() { cleared++; });
        t.interval = 1;
        t.restart();
        // onTriggered: setDirectRenderingEnabled(false) + invkClearZoom() (no
        // bridge -> emits sglClearZoom).
        tryVerify(function() { return cleared >= 1; }, 2000, "restoreZoomTimer onTriggered did not fire");
    }

    // ----- bridged (bridge !== null) branches ---------------------------------
    function test_bridged_isActive_and_client() {
        resetLog();
        const p = makeBridged();
        verify(p.isActive);
        // Component.onCompleted (isActive) sets bridge.parabolic.client = p.
        compare(mockBridgeParabolic.client, p);
    }

    function test_bridged_timers_delegate_to_host() {
        resetLog();
        const p = makeBridged();
        p.startRestoreZoomTimer();
        p.stopRestoreZoomTimer();
        compare(tc.hostLog.startTimer, 1);
        compare(tc.hostLog.stopTimer, 1);
    }

    function test_bridged_directRendering_and_items_delegate() {
        resetLog();
        const p = makeBridged();
        p.setDirectRenderingEnabled(true);
        compare(tc.hostLog.directRendering, true);
        p.setCurrentParabolicItem(mockLayout);
        compare(tc.hostLog.currentItem, mockLayout);
        p.setCurrentParabolicItemIndex(7);
        compare(tc.hostLog.currentIndex, 7);
    }

    function test_bridged_invkClearZoom_delegates() {
        resetLog();
        const p = makeBridged();
        p.invkClearZoom();            // host.restoreZoomIsBlocked false -> sglClearZoom on host
        compare(tc.hostLog.clearZoom, 1);
    }

    function test_bridged_sltTrack_clientRequests() {
        resetLog();
        const p = makeBridged();
        // delegateIndex === -1 -> clientRequestUpdateLowerItemScale
        p.sltTrackLowerItemScale(-1, [0.5, 1]);
        verify(tc.hostLog.lowerScales !== undefined);
        // clear-requested ([1]) with delegateIndex>=0 -> also lower request
        p.sltTrackLowerItemScale(0, [1]);
        // higher: delegateIndex >= itemsCount -> higher request
        p.sltTrackHigherItemScale(mockIndexer.itemsCount, [0.5, 1]);
        verify(tc.hostLog.higherScales !== undefined);
        // clear-requested ([1]) with delegateIndex < itemsCount -> higher request
        p.sltTrackHigherItemScale(0, [1]);
    }

    function test_bridged_hostRequests_with_indexer() {
        resetLog();
        const p = makeBridged();
        var lower = null, higher = null;
        p.sglUpdateLowerItemScale.connect(function(idx, s) { lower = idx; });
        p.sglUpdateHigherItemScale.connect(function(idx, s) { higher = idx; });
        p.hostRequestUpdateLowerItemScale([0.8, 1]);
        p.hostRequestUpdateHigherItemScale([0.8, 1]);
        compare(lower, mockIndexer.itemsCount - 1);
        compare(higher, 0);
    }

    // ----- helpers ------------------------------------------------------------
    function findRestoreZoomTimer(p) {
        const res = p.resources;
        for (var i = 0; i < res.length; i++) {
            // The lone Timer in the component; match on its default 50ms interval.
            if (res[i] && res[i].hasOwnProperty("interval") && res[i].running !== undefined
                    && typeof res[i].restart === "function") {
                return res[i];
            }
        }
        return null;
    }

    function restoreZoomTimerRunning(p) {
        const t = findRestoreZoomTimer(p);
        return t ? t.running : false;
    }
}
