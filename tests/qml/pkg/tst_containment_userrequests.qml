// Coverage for the containment's UserRequests ability. The component subclasses
// AbilityDefinition.UserRequests (which declares `signal sglViewType(int)`) and
// adds a Connections block that forwards the view's userRequestedViewType signal:
//   onUserRequestedViewType: containerUserRequests.sglViewType(type)
//
// Unqualified context names the target reads:
//   view — its own `property QtObject view: null`; the Connections target.
// We assign a mock `view` carrying a `userRequestedViewType(int)` signal, emit
// it, and assert the forwarded sglViewType fires with the same type via a
// SignalSpy. That exercises the onUserRequestedViewType handler body.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "ContainmentUserRequests"
    when: windowShown

    // A mock latteView shaped for the Connections target: it only needs the
    // userRequestedViewType(int) signal the handler is wired to.
    Component {
        id: viewComponent
        QtObject {
            signal userRequestedViewType(int type)
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/UserRequests.qml")

    function make(view) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {"view": view});
        verify(obj, "instantiate failed");
        return obj;
    }

    // The Connections handler forwards view.userRequestedViewType(type) to this
    // component's own sglViewType(type). Emit on the mock view and assert the
    // forwarded signal fires with the same payload.
    function test_forwardsViewType() {
        const view = createTemporaryObject(viewComponent, root);
        verify(view, "view mock failed");
        const obj = make(view);

        const spy = createTemporaryObject(spyComponent, root, {"target": obj, "signalName": "sglViewType"});
        verify(spy, "spy failed");

        view.userRequestedViewType(2);
        compare(spy.count, 1, "sglViewType should fire once per emit");
        compare(spy.signalArguments[0][0], 2, "forwarded viewType payload mismatch");

        // A second, distinct emit forwards again with the new payload.
        view.userRequestedViewType(5);
        compare(spy.count, 2);
        compare(spy.signalArguments[1][0], 5);
    }

    // When view is null the Connections target is null, so emitting on a former
    // view has no wire. Re-pointing view at a live mock re-binds the Connections
    // target; emitting then forwards. This drives the target re-evaluation path.
    function test_rebindsViewTarget() {
        const obj = make(null);
        const spy = createTemporaryObject(spyComponent, root, {"target": obj, "signalName": "sglViewType"});
        verify(spy, "spy failed");

        const view = createTemporaryObject(viewComponent, root);
        obj.view = view;
        view.userRequestedViewType(7);

        compare(spy.count, 1, "re-pointed view should forward");
        compare(spy.signalArguments[0][0], 7);
    }

    Component {
        id: spyComponent
        SignalSpy {}
    }
}
