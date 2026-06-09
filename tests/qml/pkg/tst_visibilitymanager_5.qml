// Coverage for the containment's VisibilityManager. The component is an
// anonymous Item that reads a pile of unqualified context names (root,
// latteView, animations, metrics, parabolic, background, debug, autosize,
// Plasmoid, layoutsContainer, layoutsManager, themeExtended ...). QML resolves
// those names against the creation context, i.e. this test file's root object.
// So we name the TestCase `root` and declare every name the component touches
// as a property / mock object here, then drive its functions and a few signal
// paths. Every test asserts an observable effect: a returned/bound value, a
// property the function mutates, or a mock side-effect a signal handler triggers.
//
// Offscreen reality (probed, not assumed): LatteCore.WindowSystem.compositingActive
// is TRUE here, so animationSpeed = speedFactor.current * 1.62 * duration.large
// = 324ms (animations run, they don't snap), and updateInputGeometry takes its
// compositing branch. `Plasmoid.location` is undefined (no live containment), so
// the per-edge if/else chains in updateMaskArea/updateInputGeometry fall through;
// the function bodies still run and produce the no-edge geometry, which is what
// we assert. The edge-specific geometry needs a live containment -> live-only.
import QtQuick
import QtTest
import org.kde.latte.core 0.2 as LatteCore
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.plasmoid 2.0

TestCase {
    id: root
    name: "VisibilityManager5"
    when: windowShown
    visible: true
    width: 500
    height: 60

    // ---- bare context names the component reads unqualified ----
    property bool isVertical: false
    property bool isHorizontal: true
    property bool behaveAsPlasmaPanel: false
    property bool behaveAsDockWithMask: true
    property bool editMode: false
    property bool viewIsAvailable: true
    property bool inStartup: false
    property bool screenEdgeMarginEnabled: true
    property bool floatingInternalGapIsForced: false
    property bool hideThickScreenGap: false
    property bool hasFloatingGapInputEventsDisabled: false
    // root also exposes a `myView` (Connections target@113, isSinkedEventEnabled@41)
    property QtObject myView: myViewObj

    QtObject {
        id: myViewObj
        property bool isShownFully: true
        property bool inRelocationAnimation: false
    }

    QtObject {
        id: animations
        property bool active: true
        property QtObject speedFactor: QtObject { property real current: 1.0 }
        property QtObject duration: QtObject { property int large: 200 }
        property QtObject needBothAxis: QtObject { property int count: 0 }
        property QtObject needLength: QtObject { property int count: 0 }
        property QtObject needThickness: QtObject { property int count: 0 }
    }

    QtObject {
        id: parabolic
        property bool isEnabled: false
        function sglClearZoom() { root._clearZoomCalls++; }
    }
    property int _clearZoomCalls: 0

    QtObject {
        id: metrics
        property QtObject mask: QtObject {
            property real screenEdge: 4
            property QtObject thickness: QtObject {
                property real normal: 40
                property real hidden: 1
                property real zoomedForItems: 80
                property real maxZoomed: 80
            }
        }
        property QtObject totals: QtObject { property real thickness: 44 }
        property QtObject margins: QtObject { property real screenEdge: 4 }
    }

    QtObject {
        id: autosize
        property bool inCalculatedIconSize: true
        function updateIconSize() { root._iconSizeCalls++; }
    }
    property int _iconSizeCalls: 0

    QtObject {
        id: debug
        property bool maskEnabled: false
        property bool graphicsEnabled: false
    }

    QtObject {
        id: background
        property QtObject totals: QtObject {
            property real visualLength: 100
            property real visualThickness: 40
        }
        property QtObject shadows: QtObject { property real headThickness: 6 }
    }

    QtObject { id: themeExtended }
    // layoutsManager carries currentLayoutIsSwitching so the Connections{
    // function onCurrentLayoutIsSwitching } handler can fire.
    QtObject {
        id: layoutsManager
        signal currentLayoutIsSwitching(string layoutName)
    }

    Item { id: layoutsContainer; width: 100; height: 40 }

    // `Plasmoid` comes from the plasmoid import. In a bare TestCase it is an
    // attached context object with no live containment: Plasmoid.location reads
    // undefined (edge branches fall through) and Plasmoid.configuration is
    // undefined (the slidingOutToPos/slidingOutRealFloating panel bindings that
    // read configuration.screenEdgeMargin throw a caught binding TypeError and
    // keep their non-panel value). We can't shadow Plasmoid with our own
    // id/property (ids/property names can't start uppercase), so panel-edge
    // geometry stays live-only; we assert the non-edge paths that do run.

    // latteView mock. Functions count calls so we can verify the show/hide and
    // slide paths actually ran.
    property int _hideCalls: 0
    property int _showCalls: 0
    property int _slideOutFinished: 0
    property int _slideInFinished: 0
    property int _frontLayer: 0
    property int _backLayer: 0
    property int _forceRedraw: 0
    property int _relocationFinished: 0

    QtObject {
        id: visibilityObj
        property bool isHidden: false
        property bool containsMouse: false
        property int  mode: LatteCore.Types.DodgeActive
        property bool isSidebar: false
        property bool blockHiding: false
        function setViewOnFrontLayer() { root._frontLayer++; }
        function setViewOnBackLayer() { root._backLayer++; }
        function hide() { root._hideCalls++; }
        function show() { root._showCalls++; }
        function slideOutFinished() { root._slideOutFinished++; }
        function slideInFinished() { root._slideInFinished++; }
    }

    QtObject {
        id: positionerObj
        property bool inRelocationAnimation: false
        property bool inSlideAnimation: false
        property bool inRelocationShowing: false
        property real slideOffset: 0
        function hidingForRelocationFinished() { root._relocationFinished++; }
    }

    QtObject {
        id: effectsObj
        property rect rect: Qt.rect(0, 0, 500, 44)
        property rect mask: Qt.rect(0, 0, 500, 44)
        property var inputMask: Qt.rect(0, 0, -1, -1)
        function forceMaskRedraw() { root._forceRedraw++; }
    }

    QtObject {
        id: latteView
        property real x: 0
        property real y: 0
        property real width: 500
        property real height: 44
        property bool behaveAsPlasmaPanel: false
        property var localGeometry: Qt.rect(0, 0, 500, 44)
        property QtObject visibility: visibilityObj
        property QtObject positioner: positionerObj
        property QtObject effects: effectsObj
        property QtObject layout: QtObject { property string name: "MyLayout" }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/VisibilityManager.qml")

    // Reset the mock state every test reads/writes, so deltas and absolute
    // assertions don't leak between tests (counters live on `root`).
    function resetState() {
        latteView.behaveAsPlasmaPanel = false;
        root.behaveAsPlasmaPanel = false;
        root.inStartup = false;
        root.hideThickScreenGap = false;
        root.hasFloatingGapInputEventsDisabled = false;
        root.floatingInternalGapIsForced = false;
        root.screenEdgeMarginEnabled = true;
        visibilityObj.isHidden = false;
        visibilityObj.containsMouse = false;
        visibilityObj.isSidebar = false;
        visibilityObj.blockHiding = false;
        visibilityObj.mode = LatteCore.Types.DodgeActive;
        positionerObj.inSlideAnimation = false;
        positionerObj.slideOffset = 0;
        animations.needBothAxis.count = 0;
        animations.needLength.count = 0;
        animations.needThickness.count = 0;
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // The declarative properties evaluate to concrete values in this context;
    // assert the exact values so a regression in any binding fails the test.
    function test_boundProperties() {
        resetState();
        const m = make();
        // compositingActive=true, not editMode -> 1.0 * 1.62 * 200
        compare(m.animationSpeed, 324);
        // not panel && screenEdgeMargin && !forced && !sliding
        compare(m.isFloatingInClientSide, true);
        // behaveAsDockWithMask && hideThickScreenGap(false)
        compare(m.inClientSideScreenEdgeSliding, false);
        // counts all zero -> normal
        compare(m.inNormalState, true);
        // positioner.inRelocationAnimation is false
        compare(m.inRelocationAnimation, false);
        // parabolic disabled + myView shown fully
        compare(m.isSinkedEventEnabled, true);
        // horizontal -> Screen.width; assert it is a positive length
        verify(m.length > 0);
        // non-panel, Plasmoid.location undefined -> +metrics.mask.thickness.normal
        compare(m.slidingOutToPos, 40);
        // metrics.totals.thickness
        compare(m.thicknessAsPanel, 44);
        // autosize.inCalculatedIconSize && nothing sliding/hiding
        compare(m.updateIsEnabled, true);

        // inClientSideScreenEdgeSliding tracks hideThickScreenGap.
        root.hideThickScreenGap = true;
        compare(m.inClientSideScreenEdgeSliding, true);
        root.hideThickScreenGap = false;
        compare(m.inClientSideScreenEdgeSliding, false);
    }

    // updateMaskArea, normal state: writes latteView.localGeometry. With no live
    // edge it produces the no-edge rect (0,0,root.width,root.height) clamped to
    // the view's own size, i.e. (0,0,500,44).
    function test_updateMaskArea_setsLocalGeometry() {
        resetState();
        const m = make();
        latteView.localGeometry = Qt.rect(9, 9, 9, 9);
        m.updateMaskArea();
        compare(latteView.localGeometry.x, 0);
        compare(latteView.localGeometry.y, 0);
        compare(latteView.localGeometry.width, 500);
        compare(latteView.localGeometry.height, 44);
    }

    // updateMaskArea while hidden: the localGeometry block is skipped (updating
    // it while hidden breaks Dodge modes), so the value must stay untouched.
    // updateInputGeometry still runs (asserted via inputMask below).
    function test_updateMaskArea_hiddenSkipsLocalGeometry() {
        resetState();
        const m = make();
        latteView.localGeometry = Qt.rect(7, 7, 7, 7);
        effectsObj.inputMask = Qt.rect(9, 9, 9, 9);
        visibilityObj.isHidden = true;
        m.updateMaskArea();
        // localGeometry left as set
        compare(latteView.localGeometry.x, 7);
        compare(latteView.localGeometry.width, 7);
        // but the input mask was still recomputed (no longer the sentinel)
        verify(effectsObj.inputMask.width !== 9 || effectsObj.inputMask.x !== 9);
    }

    // updateInputGeometry, compositing + non-panel: writes a computed inputMask.
    function test_updateInputGeometry_setsInputMask() {
        resetState();
        const m = make();
        effectsObj.inputMask = Qt.rect(9, 9, 9, 9);
        m.updateInputGeometry();
        // no-edge path: (0,0,root.width,root.height) clamped to view size
        compare(effectsObj.inputMask.x, 0);
        compare(effectsObj.inputMask.y, 0);
        compare(effectsObj.inputMask.width, 500);
        compare(effectsObj.inputMask.height, 44);
    }

    // updateInputGeometry, panel / no-compositing branch: clears the mask.
    function test_updateInputGeometry_panelClearsMask() {
        resetState();
        const m = make();
        effectsObj.inputMask = Qt.rect(7, 7, 7, 7);
        latteView.behaveAsPlasmaPanel = true;
        m.updateInputGeometry();
        compare(effectsObj.inputMask.x, 0);
        compare(effectsObj.inputMask.y, 0);
        compare(effectsObj.inputMask.width, -1);
        compare(effectsObj.inputMask.height, -1);
    }

    // updateInputGeometry, sidebar + hidden: forces a 1px off-screen rect so no
    // input is accepted anywhere.
    function test_updateInputGeometry_sidebarHidden() {
        resetState();
        const m = make();
        effectsObj.inputMask = Qt.rect(7, 7, 7, 7);
        visibilityObj.isSidebar = true;
        visibilityObj.isHidden = true;
        m.updateInputGeometry();
        compare(effectsObj.inputMask.x, -1);
        compare(effectsObj.inputMask.y, -1);
        compare(effectsObj.inputMask.width, 1);
        compare(effectsObj.inputMask.height, 1);
    }

    // slotContainsMouseChanged: mouse contained (non-sidebar mode) -> updateMaskArea;
    // mouse not contained -> no-op. Observe via localGeometry being (re)written or not.
    function test_slotContainsMouseChanged() {
        resetState();
        const m = make();
        // not contained -> no-op, localGeometry untouched
        latteView.localGeometry = Qt.rect(7, 7, 7, 7);
        visibilityObj.containsMouse = false;
        m.slotContainsMouseChanged();
        compare(latteView.localGeometry.x, 7);
        compare(latteView.localGeometry.width, 7);
        // contained, normal mode -> updateMaskArea rewrites localGeometry
        latteView.localGeometry = Qt.rect(7, 7, 7, 7);
        visibilityObj.containsMouse = true;
        visibilityObj.mode = LatteCore.Types.DodgeActive;
        m.slotContainsMouseChanged();
        compare(latteView.localGeometry.x, 0);
        compare(latteView.localGeometry.width, 500);
    }

    // slotMustBeShown, WindowsCanCover mode raises the view to the front layer.
    function test_slotMustBeShown_windowsCanCover() {
        resetState();
        const m = make();
        const before = root._frontLayer;
        visibilityObj.mode = LatteCore.Types.WindowsCanCover;
        m.slotMustBeShown();
        compare(root._frontLayer, before + 1);
    }

    // slotMustBeHide, WindowsCanCover mode pushes the view to the back layer.
    function test_slotMustBeHide_windowsCanCover() {
        resetState();
        const m = make();
        const before = root._backLayer;
        visibilityObj.mode = LatteCore.Types.WindowsCanCover;
        m.slotMustBeHide();
        compare(root._backLayer, before + 1);
    }

    // The relocation helpers: hide sets inRelocationHiding and kicks the slide-out
    // init; send* forward to the positioner/visibility mocks.
    function test_relocation_helpers() {
        resetState();
        const m = make();
        compare(m.inRelocationHiding, false);
        m.slotHideDockDuringLocationChange();
        compare(m.inRelocationHiding, true);

        const relBefore = root._relocationFinished;
        m.sendHideDockDuringLocationChangeFinished();
        compare(root._relocationFinished, relBefore + 1);

        const hideBefore = root._hideCalls;
        m.sendSlidingOutAnimationEnded();
        compare(root._hideCalls, hideBefore + 1);
        compare(visibilityObj.isHidden, true);
        // sendSlidingOutAnimationEnded also forwards to the relocation-finished path
        compare(root._relocationFinished, relBefore + 2);
    }

    // The layout-switch Connections handler fires sglClearZoom only when the
    // switching layout name matches the view's layout AND compositing is active
    // (it is, here). A non-matching name must not call it. We drive the real
    // SIGNAL, so the mock side-effect proves the handler body ran.
    function test_layout_switch_signal() {
        resetState();
        const m = make();
        var before = root._clearZoomCalls;
        layoutsManager.currentLayoutIsSwitching("MyLayout");
        compare(root._clearZoomCalls, before + 1);
        before = root._clearZoomCalls;
        layoutsManager.currentLayoutIsSwitching("OtherLayout");
        compare(root._clearZoomCalls, before);
    }

    // myView relocation Connections handler: clearing inRelocationAnimation runs
    // updateMaskArea, which rewrites localGeometry.
    function test_myView_relocation_connection() {
        resetState();
        const m = make();
        myViewObj.inRelocationAnimation = true;
        latteView.localGeometry = Qt.rect(7, 7, 7, 7);
        myViewObj.inRelocationAnimation = false; // false -> handler calls updateMaskArea
        compare(latteView.localGeometry.x, 0);
        compare(latteView.localGeometry.width, 500);
    }

    // Auto-hide slide-out then slide-in. animationSpeed is 324ms (compositing
    // active), so the PropertyAnimation runs and onStopped fires within a couple
    // hundred ms. Counters live on `root`; assert on deltas.
    function test_slide_out_then_in() {
        resetState();
        const m = make();
        const outBase = root._slideOutFinished;
        const inBase = root._slideInFinished;
        const iconBase = root._iconSizeCalls;
        const showBase = root._showCalls;

        // Hide: slidingAnimationAutoHiddenOut.init -> start -> onStopped.
        m.slotMustBeHide();
        tryVerify(function() { return root._slideOutFinished > outBase; }, 3000,
                  "slide-out never finished");
        // The slide-out ScriptAction sets isHidden=true before onStopped.
        compare(visibilityObj.isHidden, true);
        // Show: slidingAnimationAutoHiddenIn.init -> onStarted (show) -> onStopped.
        m.slotShowDockAfterLocationChange();
        tryVerify(function() { return root._showCalls > showBase; }, 3000,
                  "slide-in never started (visibility.show not called)");
        tryVerify(function() { return root._slideInFinished > inBase; }, 3000,
                  "slide-in never finished");
        // onStopped@533 calls autosize.updateIconSize.
        verify(root._iconSizeCalls > iconBase);
    }

    // Real-floating slide is driven by root.onHideThickScreenGapChanged. Needs
    // behaveAsPlasmaPanel + not hidden + not sliding + not startup. Toggling on
    // starts slidingOutRealFloating (first ScriptAction sets inSlideAnimation=true);
    // toggling off runs slidingInRealFloating to slideOffset 0 (onStopped clears it).
    function test_real_floating_slide() {
        resetState();
        const m = make();
        root.behaveAsPlasmaPanel = true;
        compare(positionerObj.inSlideAnimation, false);
        root.hideThickScreenGap = true;
        tryVerify(function() { return positionerObj.inSlideAnimation === true; }, 2000,
                  "real-floating slide-out did not start");
        root.hideThickScreenGap = false;
        tryVerify(function() { return positionerObj.inSlideAnimation === false; }, 2000,
                  "real-floating slide-in did not finish");
    }
}
