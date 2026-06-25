// Coverage for the client-side MyView ability
// (declarativeimports/abilities/client/MyView.qml). The component derives from
// AbilityDefinition.MyView and reads a few creation-context names unqualified:
//   `Kirigami.Theme` (qualified import, the colorPalette fallback),
//   `Plasmoid.userConfiguring` (qualified attached object — undefined headlessly,
//    so the inner `local` fallback's inEditMode/inConfigureAppletsMode read falsy
//    without throwing).
// Everything else hangs off the component's own `bridge` property (an Item slot,
// null by default) and the inner `ref.myView` switch (bridge ? bridge.myView.host
// : local).
//
// Instrumented units the staged copy injects Cov.tick into:
//   onIsBridgeActiveChanged@64, Component.onCompleted@70,
//   Component.onDestruction@76, inCurrentLayout@82, action@86.
// The ~25 forwarding Bindings (isShownFully/inEditMode/colorPalette/...) aren't
// individually instrumented but run for real off either `local` or the bridge
// mock, so every property assertion pins an honest bound value.
//
// Every test asserts an observable effect: a returned value from
// inCurrentLayout()/action(), a forwarded property value, or the
// bridge.myView.client mock side-effect the lifecycle handlers write.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "MyViewClient"
    when: windowShown

    // Mock of the real host MyView the bridge points at. ref.myView is typed
    // `Item` (so is bridge.myView.host in the real bridge), and QML silently
    // keeps a non-Item null in an Item slot — so the host mock MUST be an Item,
    // not a QtObject (README rule 2). Shaped like the host ability: the
    // forwardable state the client's Bindings read, plus isReady /
    // inCurrentLayout() / action(name) that the client functions delegate to.
    Item {
        id: hostMock
        property bool isReady: true
        property int groupId: 7
        property bool inNormalState: true
        property bool isHidden: false
        property bool isShownPartially: false
        property bool isShownFully: false
        property bool isHidingBlocked: false
        property bool inEditMode: true
        property bool inConfigureAppletsMode: false
        property bool inSlidingIn: false
        property bool inSlidingOut: false
        property bool inRelocationAnimation: false
        property bool inRelocationHiding: false
        property bool badgesIn3DStyle: true
        property int alignment: 0
        property int visibilityMode: 0
        property real backgroundOpacity: 0.5
        property string lastUsedActivity: "act-42"
        property rect screenGeometry: Qt.rect(0, 0, 100, 50)
        property var containmentActions: []
        property QtObject itemShadow: QtObject {
            property bool isEnabled: false
            property int size: 0
            property color shadowColor: "#000000"
            property color shadowSolidColor: "#000000"
        }

        property bool current: true
        property int isCurrentCalls: 0
        function inCurrentLayout() { isCurrentCalls++; return current; }

        property var lastActionName: ""
        property int actionCalls: 0
        function action(name) { actionCalls++; lastActionName = name; return "act:" + name; }
    }

    // The bridge slot is `property Item` — a bare QtObject silently stays null
    // there, so the bridge mock must be an Item. myView.host feeds ref.myView
    // (also Item-typed); myView.client is writable so the lifecycle handlers'
    // write is observable. applyPalette/colorPalette drive the colorPalette
    // ternary.
    QtObject {
        id: bridgeMyView
        property var client: undefined
        property Item host: hostMock
    }
    // colorPalette is typed QtObject on the ability, so the bridge palette must
    // be a real QtObject (a JS object can't assign to a QObject* slot and would
    // read back null).
    QtObject { id: paletteSentinel; property string tag: "bridge-palette" }
    Item {
        id: bridgeMock
        property bool applyPalette: true
        property QtObject colorPalette: paletteSentinel
        property QtObject myView: bridgeMyView
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/client/MyView.qml")

    function make(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, props || {});
        verify(obj, "instantiate failed");
        return obj;
    }

    function resetBridge() {
        bridgeMyView.client = undefined;
        hostMock.isCurrentCalls = 0;
        hostMock.actionCalls = 0;
        hostMock.lastActionName = "";
    }

    // No bridge: ref.myView resolves to the inner `local` fallback. Its bindings
    // pin local's literals — isShownFully:true — and inEditMode reads
    // Plasmoid.userConfiguring, undefined headlessly => falsy. colorPalette's
    // ternary fails the bridge guard and falls back to Kirigami.Theme (a non-null
    // attached object). isBridgeActive is false.
    function test_localFallback_state() {
        resetBridge();
        const m = make({bridge: null});
        compare(m.isBridgeActive, false);
        compare(m.isShownFully, true);
        // Plasmoid attached object has no live containment -> userConfiguring is
        // undefined, so the bool property coerces to false.
        compare(m.inEditMode, false);
        compare(m.inConfigureAppletsMode, false);
        // colorPalette ternary took the Kirigami.Theme leg, not our sentinel.
        verify(m.colorPalette !== paletteSentinel);
        verify(m.colorPalette !== null);
    }

    // inCurrentLayout@82 / action@86 guard legs: with no bridge the `bridge && ...`
    // short-circuits, so inCurrentLayout returns the literal true and action
    // returns null — without ever touching the host mock.
    function test_functions_noBridge_fallbacks() {
        resetBridge();
        const m = make({bridge: null});
        compare(m.inCurrentLayout(), true);
        compare(m.action("show"), null);
        // the guard path never reached the host
        compare(hostMock.isCurrentCalls, 0);
        compare(hostMock.actionCalls, 0);
    }

    // Component.onCompleted@70: created with a live bridge -> isBridgeActive true
    // at completion, so the handler writes bridge.myView.client = _myView. Assert
    // the bridge received the component instance.
    function test_onCompleted_setsClientWhenActive() {
        resetBridge();
        const m = make({bridge: bridgeMock});
        compare(m.isBridgeActive, true);
        compare(bridgeMyView.client, m);
    }

    // Component.onCompleted@70 inactive leg: bridge null at completion -> the
    // handler runs but its if-guard is false, client stays untouched.
    function test_onCompleted_inactiveLeavesClient() {
        resetBridge();
        const m = make({bridge: null});
        compare(m.isBridgeActive, false);
        compare(bridgeMyView.client, undefined);
    }

    // onIsBridgeActiveChanged@64: start with no bridge, then assign one.
    // isBridgeActive flips false->true and the changed handler writes the client.
    function test_onIsBridgeActiveChanged_activate() {
        resetBridge();
        const m = make({bridge: null});
        compare(m.isBridgeActive, false);
        compare(bridgeMyView.client, undefined);

        m.bridge = bridgeMock;            // null -> non-null toggles isBridgeActive
        compare(m.isBridgeActive, true);
        compare(bridgeMyView.client, m);
    }

    // With a live bridge ref.myView resolves to bridge.myView.host, so the
    // forwarding Bindings thread the host's state onto the client, and colorPalette
    // takes the bridge leg (applyPalette true) -> our sentinel.
    function test_bridge_forwardsHostState() {
        resetBridge();
        const m = make({bridge: bridgeMock});
        compare(m.groupId, 7);
        compare(m.isShownFully, false);
        compare(m.inEditMode, true);
        compare(m.backgroundOpacity, 0.5);
        compare(m.lastUsedActivity, "act-42");
        compare(m.colorPalette, paletteSentinel);

        // applyPalette false -> colorPalette ternary falls back to Kirigami.Theme.
        bridgeMock.applyPalette = false;
        verify(m.colorPalette !== paletteSentinel);
    }

    // inCurrentLayout@82 / action@86 body legs: with a live, ready bridge the
    // functions delegate to ref.myView (the host). Assert the threaded-through
    // return values and that the host actually ran.
    function test_functions_bridge_delegate() {
        resetBridge();
        const m = make({bridge: bridgeMock});

        hostMock.current = true;
        compare(m.inCurrentLayout(), true);
        compare(hostMock.isCurrentCalls, 1);
        hostMock.current = false;
        compare(m.inCurrentLayout(), false);
        compare(hostMock.isCurrentCalls, 2);

        compare(m.action("toggle"), "act:toggle");
        compare(hostMock.actionCalls, 1);
        compare(hostMock.lastActionName, "toggle");
    }

    // inCurrentLayout@82 / action@86 not-ready leg: bridge present but host
    // isReady false -> the `bridge && ref.myView.isReady` guard fails, so
    // inCurrentLayout returns the literal true fallback and action returns null,
    // again without invoking the host's methods.
    function test_functions_bridge_notReady() {
        resetBridge();
        hostMock.isReady = false;
        const m = make({bridge: bridgeMock});

        compare(m.inCurrentLayout(), true);
        compare(m.action("x"), null);
        compare(hostMock.isCurrentCalls, 0);
        compare(hostMock.actionCalls, 0);

        hostMock.isReady = true;          // restore for other tests
    }

    // Component.onDestruction@76: with a live bridge, destroying the instance runs
    // the destruction handler's active branch, clearing bridge.myView.client back
    // to null. The bridge outlives the component (retained on root), so the cleared
    // value is observable after teardown.
    function test_onDestruction_clearsClient() {
        resetBridge();
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = c.createObject(root, {bridge: bridgeMock});
        verify(obj, "instantiate failed");
        compare(bridgeMyView.client, obj);   // onCompleted set it

        obj.destroy();
        tryCompare(bridgeMyView, "client", null, 3000,
                   "onDestruction did not clear bridge.myView.client");
    }
}
