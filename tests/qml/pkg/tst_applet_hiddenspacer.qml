// Coverage for the containment's applet HiddenSpacer. It's an anonymous Item
// that reads a pile of unqualified context names (root, wrapper, appletItem,
// communicator, isSeparator) and computes nHiddenSize / width / height plus a
// separator-neighbour space. QML resolves the unqualified names against the
// creation context, so the TestCase is named `root` and every name the
// component touches is declared here as a property / lowercase-id'd QtObject.
//
// directRenderingEnabled is kept true throughout so the nHiddenSize length
// Behavior is the zero-duration directBehavior -- writes land instantly and the
// assertions don't race an animation.
import QtQuick
import QtQuick.Window
import QtTest
import org.kde.latte.core 0.2 as LatteCore

TestCase {
    id: root
    name: "AppletHiddenSpacer"
    when: windowShown

    // root.isHorizontal / root.isVertical select width-vs-height and the debug
    // rectangle orientation.
    property bool isHorizontal: true
    property bool isVertical: false

    // wrapper supplies the cross-axis size the spacer mirrors.
    QtObject {
        id: wrapper
        property real width: 64
        property real height: 48
    }

    // isSeparator short-circuits nHiddenSize to 0.
    property bool isSeparator: false

    // The animatedBehavior.enabled binding reads restoreAnimation.running. The
    // real spacer has a restoreAnimation sibling; without it the binding throws
    // and the nHiddenSize Behavior never settles. directRenderingEnabled true
    // keeps animatedBehavior disabled regardless, so running stays false.
    QtObject {
        id: restoreAnimation
        property bool running: false
    }

    // communicator.requires.lengthMarginsEnabled gates nHiddenSize.
    QtObject {
        id: communicator
        property QtObject requires: QtObject { property bool lengthMarginsEnabled: true }
    }

    // The neighbour applet. parabolic.isEnabled + the head/tail separator flags
    // drive separatorSpace; spacersMaxSize * nScale is the zoomed length;
    // directRenderingEnabled true keeps writes instant; debug.spacersEnabled
    // off keeps the debug Loader inactive; containsMouse drives the slot.
    QtObject {
        id: appletItem
        property bool firstAppletInContainer: false
        property bool lastAppletInContainer: false
        property bool headAppletIsSeparator: false
        property bool tailAppletIsSeparator: false
        property real spacersMaxSize: 100
        property int animationTime: 1
        property bool containsMouse: true
        property QtObject parabolic: QtObject {
            property bool isEnabled: false
            property bool directRenderingEnabled: true
        }
        property QtObject debug: QtObject { property bool spacersEnabled: false }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/applet/HiddenSpacer.qml")

    function make(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, props || {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // A shown Window host so the spacer's *effective* `visible` resolves up a
    // real visible ancestor chain instead of being forced false by the
    // offscreen, invisible TestCase. Without this, m.visible always reads false
    // no matter what the component's own binding evaluates to.
    Component {
        id: windowHostComponent
        Window {
            width: 200; height: 200; visible: true
            property Item content: Item { anchors.fill: parent }
        }
    }

    function makeVisible(props) {
        const win = createTemporaryObject(windowHostComponent, root, {});
        verify(win, "window host instantiate failed");
        tryVerify(function() { return win.visible; }, 2000, "host window never shown");
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, win.content, props || {});
        verify(obj, "instantiate failed");
        return obj;
    }

    function init() {
        // Reset shared mock state each test so order doesn't leak.
        appletItem.parabolic.isEnabled = false;
        appletItem.headAppletIsSeparator = false;
        appletItem.tailAppletIsSeparator = false;
        appletItem.containsMouse = true;
        root.isHorizontal = true;
        root.isVertical = false;
        root.isSeparator = false;
        communicator.requires.lengthMarginsEnabled = true;
    }

    // nHiddenSize early-returns 0 when isSeparator -- regardless of nScale.
    function test_nHiddenSize_separator_isZero() {
        const m = make({nScale: 0.5});
        root.isSeparator = true;
        // re-trigger the binding by nudging an input it depends on
        compare(m.nHiddenSize, 0);
    }

    // nHiddenSize early-returns 0 when length margins are disabled.
    function test_nHiddenSize_lengthMarginsDisabled_isZero() {
        const m = make({nScale: 0.5});
        communicator.requires.lengthMarginsEnabled = false;
        compare(m.nHiddenSize, 0);
    }

    // nScale<=0 with no neighbour separator -> separatorSpace 0 -> nHiddenSize 0.
    function test_nHiddenSize_zeroScale_noSeparator_isZero() {
        const m = make({nScale: 0});
        compare(m.separatorSpace, 0);
        compare(m.nHiddenSize, 0);
    }

    // nScale>0: nHiddenSize = spacersMaxSize * nScale + separatorSpace(0 here).
    function test_nHiddenSize_positiveScale_scalesBySpacersMax() {
        const m = make({nScale: 0.25});
        compare(m.separatorSpace, 0);
        compare(m.nHiddenSize, 25);   // 100 * 0.25 + 0
    }

    // separatorSpace: a left spacer reads tailAppletIsSeparator. With parabolic
    // enabled it's separatorLength/2 = 5/2 = 2 (int property).
    function test_separatorSpace_leftSpacer_tailSeparator() {
        appletItem.tailAppletIsSeparator = true;
        appletItem.parabolic.isEnabled = true;
        const m = make({isRightSpacer: false, nScale: 0});
        verify(m.hasNeighbourSeparator);
        compare(m.separatorSpace, 2);
        // nScale<=0 path returns separatorSpace itself.
        compare(m.nHiddenSize, 2);
    }

    // A right spacer reads headAppletIsSeparator instead. tailAppletIsSeparator
    // being set must NOT make a right spacer think it has a neighbour.
    function test_hasNeighbourSeparator_rightSpacer_readsHead() {
        appletItem.tailAppletIsSeparator = true;   // wrong side for a right spacer
        appletItem.headAppletIsSeparator = false;
        appletItem.parabolic.isEnabled = true;
        const m = make({isRightSpacer: true, nScale: 0});
        verify(!m.hasNeighbourSeparator);
        compare(m.separatorSpace, 0);
    }

    // separatorSpace stays 0 when a neighbour separator exists but parabolic is
    // disabled -- the &&-guard's second operand.
    function test_separatorSpace_parabolicDisabled_isZero() {
        appletItem.tailAppletIsSeparator = true;
        appletItem.parabolic.isEnabled = false;
        const m = make({isRightSpacer: false, nScale: 0});
        verify(m.hasNeighbourSeparator);
        compare(m.separatorSpace, 0);
    }

    // positive scale AND a neighbour separator add together.
    function test_nHiddenSize_scalePlusSeparatorSpace() {
        appletItem.tailAppletIsSeparator = true;
        appletItem.parabolic.isEnabled = true;
        const m = make({isRightSpacer: false, nScale: 0.5});
        compare(m.separatorSpace, 2);
        compare(m.nHiddenSize, 52);   // 100 * 0.5 + 2
    }

    // Horizontal: width follows nHiddenSize, height mirrors wrapper.height.
    function test_dimensions_horizontal() {
        root.isHorizontal = true;
        const m = make({nScale: 0.1});
        compare(m.nHiddenSize, 10);
        compare(m.width, 10);
        compare(m.height, wrapper.height);   // 48
    }

    // Vertical (isHorizontal false): the axes swap.
    function test_dimensions_vertical() {
        root.isHorizontal = false;
        const m = make({nScale: 0.1});
        compare(m.nHiddenSize, 10);
        compare(m.width, wrapper.width);     // 64
        compare(m.height, 10);
    }

    // visible: a left spacer is visible when it's the first applet in the
    // container, even with no separator space.
    function test_visible_leftSpacer_firstApplet() {
        appletItem.firstAppletInContainer = true;
        appletItem.lastAppletInContainer = false;
        const m = makeVisible({isRightSpacer: false, nScale: 0});
        tryVerify(function() { return m.visible; }, 2000);
        appletItem.firstAppletInContainer = false;
        tryVerify(function() { return !m.visible; }, 2000);
    }

    // visible: a non-edge spacer with no separator space is hidden, but turns
    // visible once separatorSpace>0.
    function test_visible_separatorSpaceForcesVisible() {
        appletItem.firstAppletInContainer = false;
        appletItem.lastAppletInContainer = false;
        appletItem.tailAppletIsSeparator = true;
        appletItem.parabolic.isEnabled = true;
        const m = makeVisible({isRightSpacer: false, nScale: 0});
        compare(m.separatorSpace, 2);
        tryVerify(function() { return m.visible; }, 2000);
    }

    // The appletItem.onContainsMouseChanged slot zeroes nScale when the mouse
    // leaves. Seed a non-zero scale, flip containsMouse false, watch it drop.
    function test_containsMouseChanged_clearsScaleOnExit() {
        const m = make({nScale: 0.4});
        compare(m.nScale, 0.4);
        appletItem.containsMouse = false;   // fires onContainsMouseChanged
        compare(m.nScale, 0);
        compare(m.nHiddenSize, 0);
    }

    // The slot only clears on exit: re-entering (containsMouse true) must not
    // touch a scale the test set afterwards.
    function test_containsMouseChanged_enterDoesNotClear() {
        appletItem.containsMouse = false;
        const m = make({nScale: 0});
        appletItem.containsMouse = true;    // fires, but containsMouse is true -> no clear
        m.nScale = 0.7;
        compare(m.nScale, 0.7);             // untouched
    }
}
