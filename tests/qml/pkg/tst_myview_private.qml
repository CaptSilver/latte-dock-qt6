// Coverage for the containment's MyViewPrivate ability. The component derives from
// AbilityHost.MyView and the coverage instrumentation injects Cov.tick into exactly
// two units: onIsHidingBlockedChanged@44 and decimalToHex@84. Those are the units
// this test claims.
//
// onIsHidingBlockedChanged fires when the inherited `isHidingBlocked` bool flips. The
// handler calls view.visibility.addBlockHidingEvent / removeBlockHidingEvent with the
// string `_myView + hidingBlockedStr`. `view` is a real property on the host
// (defaults null), so we hand it a mock whose visibility records the add/remove
// calls; flipping isHidingBlocked then asserts the observable side-effect.
//
// decimalToHex is a pure helper: toString(16) with left-zero padding to `padding`
// (default 2). We drive it across the padding-default, already-wide, and explicit-
// padding branches and assert the returned strings.
//
// The derived readonly props (itemShadowOpacity/itemShadowMaxSize/backgroundStoredOpacity/
// itemShadowCurrentColor) read Plasmoid.configuration + unqualified metrics/background.
// They are NOT instrumented and only evaluate when read, so we never touch them; the
// attached Plasmoid singleton has no live containment headlessly. The
// isHidingBlockedFromApplet Binding walks `layouts` (null by default); we hand it an
// empty layouts so its value block returns false cleanly without throwing.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "MyViewPrivate"
    when: windowShown

    // root.dragOverlay is read unqualified in isBindingUpdateEnabled. Declaring it
    // null keeps that binding's && short-circuit producing true.
    property var dragOverlay: null

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/privates/MyViewPrivate.qml")

    // Records the addBlockHidingEvent/removeBlockHidingEvent the handler makes.
    property int addCalls: 0
    property int removeCalls: 0
    property string lastAddArg: ""
    property string lastRemoveArg: ""

    // A mock `view` whose visibility records the handler's calls. Handed in at
    // construction so the onIsHidingBlockedChanged handler has a live target.
    Component {
        id: viewComp
        QtObject {
            property QtObject visibility: QtObject {
                function addBlockHidingEvent(s) { root.addCalls++; root.lastAddArg = "" + s; }
                function removeBlockHidingEvent(s) { root.removeCalls++; root.lastRemoveArg = "" + s; }
            }
        }
    }

    // Empty layouts so the isHidingBlockedFromApplet Binding's value block walks
    // three empty grids and returns false instead of dereferencing null.
    Component {
        id: layoutsComp
        Item {
            property Item startLayout: Item {}
            property Item mainLayout: Item {}
            property Item endLayout: Item {}
        }
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {
            view: viewComp.createObject(root),
            layouts: layoutsComp.createObject(root)
        });
        verify(obj, "instantiate failed");
        return obj;
    }

    // The empty-layouts Binding must settle isHidingBlockedFromApplet to false
    // (its value block walked three empty grids and found no blocking applet).
    function test_isHidingBlockedFromApplet_emptyLayouts() {
        const m = make();
        compare(m.isHidingBlockedFromApplet, false,
                "empty layouts -> no applet blocks hiding");
    }

    // onIsHidingBlockedChanged: flipping the inherited isHidingBlocked true calls
    // view.visibility.addBlockHidingEvent(_myView + hidingBlockedStr); flipping it
    // back calls removeBlockHidingEvent with the same string.
    function test_onIsHidingBlockedChanged_addRemove() {
        const m = make();
        compare(m.isHidingBlocked, false, "base default is not blocked");
        const addBase = root.addCalls;
        const removeBase = root.removeCalls;

        m.isHidingBlocked = true;
        compare(root.addCalls, addBase + 1, "blocking should add a hiding event");
        // The arg is `_myView + hidingBlockedStr`; it must end with the suffix.
        verify(root.lastAddArg.indexOf(m.hidingBlockedStr) >= 0,
               "add arg should carry the hidingBlockedStr suffix");

        m.isHidingBlocked = false;
        compare(root.removeCalls, removeBase + 1, "unblocking should remove the event");
        verify(root.lastRemoveArg.indexOf(m.hidingBlockedStr) >= 0,
               "remove arg should carry the hidingBlockedStr suffix");
        // Both calls must reference the same event string.
        compare(root.lastRemoveArg, root.lastAddArg,
                "add and remove must use the same event id");
    }

    // decimalToHex padding-default branch: padding undefined -> 2. 255 -> "ff"
    // (already 2 wide, no zero-fill); 5 -> "05" (zero-filled to 2).
    function test_decimalToHex_defaultPadding() {
        const m = make();
        compare(m.decimalToHex(255), "ff");
        compare(m.decimalToHex(5), "05");
        compare(m.decimalToHex(0), "00");
    }

    // decimalToHex explicit padding: wider than the hex digits zero-fills to that
    // width; narrower than the digits leaves them untouched (while loop skipped).
    function test_decimalToHex_explicitPadding() {
        const m = make();
        compare(m.decimalToHex(255, 4), "00ff", "pad to 4");
        compare(m.decimalToHex(255, 1), "ff", "padding 1 < 2 digits leaves it");
        compare(m.decimalToHex(16, 2), "10", "16 -> 10 hex, already 2 wide");
    }
}
