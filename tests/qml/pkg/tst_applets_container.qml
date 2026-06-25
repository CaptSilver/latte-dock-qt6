// Coverage spike for the containment layouter AppletsContainer. The component is
// an Item whose layout metrics (shownApplets, fillApplets, firstVisibleIndex, ...)
// are computed by a stack of Binding blocks that walk `grid.children`. The Bindings
// read unqualified context names (root, indexer, dragOverlay, inNormalFillCalculationsState,
// appletsInParentChange), so we name the TestCase `id: root` and declare each name on it,
// then assign a hand-built grid of mock applet items and assert the Bindings recomputed.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "AppletsContainer"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/privates/layouter/AppletsContainer.qml")

    // --- context names the target reads unqualified (resolved against this creation context) ---
    // `root.*` reads in the target hit these properties / functions:
    property bool isHorizontal: true
    property int maxJustifySplitterSize: 7
    property bool editMode: false
    property int updateIndexesCalls: 0
    function updateIndexes() { updateIndexesCalls = updateIndexesCalls + 1; }

    // standalone unqualified names:
    property bool appletsInParentChange: false      // feeds updateIsBlocked
    property bool inNormalFillCalculationsState: true
    property QtObject dragOverlay: null             // null -> the splitter-drag guard is satisfied
    property QtObject indexer: QtObject {
        property var hidden: []
        property var separators: []
    }

    // A grid Item whose visual children are the mock applet items the Bindings walk.
    Component {
        id: gridComp
        Item { id: gridItem }
    }

    // A mock applet item carrying the metric flags the Bindings read off grid.children[i].
    Component {
        id: appletComp
        Item {
            property bool isPlaceHolder: false
            property bool isAutoFillApplet: false
            property bool isRequestingFill: false
            property bool isHidden: false
            property bool isParabolicEdgeSpacer: false
            property bool isInternalViewSplitter: false
            property var applet: null
            property int index: -1
            // width/height already exist on Item; tests set them explicitly.
        }
    }

    function makeGrid() {
        const g = gridComp.createObject(root);
        verify(g, "grid mock instantiate failed");
        return g;
    }

    function makeApplet(parentGrid, props) {
        const a = appletComp.createObject(parentGrid, props ? props : {});
        verify(a, "applet mock instantiate failed");
        return a;
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root);
        verify(obj, "instantiate failed: " + c.errorString());
        return obj;
    }

    // count is a readonly binding on grid.children.length; assigning a populated grid
    // recomputes it. Also exercises the visible-children walk.
    function test_count_reads_grid_children() {
        const obj = make();
        const g = makeGrid();
        makeApplet(g, {});
        makeApplet(g, {});
        obj.grid = g;
        compare(obj.count, 2);
    }

    // shownApplets counts non-hidden items that are placeholders or carry a real applet;
    // pure splitters (no applet, not placeholder) and hidden items don't count.
    function test_shownApplets() {
        const obj = make();
        const g = makeGrid();
        makeApplet(g, { applet: ({}) });                       // real applet -> counts
        makeApplet(g, { isPlaceHolder: true });                // placeholder -> counts
        makeApplet(g, { applet: ({}), isHidden: true });       // hidden -> skipped
        makeApplet(g, { isInternalViewSplitter: true });       // splitter, no applet -> skipped
        obj.grid = g;
        compare(obj.shownApplets, 2);
    }

    // fillApplets counts autofill, visible, non-placeholder, non-edge-spacer items.
    function test_fillApplets() {
        const obj = make();
        const g = makeGrid();
        makeApplet(g, { isAutoFillApplet: true });                       // counts
        makeApplet(g, { isAutoFillApplet: true, isHidden: true });       // hidden -> skip
        makeApplet(g, { isAutoFillApplet: true, isPlaceHolder: true });  // placeholder -> skip
        makeApplet(g, { isAutoFillApplet: false });                      // not autofill -> skip
        obj.grid = g;
        compare(obj.fillApplets, 1);
    }

    // fillRealApplets requires isRequestingFill AND a real applet (separators excluded).
    function test_fillRealApplets() {
        const obj = make();
        const g = makeGrid();
        makeApplet(g, { isRequestingFill: true, applet: ({}) });   // counts
        makeApplet(g, { isRequestingFill: true, applet: null });   // no applet -> skip
        makeApplet(g, { isRequestingFill: false, applet: ({}) });  // not requesting -> skip
        obj.grid = g;
        compare(obj.fillRealApplets, 1);
    }

    // first/lastVisibleIndex scan indexes, skipping those listed in indexer.hidden/separators.
    function test_visible_index_range() {
        const obj = make();
        const g = makeGrid();
        makeApplet(g, { index: 5 });
        makeApplet(g, { index: 2 });
        makeApplet(g, { index: 9 });
        obj.grid = g;
        compare(obj.firstVisibleIndex, 2);
        compare(obj.lastVisibleIndex, 9);
    }

    // indexes present in indexer.hidden are excluded from the visible range.
    function test_visible_index_skips_hidden() {
        const obj = make();
        indexer.hidden = [2];     // exclude the lowest
        const g = makeGrid();
        makeApplet(g, { index: 5 });
        makeApplet(g, { index: 2 });
        makeApplet(g, { index: 9 });
        obj.grid = g;
        compare(obj.firstVisibleIndex, 5);   // 2 is hidden -> next lowest is 5
        compare(obj.lastVisibleIndex, 9);
        indexer.hidden = [];                 // restore for other tests
    }

    // With no visible items at all, firstVisibleIndex collapses to -1 (maxIndex sentinel path).
    function test_firstVisibleIndex_empty() {
        const obj = make();
        const g = makeGrid();
        obj.grid = g;
        compare(obj.firstVisibleIndex, -1);
        compare(obj.lastVisibleIndex, -1);
    }

    // onlyInternalSplitters is true iff every child is an internal splitter (and there's >=1).
    function test_onlyInternalSplitters_true() {
        const obj = make();
        const g = makeGrid();
        makeApplet(g, { isInternalViewSplitter: true });
        makeApplet(g, { isInternalViewSplitter: true });
        obj.grid = g;
        verify(obj.onlyInternalSplitters === true);
    }

    // A mixed grid (a real applet among splitters) -> not "only" internal splitters.
    function test_onlyInternalSplitters_false() {
        const obj = make();
        const g = makeGrid();
        makeApplet(g, { isInternalViewSplitter: true });
        makeApplet(g, { applet: ({}) });
        obj.grid = g;
        verify(obj.onlyInternalSplitters === false);
    }

    // sizeWithNoFillApplets sums width over non-fill visible items, charging splitters the
    // fixed maxJustifySplitterSize (root.maxJustifySplitterSize === 7) instead of their width.
    function test_sizeWithNoFillApplets() {
        const obj = make();
        const g = makeGrid();
        makeApplet(g, { width: 30, height: 0, applet: ({}) });                       // 30
        makeApplet(g, { width: 999, isInternalViewSplitter: true });                 // charged 7, not 999
        makeApplet(g, { width: 50, isAutoFillApplet: true });                        // fill -> excluded
        obj.grid = g;
        compare(obj.sizeWithNoFillApplets, 37);   // 30 + 7
    }

    // lengthWithoutSplitters sums width over visible non-splitter items.
    function test_lengthWithoutSplitters() {
        const obj = make();
        const g = makeGrid();
        makeApplet(g, { width: 30, applet: ({}) });                    // 30
        makeApplet(g, { width: 40, applet: ({}) });                    // 40
        makeApplet(g, { width: 999, isInternalViewSplitter: true });   // splitter excluded
        makeApplet(g, { width: 70, isHidden: true });                  // hidden excluded
        obj.grid = g;
        compare(obj.lengthWithoutSplitters, 70);   // 30 + 40
    }

    // When inNormalFillCalculationsState is false, the gated Bindings (sizeWithNoFillApplets,
    // lengthWithoutSplitters, onlyInternalSplitters) don't run -> values stay at their defaults.
    function test_gated_bindings_blocked() {
        inNormalFillCalculationsState = false;
        const obj = make();
        const g = makeGrid();
        makeApplet(g, { width: 30, applet: ({}) });
        obj.grid = g;
        compare(obj.sizeWithNoFillApplets, 0);     // binding `when` false -> default
        compare(obj.lengthWithoutSplitters, 0);
        // ungated bindings still fire:
        compare(obj.shownApplets, 1);
        inNormalFillCalculationsState = true;      // restore
    }

    // onCountChanged calls root.updateIndexes() only in edit mode. Toggle editMode and
    // grow the grid to bump count, asserting the side-effecting call fired.
    function test_onCountChanged_editMode() {
        editMode = true;
        updateIndexesCalls = 0;
        const obj = make();
        const g = makeGrid();
        obj.grid = g;                       // count 0 -> 0, no change yet
        makeApplet(g, {});                  // child added, but count binding needs a poke
        // Reassigning grid forces the count binding to re-evaluate to 1 (a real change).
        verify(updateIndexesCalls >= 1, "updateIndexes() should fire on count change in edit mode");
        editMode = false;                   // restore
    }
}
