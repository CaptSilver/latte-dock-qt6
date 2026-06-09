// Coverage for the containment Launchers ability (abilities/Launchers.qml),
// a LaunchersPrivate subclass. It reads a handful of unqualified context
// names: layoutsManager (its .syncedLaunchers carries the sync methods),
// latteView (+ .layout), universalSettings, layoutName, plus root/layouter
// for the updateIsBlocked binding. QML resolves those names against the
// component's creation context, so we name the TestCase `id: root` and shape
// each name like the real object here.
//
// The inherited LaunchersPrivate stealing-applet Bindings walk an unqualified
// `layouts` model (layouts.start/main/endLayout.children). A live containment
// supplies it; headlessly we give it empty Item layouts so the binding loops
// over zero children and returns -1/null cleanly instead of throwing on a null
// deref. The stealing-applet helpers are then driven directly by assigning
// appletStealingDroppedLaunchers, a `property Item` on the private overridable
// from here.
//
// Every test asserts an observable effect: a mock side-effect (the forwarded
// call landed with the right arguments), a property write the setter performed,
// or a bound-property value.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "LaunchersContainment"
    when: windowShown

    // layoutName lives on the component itself (it declares its own
    // `property string layoutName: ""`, which shadows any creation-context
    // name), so make() sets it on the instance; the synced forwards pass it as
    // their first argument and we assert it arrives unchanged.
    readonly property string testLayoutName: "MyLayout"

    QtObject {
        id: layouter
        property bool appletsInParentChange: false
    }

    // root.dragOverlay is read by the updateIsBlocked binding; null is fine.
    property var dragOverlay: null

    // LaunchersPrivate's two stealing-applet Bindings walk layouts.start/main/
    // endLayout.children. Give them real empty Item layouts so the binding
    // evaluates cleanly (loops over zero children, returns -1/null) instead of
    // dereferencing a null `layouts` and emitting a TypeError.
    Item {
        id: layouts
        property Item startLayout: Item {}
        property Item mainLayout: Item {}
        property Item endLayout: Item {}
    }

    // Mock of the synced-launchers backend. Each method records its arguments
    // so the forwarding functions are asserted, not merely executed. Not a
    // catch-all: only the methods Launchers.qml actually calls.
    QtObject {
        id: syncedLaunchersObj
        property var lastCall: ""
        property var lastArgs: []
        property int addAbilityClientCalls: 0
        property int removeAbilityClientCalls: 0

        function addAbilityClient(client) { addAbilityClientCalls++; lastCall = "addAbilityClient"; lastArgs = [client]; }
        function removeAbilityClient(client) { removeAbilityClientCalls++; lastCall = "removeAbilityClient"; lastArgs = [client]; }
        function addLauncher(layout, senderId, group, groupId, url) {
            lastCall = "addLauncher"; lastArgs = [layout, senderId, group, groupId, url];
        }
        function removeLauncher(layout, senderId, group, groupId, url) {
            lastCall = "removeLauncher"; lastArgs = [layout, senderId, group, groupId, url];
        }
        function addLauncherToActivity(layout, senderId, group, groupId, url, activityId) {
            lastCall = "addLauncherToActivity"; lastArgs = [layout, senderId, group, groupId, url, activityId];
        }
        function removeLauncherFromActivity(layout, senderId, group, groupId, url, activityId) {
            lastCall = "removeLauncherFromActivity"; lastArgs = [layout, senderId, group, groupId, url, activityId];
        }
        function urlsDropped(layout, senderId, group, groupId, urls) {
            lastCall = "urlsDropped"; lastArgs = [layout, senderId, group, groupId, urls];
        }
        function validateLaunchersOrder(layout, senderId, group, groupId, ordered) {
            lastCall = "validateLaunchersOrder"; lastArgs = [layout, senderId, group, groupId, ordered];
        }
    }

    QtObject {
        id: layoutsManager
        property QtObject syncedLaunchers: syncedLaunchersObj
    }

    // latteView.layout.launchers is the layout-launcher sink; universalSettings
    // .launchers the universal one. Both writable so the setters' writes show up.
    QtObject {
        id: layoutObj
        property var launchers: []
    }
    QtObject {
        id: latteView
        property QtObject layout: layoutObj
    }
    QtObject {
        id: universalSettings
        property var launchers: []
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/Launchers.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        // layoutName is the component's own property; seed it so the synced
        // forwards have a non-empty layout to pass through.
        const obj = createTemporaryObject(c, root, {layoutName: testLayoutName, layouts: layouts});
        verify(obj, "instantiate failed");
        return obj;
    }

    // The ability's stealing-applet helpers gate on hasStealingApplet, which is
    // appletStealingDroppedLaunchers !== null on the private. Give it a real Item
    // mock (the slot is `property Item`, so a QtObject would silently stay null
    // and the branch would never run). The Item carries the one method and the
    // one writable flag the helpers touch.
    Component {
        id: stealingAppletComponent
        Item {
            property var lastDropped: undefined
            property int dropCalls: 0
            property bool isShowingAddLaunchersMessage: false
            function addDroppedLaunchers(launchers) { dropCalls++; lastDropped = launchers; }
        }
    }

    // addAbilityClient / removeAbilityClient guard on layoutsManager.syncedLaunchers
    // then forward; assert the mock saw the call with the passed client.
    function test_abilityClient_addRemove() {
        const m = make();
        const before = syncedLaunchersObj.addAbilityClientCalls;
        m.addAbilityClient("clientA");
        compare(syncedLaunchersObj.addAbilityClientCalls, before + 1);
        compare(syncedLaunchersObj.lastCall, "addAbilityClient");
        compare(syncedLaunchersObj.lastArgs[0], "clientA");

        const rbefore = syncedLaunchersObj.removeAbilityClientCalls;
        m.removeAbilityClient("clientB");
        compare(syncedLaunchersObj.removeAbilityClientCalls, rbefore + 1);
        compare(syncedLaunchersObj.lastCall, "removeAbilityClient");
        compare(syncedLaunchersObj.lastArgs[0], "clientB");
    }

    // addSyncedLauncher forwards (layoutName, senderId, group, groupId, url) to
    // syncedLaunchers.addLauncher. Assert the layoutName prefix + all args land.
    function test_addSyncedLauncher() {
        const m = make();
        m.addSyncedLauncher(7, 2, 5, "applications:firefox.desktop");
        compare(syncedLaunchersObj.lastCall, "addLauncher");
        compare(syncedLaunchersObj.lastArgs[0], testLayoutName);
        compare(syncedLaunchersObj.lastArgs[1], 7);
        compare(syncedLaunchersObj.lastArgs[2], 2);
        compare(syncedLaunchersObj.lastArgs[3], 5);
        compare(syncedLaunchersObj.lastArgs[4], "applications:firefox.desktop");
    }

    function test_removeSyncedLauncher() {
        const m = make();
        m.removeSyncedLauncher(1, 0, 3, "a:b.desktop");
        compare(syncedLaunchersObj.lastCall, "removeLauncher");
        compare(syncedLaunchersObj.lastArgs[0], testLayoutName);
        compare(syncedLaunchersObj.lastArgs[4], "a:b.desktop");
    }

    function test_addSyncedLauncherToActivity() {
        const m = make();
        m.addSyncedLauncherToActivity(9, 1, 4, "a:c.desktop", "act-1");
        compare(syncedLaunchersObj.lastCall, "addLauncherToActivity");
        compare(syncedLaunchersObj.lastArgs[0], testLayoutName);
        compare(syncedLaunchersObj.lastArgs[4], "a:c.desktop");
        compare(syncedLaunchersObj.lastArgs[5], "act-1");
    }

    function test_removeSyncedLauncherFromActivity() {
        const m = make();
        m.removeSyncedLauncherFromActivity(3, 2, 6, "a:d.desktop", "act-2");
        compare(syncedLaunchersObj.lastCall, "removeLauncherFromActivity");
        compare(syncedLaunchersObj.lastArgs[0], testLayoutName);
        compare(syncedLaunchersObj.lastArgs[4], "a:d.desktop");
        compare(syncedLaunchersObj.lastArgs[5], "act-2");
    }

    // addDroppedLaunchers forwards an array of urls under urlsDropped.
    function test_addDroppedLaunchers() {
        const m = make();
        const urls = ["a:e.desktop", "a:f.desktop"];
        m.addDroppedLaunchers(4, 1, 2, urls);
        compare(syncedLaunchersObj.lastCall, "urlsDropped");
        compare(syncedLaunchersObj.lastArgs[0], testLayoutName);
        compare(syncedLaunchersObj.lastArgs[4].length, 2);
        compare(syncedLaunchersObj.lastArgs[4][1], "a:f.desktop");
    }

    function test_validateSyncedLaunchersOrder() {
        const m = make();
        const ordered = ["a:g.desktop", "a:h.desktop", "a:i.desktop"];
        m.validateSyncedLaunchersOrder(2, 0, 1, ordered);
        compare(syncedLaunchersObj.lastCall, "validateLaunchersOrder");
        compare(syncedLaunchersObj.lastArgs[0], testLayoutName);
        compare(syncedLaunchersObj.lastArgs[4].length, 3);
        compare(syncedLaunchersObj.lastArgs[4][2], "a:i.desktop");
    }

    // The three stealing-applet helpers. With appletStealingDroppedLaunchers set
    // to a real Item, hasStealingApplet is true so each branch body runs:
    // addDroppedLaunchersInStealingApplet forwards the list, and the show/hide
    // helpers flip isShowingAddLaunchersMessage on that Item.
    function test_stealingApplet_helpers() {
        const m = make();
        const stealer = createTemporaryObject(stealingAppletComponent, root, {});
        verify(stealer, "stealing applet mock failed to instantiate");
        m.appletStealingDroppedLaunchers = stealer;
        verify(m.hasStealingApplet);

        const list = ["a:j.desktop"];
        m.addDroppedLaunchersInStealingApplet(list);
        compare(stealer.dropCalls, 1);
        compare(stealer.lastDropped[0], "a:j.desktop");

        m.showAddLaunchersMessageInStealingApplet();
        compare(stealer.isShowingAddLaunchersMessage, true);

        m.hideAddLaunchersMessageInStealingApplet();
        compare(stealer.isShowingAddLaunchersMessage, false);
    }

    // The stealing-applet helpers no-op when there is no stealing applet
    // (appletStealingDroppedLaunchers null -> hasStealingApplet false). The
    // function still ticks; assert nothing changed and no throw escaped.
    function test_stealingApplet_helpers_noStealer() {
        const m = make();
        m.appletStealingDroppedLaunchers = null;
        verify(!m.hasStealingApplet);
        // Must not throw and must not touch any backend.
        m.addDroppedLaunchersInStealingApplet(["x"]);
        m.showAddLaunchersMessageInStealingApplet();
        m.hideAddLaunchersMessageInStealingApplet();
        compare(m.hasStealingApplet, false);
    }

    // setLayoutLaunchers writes latteView.layout.launchers when
    // isCapableOfLayoutLaunchers (latteView && latteView.layout) is true.
    function test_setLayoutLaunchers() {
        const m = make();
        verify(m.isCapableOfLayoutLaunchers);
        const newList = ["a:k.desktop", "a:l.desktop"];
        m.setLayoutLaunchers(newList);
        compare(layoutObj.launchers.length, 2);
        compare(layoutObj.launchers[0], "a:k.desktop");
    }

    // setUniversalLaunchers writes universalSettings.launchers when
    // isCapableOfUniversalLaunchers (latteView && universalSettings) is true.
    function test_setUniversalLaunchers() {
        const m = make();
        verify(m.isCapableOfUniversalLaunchers);
        const newList = ["a:m.desktop"];
        m.setUniversalLaunchers(newList);
        compare(universalSettings.launchers.length, 1);
        compare(universalSettings.launchers[0], "a:m.desktop");
    }
}
