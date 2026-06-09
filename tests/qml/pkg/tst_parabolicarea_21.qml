// Coverage spike for the containment ParabolicArea delegate, loaded from the
// staged (instrumented) package so each executed unit fires a Cov tick.
//
// ParabolicArea reads a pile of unqualified context names (appletItem,
// parabolic, communicator, wrapper, applet, index, parabolicAreaLoader,
// restoreAnimation, the separator flags, parabolicEffectIsSupported, and root).
// QML resolves those against the component's creation context, so this TestCase
// is `id: root` and declares every one of those names as a real mock shaped
// like the object the component actually pokes at. With the context supplied,
// construction, Component.onCompleted, and every public function/handler run to
// completion instead of throwing on the first undefined global, which lets the
// assertions check observable effects: wrapper.zoomScale writes, the parabolic
// signal re-emissions, and the mock thinTooltip / setCurrentParabolicItem side
// effects.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "ParabolicArea"
    when: windowShown
    visible: true
    width: 200
    height: 200

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/applet/ParabolicArea.qml")

    // ----- context the component reads (creation-context resolution) ----------
    property bool isHorizontal: true

    // unqualified `parabolicEffectIsSupported` read inside updateScale (line 160)
    property bool parabolicEffectIsSupported: true

    // separator flags read in onParabolicEntered + sltUpdateItemScale
    property bool isSeparator: false
    property bool isSpacer: false
    property bool isMarginsAreaSeparator: false

    property int index: 2

    // captured side effects, reset per-test
    property var log: ({})
    function resetLog() {
        log = {
            tooltipShow: 0, tooltipHide: 0, setCurrentItem: undefined,
            setCurrentIndex: undefined, restoreStop: 0,
            lowerEmits: [], higherEmits: [],
            hostLower: undefined, hostHigher: undefined,
            applyParabolicArgs: undefined
        };
    }

    // The parabolic ability mock. sglUpdate* are real signals (matching the
    // definition's signatures) so Component.onCompleted's .connect() succeeds and
    // sltUpdateItemScale's re-emissions actually fire the connected slot.
    QtObject {
        id: parabolic
        signal sglUpdateLowerItemScale(int delegateIndex, var newScales)
        signal sglUpdateHigherItemScale(int delegateIndex, var newScales)

        property QtObject factor: QtObject { property real zoom: 1.6 }
        property bool restoreZoomIsBlocked: false
        property var currentParabolicItem: null
        property bool directRenderingEnabled: false

        function applyParabolicEffect(itemIndex, itemMousePosition, itemLength) {
            root.log.applyParabolicArgs = { index: itemIndex, pos: itemMousePosition, len: itemLength };
            return [1.6];
        }
        function setCurrentParabolicItem(item) { root.log.setCurrentItem = item; }
        function setCurrentParabolicItemIndex(idx) { root.log.setCurrentIndex = idx; }
    }

    // communicator: indexer support off (so the local, non-bridge slot path is
    // exercised), plus the bridge.parabolic.client host hooks for the
    // bridge-supported branch.
    QtObject {
        id: communicator
        property bool indexerIsSupported: false
        property bool parabolicEffectIsSupported: false
        property QtObject requires: QtObject { property bool parabolicEffectLocked: false }
        property QtObject bridge: QtObject {
            property QtObject parabolic: QtObject {
                property QtObject client: QtObject {
                    function hostRequestUpdateLowerItemScale(scales) { root.log.hostLower = scales; }
                    function hostRequestUpdateHigherItemScale(scales) { root.log.hostHigher = scales; }
                }
            }
        }
    }

    // wrapper.zoomScale is the main observable: updateScale / calculateParabolicScales write it.
    QtObject {
        id: wrapper
        property real zoomScale: 1
    }

    // applet.plasmoid.status gates updateScale; .title is read by onParabolicEntered.
    QtObject {
        id: applet
        property QtObject plasmoid: QtObject {
            // PlasmaCore.Types.HiddenStatus is 7; anything else passes the guard.
            property int status: 1
            property string title: "mock"
        }
    }

    QtObject {
        id: parabolicAreaLoader
        property bool hasParabolicMessagesEnabled: true
        property bool isParabolicEnabled: true
        property bool isThinTooltipEnabled: true
    }

    QtObject {
        id: restoreAnimation
        property bool running: false
        function stop() { root.log.restoreStop++; }
    }

    // myView is the Connections target for onIsShownFullyChanged. The handler
    // binds to the auto-generated isShownFully property-change signal, so toggling
    // the property fires it (no explicit signal needed).
    QtObject {
        id: myViewObj
        property bool isShownFully: true
    }

    // appletItem aggregates most of the surface. appletItem.parabolic points at
    // the same `parabolic` object so the connect()/emit wiring is consistent, and
    // appletItem.communicator points at the same communicator.
    QtObject {
        id: appletItem
        property int index: 2
        property real width: 64
        property real height: 64
        property bool parabolicEffectIsSupported: true
        property bool originalAppletBehavior: false
        property bool firstAppletInContainer: false
        property bool lastAppletInContainer: false
        property bool isSeparator: false
        property bool isMarginsAreaSeparator: false
        property bool isHidden: false
        property var tooltipVisualParent: null

        property QtObject parabolic: parabolic
        property QtObject communicator: communicator
        property QtObject myView: myViewObj

        property QtObject indexer: QtObject {
            function visibleIndex(i) { return i; }
        }
        property QtObject thinTooltip: QtObject {
            function show(parent, title) { root.log.tooltipShow++; }
            function hide(parent) { root.log.tooltipHide++; }
        }
        property QtObject layouts: QtObject { property int currentSpot: 0 }
        property QtObject animations: QtObject { property int hoverPixelSensitivity: 1 }
    }

    // ----- factory ------------------------------------------------------------
    // Each made object connects its slots to the shared `parabolic` signals in
    // Component.onCompleted. Track them and destroy per-test so connections from a
    // previous test can't fire into the shared wrapper/log during the next one.
    property var made: []

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = c.createObject(root, {});
        verify(obj, "instantiate failed");
        made.push(obj);
        return obj;
    }

    function cleanup() {
        for (var i = 0; i < made.length; i++) {
            if (made[i]) {
                made[i].destroy();
            }
        }
        made = [];
        // let the deferred destructions (and their onDestruction disconnects) run
        wait(20);
    }

    // ----- construction + Component.onCompleted -------------------------------
    function test_a_construct_and_oncompleted_connects() {
        resetLog();
        const p = make();
        // Real readable defaults survive a clean construction.
        compare(p.lastMousePoint.x, 0);
        compare(p.lastMousePoint.y, 0);
        // length binds to appletItem.width when horizontal (clean binding, no throw).
        compare(p.length, appletItem.width);
        // Component.onCompleted ran parabolic.sglUpdateLowerItemScale.connect(slt...).
        // Prove the connection by emitting the signal with delegateIndex matching
        // appletItem.index and a scale stack: the slot applies the first scale to
        // wrapper.zoomScale.
        wrapper.zoomScale = 1;
        parabolic.sglUpdateLowerItemScale(appletItem.index, [1.4, 0.5, 1]);
        compare(wrapper.zoomScale, 1.4);
    }

    // ----- calculateParabolicScales ------------------------------------------
    function test_b_calculateParabolicScales_applies_zoom() {
        resetLog();
        const p = make();
        wrapper.zoomScale = 1;
        parabolic.factor.zoom = 1.6;
        parabolic.restoreZoomIsBlocked = false;
        p.calculateParabolicScales(40);
        // applyParabolicEffect got the unqualified index + length, and zoomScale
        // was set to parabolic.factor.zoom.
        verify(root.log.applyParabolicArgs !== undefined);
        compare(root.log.applyParabolicArgs.index, root.index);
        compare(root.log.applyParabolicArgs.len, p.length);
        compare(wrapper.zoomScale, parabolic.factor.zoom);
    }

    function test_c_calculateParabolicScales_blocked_early_return() {
        resetLog();
        const p = make();
        wrapper.zoomScale = 1;
        parabolic.restoreZoomIsBlocked = true;
        p.calculateParabolicScales(40);
        // Early return before applyParabolicEffect / zoomScale write.
        compare(root.log.applyParabolicArgs, undefined);
        compare(wrapper.zoomScale, 1);
        parabolic.restoreZoomIsBlocked = false;
    }

    // ----- updateScale --------------------------------------------------------
    function test_d_updateScale_applies_on_index_match() {
        resetLog();
        const p = make();
        wrapper.zoomScale = 1;
        // nIndex matches appletItem.index, guards pass -> zoomScale = max(1, 1.5).
        p.updateScale(appletItem.index, 1.5);
        compare(wrapper.zoomScale, 1.5);
    }

    function test_e_updateScale_floor_at_one() {
        resetLog();
        const p = make();
        wrapper.zoomScale = 1.5;
        p.updateScale(appletItem.index, 0.4);
        // Math.max(1, 0.4) clamps up to 1.
        compare(wrapper.zoomScale, 1);
    }

    function test_f_updateScale_ignores_index_mismatch() {
        resetLog();
        const p = make();
        wrapper.zoomScale = 1.3;
        // nIndex != appletItem.index -> the outer if is false, no write.
        p.updateScale(appletItem.index + 5, 2.0);
        compare(wrapper.zoomScale, 1.3);
    }

    // ----- sltUpdateItemScale (local, non-bridge path) ------------------------
    function test_g_sltUpdateItemScale_lower_applies_and_propagates() {
        resetLog();
        const p = make();
        communicator.parabolicEffectIsSupported = false;
        wrapper.zoomScale = 1;
        var lower = [];
        parabolic.sglUpdateLowerItemScale.connect(function(idx, s) { lower.push({ idx: idx, s: s }); });
        // delegateIndex === appletItem.index, islower -> apply first scale (1.7),
        // then re-emit the remaining stack to the lower neighbour (index-1).
        p.sltUpdateItemScale(appletItem.index, [1.7, 0.6, 1], true);
        compare(wrapper.zoomScale, 1.7);
        // a propagation emit to the lower side carried the remaining stack.
        verify(lower.length >= 1);
        compare(lower[lower.length - 1].idx, appletItem.index - 1);
        compare(lower[lower.length - 1].s.length, 2);
    }

    function test_h_sltUpdateItemScale_higher_applies_and_propagates() {
        resetLog();
        const p = make();
        communicator.parabolicEffectIsSupported = false;
        wrapper.zoomScale = 1;
        var higher = [];
        parabolic.sglUpdateHigherItemScale.connect(function(idx, s) { higher.push({ idx: idx, s: s }); });
        p.sltUpdateItemScale(appletItem.index, [1.8, 0.6, 1], false);
        compare(wrapper.zoomScale, 1.8);
        verify(higher.length >= 1);
        compare(higher[higher.length - 1].idx, appletItem.index + 1);
    }

    function test_i_sltUpdateItemScale_bridge_delegates_to_host() {
        resetLog();
        const p = make();
        // communicator.parabolicEffectIsSupported true -> delegate to the bridge
        // host client instead of applying locally.
        communicator.parabolicEffectIsSupported = true;
        wrapper.zoomScale = 1;
        p.sltUpdateItemScale(appletItem.index, [1.9, 1], true);
        verify(root.log.hostLower !== undefined);
        compare(wrapper.zoomScale, 1); // not applied locally
        // higher direction through the bridge as well
        p.sltUpdateItemScale(appletItem.index, [1.9, 1], false);
        verify(root.log.hostHigher !== undefined);
        communicator.parabolicEffectIsSupported = false;
    }

    // ----- sltUpdateLowerItemScale / sltUpdateHigherItemScale delegators ------
    function test_j_sltUpdateLowerItemScale_delegates() {
        resetLog();
        const p = make();
        communicator.parabolicEffectIsSupported = false;
        wrapper.zoomScale = 1;
        // delegates to sltUpdateItemScale(..., true): applies first scale.
        p.sltUpdateLowerItemScale(appletItem.index, [1.5, 1]);
        compare(wrapper.zoomScale, 1.5);
    }

    function test_k_sltUpdateHigherItemScale_delegates() {
        resetLog();
        const p = make();
        communicator.parabolicEffectIsSupported = false;
        wrapper.zoomScale = 1;
        p.sltUpdateHigherItemScale(appletItem.index, [1.45, 1]);
        compare(wrapper.zoomScale, 1.45);
    }

    // ----- onParabolicExited handler ------------------------------------------
    function test_l_onParabolicExited_hides_tooltip() {
        resetLog();
        const p = make();
        parabolicAreaLoader.isThinTooltipEnabled = true;
        p.parabolicExited();
        compare(root.log.tooltipHide, 1);
    }

    function test_m_onParabolicExited_no_tooltip_when_disabled() {
        resetLog();
        const p = make();
        parabolicAreaLoader.isThinTooltipEnabled = false;
        p.parabolicExited();
        compare(root.log.tooltipHide, 0);
        parabolicAreaLoader.isThinTooltipEnabled = true;
    }

    // ----- MouseArea onEntered handler ----------------------------------------
    function findMouseArea(p) {
        // The lone MouseArea (parabolicMouseArea) is a visual child of the Item.
        const kids = p.children;
        for (var j = 0; j < kids.length; j++) {
            const k = kids[j];
            if (k && k.hasOwnProperty("containsMouse") && k.hasOwnProperty("hoverEnabled")) {
                return k;
            }
        }
        const res = p.resources;
        for (var i = 0; i < res.length; i++) {
            const r = res[i];
            if (r && r.hasOwnProperty("containsMouse") && r.hasOwnProperty("hoverEnabled")) {
                return r;
            }
        }
        return null;
    }

    function test_n_mousearea_onEntered_sets_current_item() {
        resetLog();
        const p = make();
        const ma = findMouseArea(p);
        verify(ma, "could not find parabolicMouseArea");
        // Emitting entered() runs onEntered: setCurrentParabolicItem(_parabolicArea),
        // then (isParabolicEnabled) setCurrentParabolicItemIndex(visibleIndex(index)).
        ma.entered();
        compare(root.log.setCurrentItem, p);
        // indexer.visibleIndex returns its arg; index resolves to root.index.
        compare(root.log.setCurrentIndex, root.index);
    }

    // ----- Connections onIsShownFullyChanged ----------------------------------
    function test_o_onIsShownFullyChanged_replays_move_when_contained() {
        resetLog();
        const p = make();
        // containsMouse is true when parabolic.currentParabolicItem === _parabolicArea.
        parabolic.currentParabolicItem = p;
        myViewObj.isShownFully = false;
        // record lastMousePoint so the handler's parabolicMove replay is observable.
        p.parabolicMove(33, 0);
        compare(p.lastMousePoint.x, 33);
        // The handler fires parabolicMove(lastMousePoint.x, y); with isHorizontal and
        // a step >= hoverPixelSensitivity this recomputes via calculateParabolicScales.
        wrapper.zoomScale = 1;
        appletItem.layouts.currentSpot = 0;
        // toggling isShownFully back to true fires onIsShownFullyChanged; the guard
        // (isShownFully && containsMouse) is satisfied -> replays parabolicMove.
        myViewObj.isShownFully = true;
        // parabolicMove -> calculateParabolicScales applied the zoom from the replay.
        compare(wrapper.zoomScale, parabolic.factor.zoom);
        parabolic.currentParabolicItem = null;
    }

    // ----- Component.onDestruction --------------------------------------------
    function test_p_destruction_disconnects_cleanly() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = c.createObject(root, {});
        verify(obj, "instantiate failed");
        // Component.onDestruction disconnects the two parabolic signals. After
        // destruction, emitting the signal must not reach the (gone) slot.
        wrapper.zoomScale = 1;
        obj.destroy();
        wait(50);
        parabolic.sglUpdateLowerItemScale(appletItem.index, [1.99, 1]);
        // Slot disconnected at destruction -> no write to the shared wrapper.
        compare(wrapper.zoomScale, 1);
    }
}
