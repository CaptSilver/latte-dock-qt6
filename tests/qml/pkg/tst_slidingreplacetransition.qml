// Coverage for the shell's SlidingReplaceTransition, the page slide both the dock
// settings dialog and the indicator sub-options hand to StackView.replaceEnter and
// replaceExit.
//
// A Transition's `animations` list reads back empty from JS, so the assertions here
// drive a real StackView.replace() and watch where the two pages start and land.
//
// The slide distance is a property, not the transition's own width, and that is what most
// of this pins down: the dock settings dialog slides by the width of the background behind
// its StackView, while the StackView's own width follows whatever page it holds. A
// self-measuring component would silently slide those pages by the wrong distance.
import QtQuick
import QtTest
import QtQuick.Controls 2.15 as QQC2
import Stage 1.0

TestCase {
    id: root
    name: "SlidingReplaceTransition"
    when: windowShown
    visible: true
    width: 400
    height: 200

    readonly property url targetUrl: Stage.share("plasma/shells/org.kde.latte.shell/contents/controls/SlidingReplaceTransition.qml")

    readonly property real distance: 200

    Component {
        id: pageComponent
        Rectangle { width: 300; height: 100 }
    }

    QQC2.StackView {
        id: stack
        width: 300
        height: 100
    }

    function make(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, props);
        verify(obj, "instantiate failed");
        return obj;
    }

    // Wires both halves of one swap onto the stack and returns the pages, outgoing first.
    // The pages are ours rather than the stack's so they survive being popped off.
    function swap(forward) {
        stack.replaceEnter = make({ entering: true, forward: forward, slideWidth: root.distance });
        stack.replaceExit = make({ entering: false, forward: forward, slideWidth: root.distance });

        const outgoing = createTemporaryObject(pageComponent, root);
        stack.push(outgoing);
        const incoming = createTemporaryObject(pageComponent, root);
        stack.replace(incoming);
        return [outgoing, incoming];
    }

    function cleanup() {
        stack.clear();
    }

    // Travelling forward, the new page comes in from the left and the old one leaves to
    // the right, each by the distance the caller gave.
    function test_forwardSwapTravelsTheCallersDistance() {
        const pages = swap(true);
        compare(pages[1].x, -root.distance, "the incoming page must start a full distance to the left");
        compare(pages[1].opacity, 0);
        compare(pages[0].x, 0, "the outgoing page must start where it sat");

        tryCompare(stack, "busy", false, 3000);
        compare(pages[1].x, 0, "the incoming page must land in place");
        compare(pages[1].opacity, 1);
        compare(pages[0].x, root.distance, "the outgoing page must leave to the right");
        compare(pages[0].opacity, 0);
    }

    // Travelling back, both pages reverse: in from the right, out to the left.
    function test_backwardSwapReversesBothPages() {
        const pages = swap(false);
        compare(pages[1].x, root.distance, "the incoming page must start a full distance to the right");

        tryCompare(stack, "busy", false, 3000);
        compare(pages[1].x, 0, "the incoming page must land in place");
        compare(pages[0].x, -root.distance, "the outgoing page must leave to the left");
    }

    // The distance is the caller's to give: ask for a slide the stack could not have
    // measured for itself and the pages must still travel exactly that far.
    function test_distanceIgnoresTheStacksOwnWidth() {
        stack.replaceEnter = make({ entering: true, forward: true, slideWidth: 37 });
        stack.replaceExit = make({ entering: false, forward: true, slideWidth: 37 });
        verify(stack.width !== 37, "the stack must not happen to measure the requested distance");

        const outgoing = createTemporaryObject(pageComponent, root);
        stack.push(outgoing);
        const incoming = createTemporaryObject(pageComponent, root);
        stack.replace(incoming);

        compare(incoming.x, -37, "the slide must travel the width the caller passed");
        tryCompare(stack, "busy", false, 3000);
        compare(outgoing.x, 37);
    }

    // Both halves of a swap quote the same duration, and the pages travel over it rather
    // than jumping. Which number that is stays a source-level guard: a wall-clock reading
    // cannot tell 350ms from 300ms without turning flaky.
    function test_bothHalvesSlideOverOneDuration() {
        const pages = swap(true);
        compare(stack.replaceEnter.slideDuration, stack.replaceExit.slideDuration);
        verify(stack.busy, "the swap must animate rather than land instantly");

        wait(stack.replaceEnter.slideDuration / 2);
        verify(pages[1].x > -root.distance && pages[1].x < 0,
               "the incoming page must still be travelling halfway through, was at " + pages[1].x);
        verify(pages[0].x > 0 && pages[0].x < root.distance,
               "the outgoing page must still be travelling halfway through, was at " + pages[0].x);

        tryCompare(stack, "busy", false, 3000);
    }
}
