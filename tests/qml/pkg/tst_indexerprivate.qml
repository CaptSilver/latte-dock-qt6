// Coverage for the containment's IndexerPrivate ability. The component is an
// AbilityDefinition.Indexer (an Item) that reads two unqualified creation-context
// names: `layouts` (a property declared on the component itself, pointed at the
// three sub-layouts start/main/end) and `root` (only inside the
// clientsTrackingWindowsCount binding `when:`, as root.appletIsDragged). QML
// resolves both against this test file's root object, so we name the TestCase
// `root` and hand the component a real `layouts` Item built from mock applet
// items shaped exactly like the live ones (index / isSeparator / isHidden /
// isMarginsAreaSeparator / communicator{indexerIsSupported, requires, bridge}).
//
// The coverage instrumentation injects Cov.tick ONLY into the three functions
// (visibleItemsBeforeCount@255, visibleIndex@279, visibleIndexBelongsAtApplet@291);
// the Binding value blocks are not instrumented. So those three functions are the
// units this test claims. We let the real Bindings populate separators / hidden /
// marginsAreaSeparators / clients from the mock layout (asserted as observable
// values), then drive each function across its branches and assert the returns.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "IndexerPrivate"
    when: windowShown

    // The clientsTrackingWindowsCount binding's when: reads root.appletIsDragged
    // unqualified. Declare it so that binding resolves against this root instead
    // of throwing; false keeps the binding active (matches a non-dragging dock).
    property bool appletIsDragged: false

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/privates/IndexerPrivate.qml")

    // ---- mock applet-item factory ----------------------------------------
    // Each applet item is an Item with the flags/index/communicator the bindings
    // and functions read. communicator is null for "plain" applets; for indexer
    // clients it carries indexerIsSupported + a bridge.indexer.client.visibleItemsCount
    // and a requires.windowsTrackingEnabled flag.
    Component {
        id: appletComp
        Item {
            property int index: -1
            property bool isSeparator: false
            property bool isHidden: false
            property bool isMarginsAreaSeparator: false
            property var communicator: null
        }
    }

    Component {
        id: communicatorComp
        QtObject {
            property bool indexerIsSupported: false
            property int visibleItemsCount: 1
            property bool windowsTrackingEnabled: false
            // requires.windowsTrackingEnabled and bridge.indexer.client.visibleItemsCount
            // are nested the way the live communicator exposes them.
            property QtObject requires: QtObject {
                property bool windowsTrackingEnabled: false
            }
            property QtObject bridge: QtObject {
                property QtObject indexer: QtObject {
                    property QtObject client: QtObject {
                        property int visibleItemsCount: 1
                    }
                }
            }
        }
    }

    // A layouts Item with three sub-layout Items. children of each sub-layout are
    // the applet items; we add them per-test via makeApplet().
    Component {
        id: layoutsComp
        Item {
            property Item startLayout: Item {}
            property Item mainLayout: Item {}
            property Item endLayout: Item {}
        }
    }

    // Build an applet item parented into the given sub-layout (so it shows up in
    // sub-layout.children). For an indexer client, attach a communicator whose
    // bridge.indexer.client.visibleItemsCount = visCount and
    // requires.windowsTrackingEnabled = tracking.
    function makeApplet(parentLayout, idx, opts) {
        opts = opts || {};
        const a = appletComp.createObject(parentLayout, {
            index: idx,
            isSeparator: opts.isSeparator === true,
            isHidden: opts.isHidden === true,
            isMarginsAreaSeparator: opts.isMarginsAreaSeparator === true
        });
        verify(a, "applet item create failed");
        if (opts.client === true) {
            const comm = communicatorComp.createObject(a, {});
            verify(comm, "communicator create failed");
            comm.indexerIsSupported = true;
            comm.bridge.indexer.client.visibleItemsCount =
                    (opts.visCount === undefined ? 1 : opts.visCount);
            comm.requires.windowsTrackingEnabled = opts.tracking === true;
            a.communicator = comm;
        }
        return a;
    }

    function make(layoutsItem) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        // Hand layouts in at construction so every Binding has a live layouts to
        // walk (layouts defaults to null, which would throw in the value blocks).
        const obj = createTemporaryObject(c, root, {layouts: layoutsItem});
        verify(obj, "instantiate failed");
        return obj;
    }

    // The bindings compute separators/hidden/marginsAreaSeparators/clients from
    // the mock layout. Build a layout that exercises each and assert the bound
    // arrays, then assert the function outputs that depend on them.
    function test_boundIndexArrays() {
        const L = layoutsComp.createObject(root, {});
        verify(L, "layouts create failed");

        // start: a separator at 0, a plain applet at 1
        makeApplet(L.startLayout, 0, {isSeparator: true});
        makeApplet(L.startLayout, 1, {});
        // main: a hidden applet at 2, a margins-area separator at 3, a client at 4
        makeApplet(L.mainLayout, 2, {isHidden: true});
        makeApplet(L.mainLayout, 3, {isMarginsAreaSeparator: true});
        makeApplet(L.mainLayout, 4, {client: true, visCount: 2});
        // end: a plain applet at 5, and one with negative index (must be ignored)
        makeApplet(L.endLayout, 5, {});
        makeApplet(L.endLayout, -1, {isSeparator: true});

        const m = make(L);

        // Bindings are active (updateIsBlocked=false). Assert each computed array.
        compare(m.separators, [0], "separators should be the start-layout separator only");
        compare(m.hidden, [2], "hidden should be the main-layout hidden applet only");
        compare(m.marginsAreaSeparators, [3], "marginsAreaSeparators should be index 3");
        // clients are applets whose communicator.indexerIsSupported is true.
        compare(m.clients, [4], "clients should be the indexer-supported applet at 4");
    }

    // visibleItemsBeforeCount over one sub-layout, exercising all three inner
    // branches: skip (separator/hidden/marginsAreaSeparator index), single-item
    // (+1), multi-item (+ client.visibleItemsCount). actualIndex gates which
    // applets are counted (index < actualIndex).
    function test_visibleItemsBeforeCount() {
        const L = layoutsComp.createObject(root, {});
        verify(L, "layouts create failed");

        // start layout content (all in start so separators/hidden arrays apply):
        //   0: separator  -> skipped
        //   1: hidden     -> skipped
        //   2: marginsAreaSeparator -> skipped
        //   3: plain      -> +1 (single-item branch)
        //   4: client visCount=3 -> +3 (multi-item branch)
        makeApplet(L.startLayout, 0, {isSeparator: true});
        makeApplet(L.startLayout, 1, {isHidden: true});
        makeApplet(L.startLayout, 2, {isMarginsAreaSeparator: true});
        makeApplet(L.startLayout, 3, {});
        makeApplet(L.startLayout, 4, {client: true, visCount: 3});

        const m = make(L);
        // Confirm the bindings classified the skip indices so the function's
        // separators/hidden/marginsAreaSeparators reads are the real arrays.
        compare(m.separators, [0]);
        compare(m.hidden, [1]);
        compare(m.marginsAreaSeparators, [2]);

        // actualIndex past everything: 0,1,2 skipped; 3 -> +1; 4 -> +3  => 4
        compare(m.visibleItemsBeforeCount(L.startLayout, 99), 4);
        // actualIndex=4: counts indices <4 only -> 3 plain only => 1
        compare(m.visibleItemsBeforeCount(L.startLayout, 4), 1);
        // actualIndex=0: nothing < 0 => 0
        compare(m.visibleItemsBeforeCount(L.startLayout, 0), 0);
    }

    // visibleIndex: the early -1 branch (actualIndex is a separator or hidden),
    // and the summed branch (visibleItemsBeforeCount across the three layouts +1).
    function test_visibleIndex() {
        const L = layoutsComp.createObject(root, {});
        verify(L, "layouts create failed");

        // start: separator@0, plain@1
        makeApplet(L.startLayout, 0, {isSeparator: true});
        makeApplet(L.startLayout, 1, {});
        // main: hidden@2, plain@3
        makeApplet(L.mainLayout, 2, {isHidden: true});
        makeApplet(L.mainLayout, 3, {});
        // end: plain@4
        makeApplet(L.endLayout, 4, {});

        const m = make(L);
        compare(m.separators, [0]);
        compare(m.hidden, [2]);

        // actualIndex is a separator -> -1
        compare(m.visibleIndex(0), -1);
        // actualIndex is hidden -> -1
        compare(m.visibleIndex(2), -1);

        // actualIndex=4 (the last plain applet): items before it that are visible
        // are the plain ones at 1 and 3 => 2, +1 => 3.
        compare(m.visibleIndex(4), 3);
        // actualIndex=1 (first plain): nothing visible before => 0, +1 => 1.
        compare(m.visibleIndex(1), 1);
    }

    // visibleIndexBelongsAtApplet: the guard branch (negative index / null applet),
    // the exact-match branch, the multi-item range branch (client spanning several
    // visible indices), and the false fallthrough.
    function test_visibleIndexBelongsAtApplet() {
        const L = layoutsComp.createObject(root, {});
        verify(L, "layouts create failed");

        // start: plain@0
        const a0 = makeApplet(L.startLayout, 0, {});
        // main: a client@1 spanning visCount=3 visible slots
        const a1 = makeApplet(L.mainLayout, 1, {client: true, visCount: 3});
        // end: plain@2
        const a2 = makeApplet(L.endLayout, 2, {});

        const m = make(L);
        compare(m.clients, [1], "the client applet should be indexed");

        // guard: negative itemVisibleIndex -> false
        compare(m.visibleIndexBelongsAtApplet(a0, -1), false);
        // guard: null applet -> false
        compare(m.visibleIndexBelongsAtApplet(null, 1), false);

        // a0 is the first applet: its visibleIndex is 1. Exact match -> true.
        compare(m.visibleIndex(0), 1);
        compare(m.visibleIndexBelongsAtApplet(a0, 1), true);
        // a non-matching visible index for a single-item applet -> false fallthrough.
        compare(m.visibleIndexBelongsAtApplet(a0, 5), false);

        // a1 is a multi-item client at visibleIndex base = 2 (a0 contributes 1
        // before it), spanning 3 slots -> indices 2,3,4 all belong to it.
        const base = m.visibleIndex(1);
        compare(base, 2);
        compare(m.visibleIndexBelongsAtApplet(a1, 2), true);   // == base
        compare(m.visibleIndexBelongsAtApplet(a1, 4), true);   // within base..base+visCount
        compare(m.visibleIndexBelongsAtApplet(a1, 5), false);  // == base+visCount, out of range
    }
}
