// Coverage for the host ThinTooltip ability. The component subclasses the
// abilities.definition.ThinTooltip Item, so it carries the base properties
// (isEnabled, showIsBlocked, currentText, currentVisualParent) plus its own
// public-API Item, the two delayer Timers, and the onShowIsBlockedChanged
// handler. It is largely self-contained: aside from the embedded
// LatteCore.Dialog (whose Plasmoid.location binding is inert headlessly) it
// reads only its own ids and the base properties. `debug` is shadowed by the
// host's own `property Item debug: null`, so no creation-context mock is
// needed for the units we drive.
//
// Every test loads the instrumented staged copy by file URL and pins an
// observable effect: the dialog's visible/visualParent, the base
// currentText/currentVisualParent, or a timer firing.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "ThinTooltipHost"
    when: windowShown
    visible: true
    width: 200
    height: 60

    // Stand-in visual parents the show()/hide() paths store and compare.
    Item { id: parentA; width: 10; height: 10 }
    Item { id: parentB; width: 10; height: 10 }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/host/ThinTooltip.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // Locate the embedded LatteCore.Dialog among the host's resources. It is
    // the resource exposing a `visualParent` writable property + a `type`.
    function findDialog(m) {
        const res = m.resources;
        for (var i = 0; i < res.length; i++) {
            const r = res[i];
            if (r && r.hasOwnProperty("visualParent") && ("type" in r))
                return r;
        }
        return null;
    }

    function findTimerByInterval(m, wanted) {
        const res = m.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].interval === wanted)
                return res[i];
        }
        return null;
    }

    // show@51: enabled + not blocked stores the visual parent on both the base
    // and the dialog, truncates over-long text to maxCharacters, and arms the
    // show timer (dialog not yet visible). Assert the stored state.
    function test_show_storesParentAndText() {
        const m = make();
        m.isEnabled = true;
        m.showIsBlocked = false;

        m.show(parentA, "hello");
        compare(m.currentVisualParent, parentA);
        compare(m.currentText, "hello");

        const dlg = findDialog(m);
        verify(dlg, "embedded dialog not found");
        compare(dlg.visualParent, parentA);
    }

    // show@51 text-truncation branch: a string longer than maxCharacters (80)
    // is cut to maxCharacters-1 chars + "...".
    function test_show_truncatesLongText() {
        const m = make();
        m.isEnabled = true;
        m.showIsBlocked = false;

        const long = "x".repeat(120);
        m.show(parentA, long);
        compare(m.currentText.length, m.maxCharacters - 1 + 3);
        verify(m.currentText.endsWith("..."));
    }

    // show@51 blocked/disabled branch: when blocked we still update the visual
    // parent (the comment explains why the early return is disabled) but the
    // show timer must NOT be armed and the dialog stays hidden.
    function test_show_blockedStillUpdatesParent() {
        const m = make();
        m.isEnabled = true;
        m.showIsBlocked = true;

        const dlg = findDialog(m);
        verify(dlg, "embedded dialog not found");
        dlg.visible = false;

        m.show(parentB, "blocked");
        compare(m.currentVisualParent, parentB);
        compare(m.currentText, "blocked");
        // blocked -> show timer not started, dialog still hidden
        compare(dlg.visible, false);
    }

    // hide@73: hide(parent) only acts when the parent matches the current
    // visual parent — it records lastHidingVisualParent and arms the hide
    // timer. A mismatched parent is a no-op.
    function test_hide_matchesCurrentParent() {
        const m = make();
        m.isEnabled = true;
        m.showIsBlocked = false;
        m.show(parentA, "h");
        compare(m.currentVisualParent, parentA);

        // mismatched parent: no-op, lastHidingVisualParent stays null
        m.hide(parentB);
        compare(m.lastHidingVisualParent, null);

        // matching parent: records lastHidingVisualParent
        m.hide(parentA);
        compare(m.lastHidingVisualParent, parentA);
    }

    // publicApi.show@31 / publicApi.hide@35 forward to the host show()/hide().
    // Drive both through the public Item and assert the forwarded effect.
    function test_publicApi_forwards() {
        const m = make();
        m.isEnabled = true;
        m.showIsBlocked = false;

        m.publicApi.show(parentA, "viaApi");
        compare(m.currentVisualParent, parentA);
        compare(m.currentText, "viaApi");
        compare(m.publicApi.currentVisualParent, parentA);
        compare(m.publicApi.currentText, "viaApi");

        m.publicApi.hide(parentA);
        compare(m.lastHidingVisualParent, parentA);
    }

    // _showTimer.onTriggered@85: with a current visual parent set, firing the
    // show timer makes the dialog visible.
    function test_showTimer_makesDialogVisible() {
        const m = make();
        m.isEnabled = true;
        m.showIsBlocked = false;

        const dlg = findDialog(m);
        verify(dlg, "embedded dialog not found");
        dlg.visible = false;

        m.show(parentA, "t"); // arms _showTimer (interval 100)
        compare(m.currentVisualParent, parentA);
        tryVerify(function() { return dlg.visible === true; }, 3000,
                  "show timer never made the dialog visible");
    }

    // _hideTimer.onTriggered@100: with lastHidingVisualParent === current
    // visual parent, firing the hide timer hides the dialog and clears the
    // current parent / text.
    function test_hideTimer_clearsState() {
        const m = make();
        m.isEnabled = true;
        m.showIsBlocked = false;

        const dlg = findDialog(m);
        verify(dlg, "embedded dialog not found");

        m.show(parentA, "bye");
        compare(m.currentVisualParent, parentA);
        // make the dialog visible up front so we can observe the timer hiding it
        dlg.visible = true;

        m.hide(parentA); // arms _hideTimer; lastHidingVisualParent = parentA
        compare(m.lastHidingVisualParent, parentA);

        tryVerify(function() { return m.currentVisualParent === null; }, 3000,
                  "hide timer never cleared the current visual parent");
        compare(m.currentText, "");
        compare(m.lastHidingVisualParent, null);
        compare(dlg.visible, false);
    }

    // onShowIsBlockedChanged@42: with a current visual parent set and the
    // dialog hidden, unblocking shows the dialog; re-blocking hides it.
    function test_onShowIsBlockedChanged_togglesDialog() {
        const m = make();
        m.isEnabled = true;

        const dlg = findDialog(m);
        verify(dlg, "embedded dialog not found");

        // Set up a current visual parent without going through the timer.
        m.showIsBlocked = true;
        m.show(parentA, "x");
        compare(m.currentVisualParent, parentA);
        dlg.visible = false;

        // unblock -> handler shows the dialog
        m.showIsBlocked = false;
        compare(dlg.visible, true);

        // re-block while visible -> handler hides it
        m.showIsBlocked = true;
        compare(dlg.visible, false);
    }
}
