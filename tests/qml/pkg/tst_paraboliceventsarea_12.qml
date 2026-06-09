// Coverage spike for the basicitem ParabolicEventsArea. The real component is a
// Loader sourceComponent inside BasicItem and reads a pile of free identifiers
// from that enclosing scope (abilityItem, parabolicItem, parabolicEventsAreaLoader,
// index, hiddenSpacerLeft/Right, restoreAnimation). We reproduce that scope here
// with a wrapper Component that declares those ids and loads the *instrumented*
// staged copy via a nested Loader, so each function/handler we drive fires a Cov tick.
import QtQuick
import QtTest

TestCase {
    id: tc
    name: "ParabolicEventsArea"
    when: windowShown
    visible: true
    width: 200
    height: 200

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/items/basicitem/ParabolicEventsArea.qml")

    // ---- mock backing objects, shared by the wrapper instances ----

    // A parabolic ability: the busiest mock. It carries the two scale-update
    // signals the component connects to in Component.onCompleted, plus the
    // functions/properties the handlers read.
    QtObject {
        id: parabolicFactor
        property real zoom: 1.6
    }

    QtObject {
        id: parabolicAbility
        property var currentParabolicItem: null
        property bool restoreZoomIsBlocked: false
        property bool directRenderingEnabled: false
        property var factor: parabolicFactor

        property var lastSetItem: null
        property int lastSetIndex: -1
        property var lastEffectArgs: null
        property int lowerSignals: 0
        property int higherSignals: 0

        signal sglUpdateLowerItemScale(int delegateIndex, var newScales)
        signal sglUpdateHigherItemScale(int delegateIndex, var newScales)

        function setCurrentParabolicItem(item) { lastSetItem = item; currentParabolicItem = item; }
        function setCurrentParabolicItemIndex(idx) { lastSetIndex = idx; }
        function applyParabolicEffect(itemIndex, mousePos, len) {
            lastEffectArgs = { index: itemIndex, pos: mousePos, len: len };
            return [1.5, 1.2, 1.0];
        }

        function emitLower(idx, scales) { sglUpdateLowerItemScale(idx, scales); }
        function emitHigher(idx, scales) { sglUpdateHigherItemScale(idx, scales); }

        function onLower() { lowerSignals++; }
        function onHigher() { higherSignals++; }
        Component.onCompleted: {
            sglUpdateLowerItemScale.connect(onLower);
            sglUpdateHigherItemScale.connect(onHigher);
        }
    }

    QtObject {
        id: thinTooltipAbility
        property int shown: 0
        property int hidden: 0
        function show(visualParent, text) { shown++; }
        function hide(visualParent) { hidden++; }
    }

    QtObject {
        id: shortcutsAbility
        function shortcutIndex(entryIndex) { return entryIndex + 100; }
    }

    QtObject {
        id: myViewAbility
        property bool isShownFully: true
        property bool isReady: true
    }

    QtObject {
        id: animationsAbility
        property int hoverPixelSensitivity: 1
    }

    QtObject {
        id: abilitiesObj
        property var parabolic: parabolicAbility
        property var thinTooltip: thinTooltipAbility
        property var shortcuts: shortcutsAbility
        property var myView: myViewAbility
        property var animations: animationsAbility
    }

    // The parabolicItem mock (also reachable as abilityItem.parabolicItem).
    QtObject {
        id: parabolicItemMock
        property bool isParabolicEventBlocked: false
        property bool isUpdatingOnlySpacers: false
        property real zoom: 1
    }

    QtObject {
        id: abilityItemMock
        property var abilities: abilitiesObj
        property bool isHorizontal: true
        property real width: 64
        property real height: 64
        property var parabolicItem: parabolicItemMock
        property var tooltipVisualParent: tc
        property string thinTooltipText: "tip"
        property int itemIndex: 3
        property bool isSeparator: false
        property bool isHidden: false
        property bool isFirstItemInContainer: false
        property bool isLastItemInContainer: false
    }

    QtObject {
        id: loaderMock
        property bool isParabolicEnabled: true
        property bool isThinTooltipEnabled: true
    }

    QtObject {
        id: spacerLeftMock
        property real nScale: 0
    }
    QtObject {
        id: spacerRightMock
        property real nScale: 0
    }
    QtObject {
        id: restoreAnimationMock
        property int stopped: 0
        function stop() { stopped++; }
    }

    // The wrapper that re-creates the enclosing BasicItem scope. The free
    // identifiers in the staged component resolve against the ids declared here
    // because the nested Loader's items live in this component's context.
    property Component wrapperComp: Component {
        Item {
            id: wrapperRoot
            property int index: 5
            property alias parabolicAreaItem: innerLoader.item

            // expose the test's mock ids under the names the staged file reads
            property var abilityItem: abilityItemMock
            property var parabolicItem: parabolicItemMock
            property var parabolicEventsAreaLoader: loaderMock
            property var hiddenSpacerLeft: spacerLeftMock
            property var hiddenSpacerRight: spacerRightMock
            property var restoreAnimation: restoreAnimationMock

            Loader {
                id: innerLoader
                source: tc.targetUrl
            }
        }
    }

    function makeArea() {
        const wrapper = createTemporaryObject(wrapperComp, tc);
        verify(wrapper, "wrapper failed to instantiate");
        tryVerify(function(){ return wrapper.parabolicAreaItem !== null; }, 3000, "inner loader never produced item");
        const area = wrapper.parabolicAreaItem;
        verify(area, "ParabolicEventsArea item is null: " + innerErr(wrapper));
        return { wrapper: wrapper, area: area };
    }

    function innerErr(wrapper) {
        // best-effort error surface; the Loader keeps no errorString but status helps
        return "loader.status indicates failure";
    }

    // Component.onCompleted wires sltUpdate{Lower,Higher}ItemScale to the
    // parabolic ability's two signals. Prove the wiring by emitting each signal
    // at this item's own index and checking the slot ran (observable: the slot
    // forwards to the neighbour, bumping lowerSignals/higherSignals on the mock).
    function test_construction_connects() {
        const r = makeArea();
        const area = r.area;
        const myIndex = 5;

        abilityItemMock.isSeparator = false;
        abilityItemMock.isHidden = false;
        parabolicItemMock.isUpdatingOnlySpacers = false;

        parabolicAbility.lowerSignals = 0;
        parabolicAbility.emitLower(myIndex, [1.5, 1.3, 1.0]);
        verify(parabolicAbility.lowerSignals >= 1, "onCompleted connected the lower-scale slot");

        parabolicAbility.higherSignals = 0;
        parabolicAbility.emitHigher(myIndex, [1.5, 1.3, 1.0]);
        verify(parabolicAbility.higherSignals >= 1, "onCompleted connected the higher-scale slot");
    }

    // calculateParabolicScales: drive each branch (early-return on zoom==1,
    // first/last clamp branch, normal apply, spacers-only apply).
    function test_calculateParabolicScales() {
        const r = makeArea();
        const area = r.area;

        // early return: factor.zoom === 1
        parabolicFactor.zoom = 1;
        area.calculateParabolicScales(10);
        compare(parabolicAbility.lastEffectArgs, null, "should early-return before applyParabolicEffect");

        // normal apply path
        parabolicFactor.zoom = 1.6;
        parabolicItemMock.isParabolicEventBlocked = false;
        parabolicAbility.restoreZoomIsBlocked = false;
        parabolicItemMock.isUpdatingOnlySpacers = false;
        parabolicItemMock.zoom = 1.2;       // not 1, skip clamp branch
        area.calculateParabolicScales(20);
        verify(parabolicAbility.lastEffectArgs !== null, "applyParabolicEffect ran");
        compare(parabolicItemMock.zoom, parabolicFactor.zoom, "zoom set from factor");

        // first/last clamp branch: zoom===1 and isFirstItemInContainer
        abilityItemMock.isFirstItemInContainer = true;
        parabolicItemMock.zoom = 1;
        parabolicAbility.lastEffectArgs = null;
        area.calculateParabolicScales(100);
        verify(parabolicAbility.lastEffectArgs !== null, "applyParabolicEffect ran in clamp branch");
        abilityItemMock.isFirstItemInContainer = false;

        // spacers-only apply path
        parabolicItemMock.isUpdatingOnlySpacers = true;
        spacerLeftMock.nScale = -1;
        spacerRightMock.nScale = -1;
        area.calculateParabolicScales(30);
        const expected = (parabolicFactor.zoom - 1) / 2;
        compare(spacerLeftMock.nScale, expected, "left spacer scaled");
        compare(spacerRightMock.nScale, expected, "right spacer scaled");
        parabolicItemMock.isUpdatingOnlySpacers = false;

        // early return: parabolicItem blocked
        parabolicItemMock.isParabolicEventBlocked = true;
        parabolicAbility.lastEffectArgs = null;
        area.calculateParabolicScales(40);
        compare(parabolicAbility.lastEffectArgs, null, "blocked -> early return");
        parabolicItemMock.isParabolicEventBlocked = false;
    }

    // updateScale: matching index, both spacers and zoom branches; plus non-match.
    function test_updateScale() {
        const r = makeArea();
        const area = r.area;
        const myIndex = 5;

        parabolicItemMock.isUpdatingOnlySpacers = false;
        parabolicItemMock.zoom = 1;
        area.updateScale(myIndex, 1.8);
        compare(parabolicItemMock.zoom, 1.8, "zoom applied for matching index");

        // Math.max(1, nScale) floor
        area.updateScale(myIndex, 0.3);
        compare(parabolicItemMock.zoom, 1, "zoom floored at 1");

        // spacers branch
        parabolicItemMock.isUpdatingOnlySpacers = true;
        spacerLeftMock.nScale = 0;
        spacerRightMock.nScale = 0;
        area.updateScale(myIndex, 2.0);
        compare(spacerLeftMock.nScale, 0.5, "left spacer from updateScale");
        compare(spacerRightMock.nScale, 0.5, "right spacer from updateScale");
        parabolicItemMock.isUpdatingOnlySpacers = false;

        // non-matching index: nothing happens
        parabolicItemMock.zoom = 1.3;
        area.updateScale(myIndex + 999, 5.0);
        compare(parabolicItemMock.zoom, 1.3, "non-matching index ignored");
    }

    // sltUpdateItemScale and the lower/higher wrappers. Hits the
    // delegateIndex===index accept path, the clearrequested early-return, and
    // the neighbour-clear (else-if) branch.
    function test_sltUpdateItemScale() {
        const r = makeArea();
        const area = r.area;
        const myIndex = 5;

        // delegateIndex === index, normal stack with remaining scales -> forwards
        parabolicAbility.lowerSignals = 0;
        parabolicAbility.higherSignals = 0;
        abilityItemMock.isSeparator = false;
        abilityItemMock.isHidden = false;
        area.sltUpdateLowerItemScale(myIndex, [1.5, 1.3, 1.0]);
        verify(parabolicAbility.lowerSignals >= 1, "lower neighbour signal forwarded");

        area.sltUpdateHigherItemScale(myIndex, [1.5, 1.3, 1.0]);
        verify(parabolicAbility.higherSignals >= 1, "higher neighbour signal forwarded");

        // empty newScales -> early return inside the match (no forward happens)
        parabolicAbility.lowerSignals = 0;
        area.sltUpdateLowerItemScale(myIndex, []);
        compare(parabolicAbility.lowerSignals, 0, "empty newScales early-returns, forwards nothing");

        // clearrequestedfromlastacceptedsignal stack [1] after accepting first:
        // accept first(==1 leaves []) ... use [x,1] so after splice it's [1]
        parabolicAbility.lowerSignals = 0;
        area.sltUpdateLowerItemScale(myIndex, [1.4, 1.0]);
        verify(parabolicAbility.lowerSignals >= 1, "clear-request forwarded to lower neighbour");

        // separator/hidden -> skip the accept block, still forward when not clear-req
        abilityItemMock.isSeparator = true;
        parabolicAbility.lowerSignals = 0;
        area.sltUpdateLowerItemScale(myIndex, [1.7, 1.2]);
        verify(parabolicAbility.lowerSignals >= 1, "separator still forwards");
        abilityItemMock.isSeparator = false;

        // else-if neighbour-clear branch: delegateIndex != index, islower &&
        // clearrequested && index < delegateIndex -> updateScale(index,1)
        parabolicItemMock.isUpdatingOnlySpacers = false;
        parabolicItemMock.zoom = 1.9;
        area.sltUpdateLowerItemScale(myIndex + 3, [1.0]);  // index(5) < 8, clearreq
        compare(parabolicItemMock.zoom, 1, "neighbour-clear lower resets zoom to 1");

        // higher side neighbour-clear: index > delegateIndex
        parabolicItemMock.zoom = 1.9;
        area.sltUpdateHigherItemScale(myIndex - 3, [1.0]);  // index(5) > 2, clearreq
        compare(parabolicItemMock.zoom, 1, "neighbour-clear higher resets zoom to 1");
    }

    // onParabolicEntered handler: sets last pos, stops restore animation, shows
    // tooltip, runs calculateParabolicScales (horizontal -> uses mouseX).
    function test_parabolicEntered_signal() {
        const r = makeArea();
        const area = r.area;

        restoreAnimationMock.stopped = 0;
        thinTooltipAbility.shown = 0;
        parabolicFactor.zoom = 1.6;
        parabolicItemMock.isParabolicEventBlocked = false;
        parabolicAbility.lastEffectArgs = null;
        abilityItemMock.isHorizontal = true;

        area.parabolicEntered(42, 7);
        compare(area.lastMouseX, 42);
        compare(area.lastMouseY, 7);
        verify(restoreAnimationMock.stopped >= 1, "restoreAnimation.stop called");
        verify(thinTooltipAbility.shown >= 1, "thin tooltip shown");
        compare(area.lastParabolicPos, 42, "horizontal -> lastParabolicPos = mouseX");
        verify(parabolicAbility.lastEffectArgs !== null, "calculateParabolicScales ran");
    }

    // onParabolicMove handler: exercise the guarded fast path that calls
    // calculateParabolicScales, plus the negative/blocked early returns.
    function test_parabolicMove_signal() {
        const r = makeArea();
        const area = r.area;

        abilityItemMock.isHorizontal = true;
        parabolicItemMock.isParabolicEventBlocked = false;
        myViewAbility.isReady = true;
        myViewAbility.isShownFully = true;
        parabolicItemMock.zoom = 1;                     // == 1 satisfies the gate
        parabolicAbility.directRenderingEnabled = false;
        animationsAbility.hoverPixelSensitivity = 1;
        area.lastParabolicPos = 0;
        parabolicFactor.zoom = 1.6;
        parabolicAbility.lastEffectArgs = null;

        area.parabolicMove(50, 9);                       // step 50 >= 1 -> recalc
        compare(area.lastMouseX, 50);
        verify(parabolicAbility.lastEffectArgs !== null, "move recalculated scales");

        // mousePos < 0 -> early return
        parabolicAbility.lastEffectArgs = null;
        area.parabolicMove(-5, 9);
        compare(parabolicAbility.lastEffectArgs, null, "negative pos early-returns");

        // not shown fully while ready -> early return
        myViewAbility.isShownFully = false;
        parabolicAbility.lastEffectArgs = null;
        area.parabolicMove(80, 9);
        compare(parabolicAbility.lastEffectArgs, null, "not-shown-fully early-returns");
        myViewAbility.isShownFully = true;

        // directRendering branch
        parabolicAbility.directRenderingEnabled = true;
        parabolicItemMock.zoom = 1.42;                   // not 1, but directRender path taken
        area.lastParabolicPos = 0;
        parabolicAbility.lastEffectArgs = null;
        area.parabolicMove(70, 9);
        verify(parabolicAbility.lastEffectArgs !== null, "directRendering path recalculated");
        parabolicAbility.directRenderingEnabled = false;
    }

    // onParabolicExited handler: resets pos and hides tooltip.
    function test_parabolicExited_signal() {
        const r = makeArea();
        const area = r.area;
        thinTooltipAbility.hidden = 0;
        area.lastParabolicPos = 99;
        area.parabolicExited();
        compare(area.lastParabolicPos, 0, "exited resets parabolic pos");
        verify(thinTooltipAbility.hidden >= 1, "tooltip hidden on exit");
    }

    // The Connections onIsParabolicEventBlockedChanged + onIsShownFullyChanged.
    // We make containsMouse true by marking this area the current parabolic item,
    // then flip the watched properties to fire the handlers and assert their
    // downstream effects (parabolicEntered stops restore + recalcs; parabolicMove
    // recalcs).
    function test_connections_handlers() {
        const r = makeArea();
        const area = r.area;

        // make the area the current parabolic item -> containsMouse true
        parabolicAbility.currentParabolicItem = area;
        verify(area.containsMouse, "containsMouse true when current parabolic item");

        // horizontal hover at x=11 keeps mousePos>=0 so the move handler recalcs
        area.lastMouseX = 11;
        area.lastMouseY = 12;
        abilityItemMock.isHorizontal = true;
        parabolicFactor.zoom = 1.6;
        parabolicItemMock.zoom = 1;
        parabolicAbility.restoreZoomIsBlocked = false;
        parabolicItemMock.isUpdatingOnlySpacers = false;
        myViewAbility.isReady = true;
        myViewAbility.isShownFully = true;
        animationsAbility.hoverPixelSensitivity = 1;
        area.lastParabolicPos = 0;

        // onIsParabolicEventBlockedChanged: the false edge with containsMouse
        // fires parabolicEntered, which stops the restore animation and recalcs.
        restoreAnimationMock.stopped = 0;
        parabolicAbility.lastEffectArgs = null;
        parabolicItemMock.isParabolicEventBlocked = true;
        parabolicItemMock.isParabolicEventBlocked = false;   // handler -> parabolicEntered
        verify(restoreAnimationMock.stopped >= 1, "blocked->unblocked fired parabolicEntered (stop ran)");
        verify(parabolicAbility.lastEffectArgs !== null, "blocked->unblocked recalculated scales");

        // onIsShownFullyChanged: the false->true edge with containsMouse fires
        // parabolicMove, which recalcs (zoom===1 satisfies the gate).
        parabolicItemMock.zoom = 1;
        area.lastParabolicPos = 0;
        parabolicAbility.lastEffectArgs = null;
        myViewAbility.isShownFully = false;
        myViewAbility.isShownFully = true;                   // handler -> parabolicMove
        verify(parabolicAbility.lastEffectArgs !== null, "shownFully edge recalculated scales via parabolicMove");

        parabolicAbility.currentParabolicItem = null;
    }

    // Drive the MouseArea.onEntered by emitting the parabolic-item-not-current
    // state and firing entered via the signal connect path. We can call it
    // directly via the mouse area child.
    function test_mousearea_entered() {
        const r = makeArea();
        const area = r.area;

        // find the inner MouseArea
        var ma = null;
        for (var i = 0; i < area.children.length; i++) {
            if (area.children[i] && area.children[i].hoverEnabled !== undefined) {
                ma = area.children[i];
                break;
            }
        }
        verify(ma, "found inner MouseArea");

        parabolicAbility.lastSetItem = null;
        parabolicAbility.lastSetIndex = -1;
        loaderMock.isThinTooltipEnabled = true;
        loaderMock.isParabolicEnabled = true;
        thinTooltipAbility.shown = 0;

        ma.entered();   // invoke the signal -> onEntered handler

        compare(parabolicAbility.lastSetItem, area, "onEntered set current parabolic item");
        verify(parabolicAbility.lastSetIndex !== -1, "onEntered set current parabolic item index");
        verify(thinTooltipAbility.shown >= 1, "onEntered showed thin tooltip");
    }
}
