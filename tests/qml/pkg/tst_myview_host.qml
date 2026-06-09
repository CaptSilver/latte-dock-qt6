// Drives the host MyView ability (abilities/host/MyView.qml) through its two
// pure functions and the two publicApi forwarders. The component is loaded
// from the staged (instrumented) package by file URL so the Cov.tick calls
// fire, and every assertion pins an observable effect: a return value or a
// mock side-effect.
//
// The only name the functions read is `view` — but that is the component's
// own `property QtObject view: null`, not a creation-context name, so it is
// set on the instance instead of mocked on the TestCase. We shape `view` like
// the real LatteView: a `layout` carrying `isCurrent()` and an `action(name)`
// method, both recording their call so the forwards are asserted, not merely
// executed.
//
// Four units are instrumented: inCurrentLayout@15, action@19, and the publicApi
// forwarders action@60 / inCurrentLayout@64. The forwarders delegate to the
// `apis` functions, so calling them ticks both the forwarder and the underlying
// function. All four are exercised honestly with both the view-null guard path
// and the view-present body.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "MyViewHost"
    when: windowShown

    // Shaped mock of the real LatteView the `view` slot points at. layout
    // carries isCurrent() (read by inCurrentLayout) and the top object carries
    // action(name) (read by action). Each records so the call is asserted.
    QtObject {
        id: layoutMock
        property bool current: true
        property int isCurrentCalls: 0
        function isCurrent() { isCurrentCalls++; return current; }
    }
    QtObject {
        id: viewMock
        property QtObject layout: layoutMock
        property var lastActionName: ""
        property int actionCalls: 0
        // The real view.action(name) returns a QAction-like object; a plain
        // sentinel string is enough to assert the return threaded through.
        function action(name) { actionCalls++; lastActionName = name; return "act:" + name; }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/host/MyView.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // inCurrentLayout@15 guard path: with view null the `view && ...` short
    // circuits to a falsy value before any deref. action@19 guard path: with
    // view null the ternary returns null. Neither touches the mocks.
    function test_nullView_guards() {
        const m = make();
        m.view = null;
        layoutMock.isCurrentCalls = 0;
        viewMock.actionCalls = 0;

        // inCurrentLayout returns a falsy short-circuit value (null), not true.
        verify(!m.inCurrentLayout());
        // action returns null when there is no view.
        compare(m.action("show"), null);

        // The guard path never reached the mock.
        compare(layoutMock.isCurrentCalls, 0);
        compare(viewMock.actionCalls, 0);
    }

    // inCurrentLayout@15 body: with a live view it threads view.layout.isCurrent()
    // out. Assert both the returned value and that the mock's isCurrent ran, for
    // both the true and false legs.
    function test_inCurrentLayout_body() {
        const m = make();
        m.view = viewMock;

        layoutMock.current = true;
        layoutMock.isCurrentCalls = 0;
        verify(m.inCurrentLayout() === true);
        compare(layoutMock.isCurrentCalls, 1);

        layoutMock.current = false;
        verify(m.inCurrentLayout() === false);
        compare(layoutMock.isCurrentCalls, 2);
    }

    // action@19 body: with a live view it returns view.action(name). Assert the
    // forwarded name and the threaded-through return value.
    function test_action_body() {
        const m = make();
        m.view = viewMock;
        viewMock.actionCalls = 0;
        viewMock.lastActionName = "";

        compare(m.action("toggle"), "act:toggle");
        compare(viewMock.actionCalls, 1);
        compare(viewMock.lastActionName, "toggle");
    }

    // publicApi.action@60 forwards to apis.action@19; publicApi.inCurrentLayout@64
    // forwards to apis.inCurrentLayout@15. Driving them through publicApi ticks
    // both the forwarder and the underlying function. Assert the value threaded
    // all the way out and the mock recorded the forwarded call.
    function test_publicApi_forwarders() {
        const m = make();
        m.view = viewMock;
        const api = m.publicApi;
        verify(api, "publicApi missing");

        viewMock.actionCalls = 0;
        viewMock.lastActionName = "";
        compare(api.action("config"), "act:config");
        compare(viewMock.actionCalls, 1);
        compare(viewMock.lastActionName, "config");

        layoutMock.current = true;
        layoutMock.isCurrentCalls = 0;
        verify(api.inCurrentLayout() === true);
        compare(layoutMock.isCurrentCalls, 1);
    }
}
