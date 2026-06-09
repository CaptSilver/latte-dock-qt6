// Coverage for the group preview's ToolTipDelegate2.qml (the ScrollView that
// hosts the per-window preview grid). The component is loaded from the staged
// (instrumented) package by file URL so the Cov.tick calls fire, and every
// assertion pins an observable effect.
//
// Two units are instrumented in this file: instanceAtPos@135 (a pure geometry
// helper) and the DropArea onDragLeave@85 handler. The remaining handler
// (onDragMove, an arrow function) and the Grid's hasVisibleDescription binding
// are not instrumented by the tool, so they earn no unit.
//
// Unqualified creation-context names the two units resolve against:
//   - windowsPreviewDlg  : onDragLeave calls windowsPreviewDlg.hide(9.9)
//   - tasksModel         : the DelegateModel/onDragMove path (live-only here)
// We shape windowsPreviewDlg + tasksModel as lowercase-id QtObjects on the
// TestCase root (the component's creation context). The mocks record their
// call so the handler body is asserted, not merely entered.
//
// Headless reality (probed): we keep the preview Loader inactive by setting
// isLauncher=true, so the heavy ToolTipInstance delegate (which dereferences
// mpris2Source/appletAbilities/toolTipDelegate from a live dock) never
// materialises. That means instanceAtPos walks an empty contentItem.children
// and returns null honestly — its entry + empty-loop return run for real. The
// loop body (matching a real preview instance) and the onDragMove handler need
// a live group ListView with real ToolTipInstance delegates -> live-only.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "ToolTipDelegate2"
    when: windowShown

    // onDragLeave's only effect is windowsPreviewDlg.hide(9.9). Record the call
    // count + the argument so the handler body (past the entry tick) is asserted.
    // Shaped, not a catch-all: only hide() is declared, the one member touched.
    property int hideCalls: 0
    property real hideValue: -1
    QtObject {
        id: windowsPreviewDlg
        function hide(v) { root.hideCalls++; root.hideValue = v; }
    }

    // tasksModel is read by onDragMove (requestActivate) and the DelegateModel
    // (makeModelIndex); both of those paths are live-only here. Declared shaped
    // anyway so its name resolves cleanly in the creation context rather than
    // leaving a dangling ReferenceError on the component.
    QtObject {
        id: tasksModel
        property int activateCalls: 0
        function requestActivate(idx) { activateCalls++; }
        function makeModelIndex(rowIdx, idx) { return [rowIdx, idx]; }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/previews/ToolTipDelegate2.qml")

    // isLauncher=true keeps the preview Loader inactive so the heavy
    // ToolTipInstance delegate is never instantiated; the ScrollView itself
    // builds cleanly headless.
    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {isLauncher: true});
        verify(obj, "instantiate failed");
        return obj;
    }

    // Depth-first walk over children + data to find the DeclarativeDropArea so
    // we can emit its dragLeave signal directly (no live drag session needed).
    function findDropArea(node, depth) {
        if (!node || depth > 12)
            return null;
        if (node.toString().indexOf("DeclarativeDropArea") >= 0)
            return node;
        var kids = (node.children !== undefined) ? node.children : [];
        for (var i = 0; i < kids.length; i++) {
            var f = findDropArea(kids[i], depth + 1);
            if (f) return f;
        }
        var res = (node.data !== undefined) ? node.data : [];
        for (var j = 0; j < res.length; j++) {
            var f2 = findDropArea(res[j], depth + 1);
            if (f2) return f2;
        }
        return null;
    }

    // instanceAtPos walks the preview instances and returns the one whose
    // bounding box contains (x,y), else null. With the Loader inactive the
    // non-group branch reads contentItem.children (empty), so the loop never
    // iterates and the function returns null. Asserting null pins the entry +
    // empty-loop return that run honestly headless.
    function test_instanceAtPos_emptyReturnsNull() {
        const m = make();
        compare(m.isGroup, false);
        compare(m.instanceAtPos(0, 0), null);
        // a different coordinate still has no instance to hit -> null
        compare(m.instanceAtPos(50, 75), null);
    }

    // onDragLeave fires windowsPreviewDlg.hide(9.9). Emit the DropArea's
    // dragLeave signal (its event arg is ignored by the handler, so null is
    // accepted) and assert the mock recorded exactly one hide() with 9.9 —
    // proving the handler body ran past the entry tick, not just banked it.
    function test_onDragLeave_hidesPreview() {
        const m = make();
        const da = findDropArea(m, 0);
        verify(da, "could not locate the DropArea");

        root.hideCalls = 0;
        root.hideValue = -1;
        da.dragLeave(null);
        compare(root.hideCalls, 1);
        compare(root.hideValue, 9.9);

        // a second leave fires the handler again -> second recorded hide.
        da.dragLeave(null);
        compare(root.hideCalls, 2);
    }
}
