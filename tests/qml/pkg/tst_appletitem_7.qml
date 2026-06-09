// Coverage for the containment AppletItem delegate, loaded from the staged
// (instrumented) package.
//
// AppletItem reads a pile of unqualified ambient names (root, latteView,
// layoutsContainer, the ability objects) from its creation context. QML
// resolves those lexically against the scope where the component is created,
// so this TestCase is named `id: root` and declares every name the units we
// assert actually read — shaped like the real objects. With the context
// supplied the function/handler bodies run to completion and produce
// observable effects we can assert, instead of ticking-then-throwing.
//
// Units that need a live dock (latteView.extendedInterface, a real `applet`,
// colorizerManager, fastLayoutManager, the viewSignalsConnector mouse path)
// are left to the live-only list; we don't bank their entry ticks here.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "AppletItem7"
    when: windowShown
    visible: true
    width: 400
    height: 400

    // ---- ambient context the AppletItem units read ---------------------------
    // root.* names. `root` is this TestCase, an Item, so mapToItem() is real.
    property bool isHorizontal: true
    property bool isVertical: false
    property bool inConfigureAppletsMode: false
    property bool behaveAsPlasmaPanel: false
    property bool behaveAsDockWithMask: false
    property bool dragActiveWindowEnabled: false
    property bool mouseWheelActions: false
    property bool inDraggingOverAppletOrOutOfContainment: false
    property int maxLengthPerCentage: 100
    property var latteView: null
    property var dragOverlay: null
    property QtObject dragInfo: QtObject {
        property bool entered: false
        property bool isPlasmoid: false
    }
    property QtObject background: QtObject { property real currentOpacity: 1.0 }
    // AppletItem.Component.onCompleted connects to these two root signals.
    signal updateIndexes()
    signal destroyInternalViewSplitters()

    // layoutsContainer is read by checkIndex() when a layout has children. It
    // must be a *property* of root, not a child id: a separately-loaded
    // component's creation context exposes the context object's properties, not
    // its sibling object ids.
    property QtObject layoutsContainer: QtObject {
        property QtObject startLayout: QtObject { property var children: []; property int beginIndex: 0 }
        property QtObject mainLayout: QtObject { property var children: []; property int beginIndex: 0 }
        property QtObject endLayout: QtObject { property var children: []; property int beginIndex: 0 }
    }

    // colorizerManager + fastLayoutManager are read by eager child bindings
    // (the colorizer/indicator) and a couple of lazy computed props. Shape them
    // so construction is quiet; we don't assert any unit that needs them live.
    property QtObject colorizerManager: QtObject {
        property var applyTheme: null
        property bool mustBeShown: false
    }
    property var fastLayoutManager: null

    // ---- ability-object mocks (passed in as properties) ----------------------
    QtObject {
        id: durations
        property int small: 100
        property int large: 200
        property int proposed: 150
    }
    QtObject {
        id: speed
        property real normal: 1.0
    }
    // The AppletItem ability slots are declared `property Item`, so each mock
    // passed into one must be an Item (a QtObject silently fails to assign and
    // the slot stays null). Nested sub-objects are plain `var`/QtObject.
    Item {
        id: animationsMock
        property QtObject duration: durations
        property QtObject speedFactor: speed
        property QtObject needBothAxis: QtObject {
            property bool removed: false
            function removeEvent(o) { removed = true; return true; }
        }
    }

    Item {
        id: metricsMock
        property int iconSize: 48
        property QtObject totals: QtObject {
            property int lengthEdges: 4
            property int thicknessEdges: 4
        }
        property QtObject fraction: QtObject { property int lengthAppletPadding: -1 }
        property QtObject padding: QtObject { property int length: 2; property int lengthApplet: 3 }
        property QtObject margin: QtObject {
            property int length: 2
            property int screenEdge: 0
            property int tailThickness: 1
            property int headThickness: 1
        }
        property QtObject mask: QtObject {
            property QtObject thickness: QtObject { property int zoomedForItems: 60 }
        }
    }

    Item {
        id: indexerMock
        property var hidden: []
        property var clients: []
        property var separators: []
        property var marginsAreaSeparators: []
        property int visibleIndexCalls: 0
        property int lastVisibleIndexArg: -99
        function visibleIndex(i) { visibleIndexCalls++; lastVisibleIndexArg = i; return i; }
        function getClientBridge(i) { return null; }
    }

    Item {
        id: parabolicMock
        property bool isEnabled: false
        property QtObject factor: QtObject { property real zoom: 1.0 }
        signal sglClearZoom()
    }

    Item { id: thinTooltipMock; property bool isEnabled: false }

    Item {
        id: debugMock
        property bool graphicsEnabled: false
        property bool layouterEnabled: false
    }

    // layouter.{start,main,end}Layout — checkIndex walks `count` items each.
    QtObject {
        id: startLayoutMock
        property int firstVisibleIndex: 0
        property int lastVisibleIndex: 0
        property int beginIndex: 0
        property int count: 0
    }
    QtObject {
        id: mainLayoutMock
        property int firstVisibleIndex: 0
        property int lastVisibleIndex: 0
        property int beginIndex: 0
        property int count: 0
    }
    QtObject {
        id: endLayoutMock
        property int firstVisibleIndex: 0
        property int lastVisibleIndex: 0
        property int beginIndex: 0
        property int count: 0
    }
    Item {
        id: layouterMock
        property QtObject startLayout: startLayoutMock
        property QtObject mainLayout: mainLayoutMock
        property QtObject endLayout: endLayoutMock
    }

    Item {
        id: myViewMock
        property int alignment: 0
        property bool isShownFully: true
        property QtObject itemShadow: QtObject { property color shadowSolidColor: "black" }
    }

    Item {
        id: shortcutsMock
        property bool unifiedGlobalShortcuts: false
        signal sglActivateEntryAtIndex(int entryIndex)
        signal sglNewInstanceForEntryAtIndex(int entryIndex)
    }

    function make(extra) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        var props = {
            animations: animationsMock,
            metrics: metricsMock,
            indexer: indexerMock,
            parabolic: parabolicMock,
            thinTooltip: thinTooltipMock,
            debug: debugMock,
            layouter: layouterMock,
            myView: myViewMock,
            shortcuts: shortcutsMock,
            index: -1,
            internalSplitterId: 0
        };
        if (extra) {
            for (var k in extra) props[k] = extra[k];
        }
        const obj = createTemporaryObject(c, root, props);
        verify(obj, "instantiate failed");
        return obj;
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/applet/AppletItem.qml")

    // Instantiation: Component.onCompleted runs checkIndex() (which now reaches
    // the real layout loops because layouter/layoutsContainer resolve) and wires
    // the root signal connections. With empty layouts index resolves to -1.
    function test_instantiate() {
        const a = make();
        verify(a !== null);
        compare(a.index, -1);
        // separator/margins computed props evaluate (isSeparator false, index<0).
        compare(a.tailAppletIsSeparator, false);
        compare(a.headAppletIsSeparator, false);
        compare(a.inMarginsArea, false);
    }

    // checkIndex() walks all three (empty) layout containers and lands index=-1.
    function test_checkIndex_empty() {
        const a = make({ index: 7 });
        // even with a stale index, an empty layout set resets it to -1.
        a.checkIndex();
        compare(a.index, -1);
    }

    // checkIndex() finds the applet in mainLayout and computes its index from
    // beginIndex + position. We seed mainLayout.children with the live object.
    function test_checkIndex_found() {
        const a = make();
        mainLayoutMock.count = 1;
        layoutsContainer.mainLayout.beginIndex = 3;
        layoutsContainer.mainLayout.children = [a];
        a.checkIndex();
        compare(a.index, 3); // beginIndex(3) + mainAppletIndex(0)
        // reset so other tests start clean
        mainLayoutMock.count = 0;
        layoutsContainer.mainLayout.children = [];
        layoutsContainer.mainLayout.beginIndex = 0;
    }

    // sltClearZoom: communicator.parabolicEffectIsSupported is false by default,
    // so the else branch calls restoreAnimation.start(). The restore animation
    // tweens wrapper.zoomScale back to 1, so it only "runs" when zoomScale is
    // off 1 — bump it first, then assert the animation is running.
    function test_sltClearZoom_restores() {
        const a = make();
        const w = findWrapper(a);
        verify(w, "could not find ItemWrapper");
        w.zoomScale = 1.6;
        verify(!a.restoreAnimation.running);
        a.sltClearZoom();
        verify(a.restoreAnimation.running, "restore animation should be started");
        a.restoreAnimation.stop();
    }

    // The parabolic.sglClearZoom signal is connected to sltClearZoom in
    // onCompleted; emitting it must also start the restore animation.
    function test_sglClearZoom_connected() {
        const a = make();
        const w = findWrapper(a);
        verify(w, "could not find ItemWrapper");
        w.zoomScale = 1.6;
        verify(!a.restoreAnimation.running);
        parabolicMock.sglClearZoom();
        verify(a.restoreAnimation.running, "sglClearZoom should drive sltClearZoom");
        a.restoreAnimation.stop();
    }

    // updateParabolicEffectIsSupported() starts parabolicEffectIsSupportedTimer.
    // The timer may already be running from construction-time isSystray/
    // isAutoFillApplet settling, so stop it first to make the (re)start
    // observable, then assert the public fn transitions it back to running.
    function test_updateParabolicEffectIsSupported_startsTimer() {
        const a = make();
        const t = findTimer(a, 100);
        verify(t, "could not find parabolicEffectIsSupportedTimer");
        t.stop();
        verify(!t.running, "timer should be stopped before the call");
        a.updateParabolicEffectIsSupported();
        verify(t.running, "updateParabolicEffectIsSupported should start the timer");
        t.stop();
    }

    // slotDestroyInternalViewSplitters: isInternalViewSplitter is false
    // (internalSplitterId 0) so it is a no-op and the object survives.
    function test_slotDestroy_noop() {
        const a = make();
        a.slotDestroyInternalViewSplitters();
        verify(a !== null);
        compare(a.index, -1); // still alive and consistent
    }

    // containsPos maps a global point through root.mapToItem. root is this
    // TestCase (a real Item) so the map is real. A point inside the applet's
    // own 0..width/0..height box returns true; one outside returns false.
    function test_containsPos() {
        const a = make();
        a.width = 50;
        a.height = 50;
        // place the applet at a known offset under root
        a.x = 0;
        a.y = 0;
        compare(a.containsPos(Qt.point(10, 10)), true);
        compare(a.containsPos(Qt.point(999, 999)), false);
    }

    // onIndexChanged records previousIndex only while index>-1.
    function test_indexChanged() {
        const a = make();
        a.index = 5;
        compare(a.previousIndex, 5);
        a.index = -1;
        compare(a.previousIndex, 5); // unchanged on the -1 transition
        a.index = 9;
        compare(a.previousIndex, 9);
    }

    // Component.onCompleted connected root.updateIndexes -> checkIndex. Emitting
    // the signal must recompute index from the (now seeded) main layout.
    function test_updateIndexes_signalRecomputes() {
        const a = make();
        mainLayoutMock.count = 1;
        layoutsContainer.mainLayout.beginIndex = 2;
        layoutsContainer.mainLayout.children = [a];
        root.updateIndexes(); // drives the connected checkIndex
        compare(a.index, 2);
        mainLayoutMock.count = 0;
        layoutsContainer.mainLayout.children = [];
        layoutsContainer.mainLayout.beginIndex = 0;
    }

    // onParentChanged -> checkIndex(). Reparenting fires it; with empty layouts
    // index stays -1, proving the handler ran the reset path.
    function test_parentChanged_recomputes() {
        const a = make({ index: 4 });
        // a starts parented to root; move it to a fresh Item to fire the handler.
        const holder = newItem();
        a.parent = holder;
        compare(a.index, -1);
        holder.destroy();
    }

    // The parabolicEffectIsSupported timer body: wrapper.zoomScale is 1,
    // communicator.indexerIsSupported false, applet null and not systray/
    // autofill -> the else branch sets parabolicEffectIsSupported = true. Flip
    // it false first so the timer firing is an observable change back to true.
    function test_parabolicSupportTimer_setsTrue() {
        const a = make();
        const t = findTimer(a, 100);
        verify(t, "could not find parabolicEffectIsSupportedTimer");
        a.parabolicEffectIsSupported = false;
        t.interval = 1;
        t.restart();
        tryCompare(a, "parabolicEffectIsSupported", true, 1000);
    }

    // scrollDelayer fires onTriggered which sets the connector's blockWheel
    // false. The connector lives as a child; we read its blockWheel via the
    // pressed/blockWheel pair we can find. Set blockWheel true, fire, confirm.
    function test_scrollDelayTimer_clearsBlockWheel() {
        const a = make();
        const conn = findConnector(a);
        verify(conn, "could not find viewSignalsConnector");
        const t = findTimer(a, 500);
        verify(t, "could not find scrollDelayer");
        conn.blockWheel = true;
        t.interval = 1;
        t.restart();
        tryCompare(conn, "blockWheel", false, 1000);
    }

    // The shortcut Connections guard on unifiedGlobalShortcuts: when false they
    // return before touching indexer.visibleIndex(); when true they call it.
    // Use the indexer mock's call counter as the observable side-effect, and an
    // entryIndex that does NOT match visibleIndex(index) so the body
    // short-circuits before the latteView toggle (which would need a live dock).
    function test_shortcut_activate_guarded() {
        const a = make();
        a.index = 4; // checkIndex reset it to -1 in onCompleted; set a real value
        indexerMock.visibleIndexCalls = 0;

        // disabled -> early return, indexer.visibleIndex() never called.
        shortcutsMock.unifiedGlobalShortcuts = false;
        shortcutsMock.sglActivateEntryAtIndex(0);
        compare(indexerMock.visibleIndexCalls, 0);

        // enabled, non-matching entryIndex -> visibleIndex(index) IS called,
        // then the (visibleIndex===entryIndex) test is false so it stops there.
        shortcutsMock.unifiedGlobalShortcuts = true;
        shortcutsMock.sglActivateEntryAtIndex(999);
        compare(indexerMock.visibleIndexCalls, 1);
        compare(indexerMock.lastVisibleIndexArg, 4);
    }

    function test_shortcut_newInstance_guarded() {
        const a = make();
        a.index = 6;
        indexerMock.visibleIndexCalls = 0;

        shortcutsMock.unifiedGlobalShortcuts = false;
        shortcutsMock.sglNewInstanceForEntryAtIndex(0);
        compare(indexerMock.visibleIndexCalls, 0);

        shortcutsMock.unifiedGlobalShortcuts = true;
        shortcutsMock.sglNewInstanceForEntryAtIndex(999);
        compare(indexerMock.visibleIndexCalls, 1);
        compare(indexerMock.lastVisibleIndexArg, 6);
    }

    // ---- helpers --------------------------------------------------------------
    function newItem() {
        const c = Qt.createComponent("data:text/plain,import QtQuick; Item {}");
        verify(c.status === Component.Ready, c.errorString());
        return c.createObject(root, {});
    }

    function findTimer(a, ms) {
        const res = a.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].interval === ms && typeof res[i].restart === "function"
                    && res[i].hasOwnProperty("running"))
                return res[i];
        }
        return null;
    }

    // The ItemWrapper child exposes zoomScale + wrapperContainer + overlayIconLoader.
    function findWrapper(a) {
        const res = a.resources;
        for (var i = 0; i < res.length; i++) {
            const r = res[i];
            if (r && r.hasOwnProperty("zoomScale") && r.hasOwnProperty("wrapperContainer"))
                return r;
        }
        // wrapper is also reachable through the public alias.
        return a.wrapper ? a.wrapper : null;
    }

    // viewSignalsConnector is a Connections with a `pressed` and `blockWheel`
    // bool property pair — match on that shape.
    function findConnector(a) {
        const res = a.resources;
        for (var i = 0; i < res.length; i++) {
            const r = res[i];
            if (r && r.hasOwnProperty("pressed") && r.hasOwnProperty("blockWheel")) {
                return r;
            }
        }
        return null;
    }
}
