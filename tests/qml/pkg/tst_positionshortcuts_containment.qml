// Coverage for the containment PositionShortcuts ability
// (abilities/PositionShortcuts.qml), a PositionShortcutsPrivate subclass that
// adds four functions called from the Latte C++ side. Each is driven through
// its real entry and every assertion pins an observable effect: a property the
// setter wrote, a signal the function emitted, or the return value forwarded
// from a shaped mock.
//
// The instantiated tree reads a few unqualified creation-context names: the
// updateIsBlocked override reads root.dragOverlay + layouter.appletsInParentChange;
// the inherited badges Binding reads shortcutsEngine; the inherited
// appletIdStealingPositionShortcuts Binding walks layouts.start/main/endLayout
// .children; and appletIdForIndex reads indexer. QML resolves those against the
// component's creation context, so we name the TestCase `id: root` and shape
// each name like the real object here (the private's own `property Item layouts`
// shadows the context name, so we seed it on the instance in make()).
import QtQuick
import QtTest

TestCase {
    id: root
    name: "PositionShortcutsContainment"
    when: windowShown

    // updateIsBlocked override reads root.dragOverlay (and .pressed); null makes
    // the && short-circuit to false cleanly, so updateIsBlocked stays false and
    // the inherited Bindings evaluate instead of being held back.
    property var dragOverlay: null

    QtObject {
        id: layouter
        property bool appletsInParentChange: false
    }

    // The inherited badges Binding's when/value both guard on shortcutsEngine;
    // null short-circuits both, so the binding settles to [] without throwing.
    property var shortcutsEngine: null

    // The inherited appletIdStealingPositionShortcuts Binding walks
    // layouts.start/main/endLayout.children. Give it real empty Item layouts so
    // the loops iterate over zero children and the binding returns -1 cleanly
    // instead of dereferencing a null `layouts`. Passed to the instance in
    // make() because the private declares its own `property Item layouts` which
    // shadows this creation-context name.
    Item {
        id: layouts
        property Item startLayout: Item {}
        property Item mainLayout: Item {}
        property Item endLayout: Item {}
    }

    // appletIdForIndex forwards entryIndex to indexer.appletIdForVisibleIndex and
    // returns its result. Shaped (not catch-all): records the index it was asked
    // about and returns a sentinel so the forward + return are both asserted.
    QtObject {
        id: indexer
        property int lastVisibleIndex: -999
        function appletIdForVisibleIndex(visibleIndex) {
            lastVisibleIndex = visibleIndex;
            return 4242;
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/PositionShortcuts.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {layouts: layouts});
        verify(obj, "instantiate failed");
        return obj;
    }

    // setShowAppletShortcutBadges is called from the globalshortcuts C++ side and
    // is a pure four-property write. Assert each of the four destinations took the
    // passed value (the inherited base/private declare showPositionShortcutBadges,
    // showAppletShortcutBadges, showMetaBadge, applicationLauncherId).
    function test_setShowAppletShortcutBadges_writesAllFour() {
        const m = make();
        m.setShowAppletShortcutBadges(true, true, true, 17);
        compare(m.showPositionShortcutBadges, true);
        compare(m.showAppletShortcutBadges, true);
        compare(m.showMetaBadge, true);
        compare(m.applicationLauncherId, 17);

        // a second call overwrites with the new values (no latching).
        m.setShowAppletShortcutBadges(false, false, false, -3);
        compare(m.showPositionShortcutBadges, false);
        compare(m.showAppletShortcutBadges, false);
        compare(m.showMetaBadge, false);
        compare(m.applicationLauncherId, -3);
    }

    // activateEntryAtIndex emits sglActivateEntryAtIndex(entryIndex) for a numeric
    // argument and early-returns (no emission) for a non-number. Assert both legs
    // of the typeof guard via a SignalSpy.
    function test_activateEntryAtIndex_emitsOnNumber() {
        const m = make();
        const spy = createTemporaryObject(signalSpyComponent, root,
                                          {target: m, signalName: "sglActivateEntryAtIndex"});
        m.activateEntryAtIndex(5);
        compare(spy.count, 1);
        compare(spy.signalArguments[0][0], 5);

        // non-number -> early return, no second emission.
        m.activateEntryAtIndex("not-a-number");
        compare(spy.count, 1);
    }

    // newInstanceForEntryAtIndex mirrors activateEntryAtIndex over the
    // sglNewInstanceForEntryAtIndex signal; assert both guard legs.
    function test_newInstanceForEntryAtIndex_emitsOnNumber() {
        const m = make();
        const spy = createTemporaryObject(signalSpyComponent, root,
                                          {target: m, signalName: "sglNewInstanceForEntryAtIndex"});
        m.newInstanceForEntryAtIndex(8);
        compare(spy.count, 1);
        compare(spy.signalArguments[0][0], 8);

        m.newInstanceForEntryAtIndex({});
        compare(spy.count, 1);
    }

    // appletIdForIndex forwards entryIndex to indexer.appletIdForVisibleIndex and
    // returns its result. Assert the mock saw the index and the sentinel returned.
    function test_appletIdForIndex_forwardsToIndexer() {
        const m = make();
        indexer.lastVisibleIndex = -999;
        const id = m.appletIdForIndex(3);
        compare(indexer.lastVisibleIndex, 3);
        compare(id, 4242);
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
