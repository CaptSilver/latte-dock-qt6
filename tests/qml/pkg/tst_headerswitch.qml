// Coverage for HeaderSwitch.qml (org.kde.latte.components). The component is
// self-contained: it reads no unqualified Latte context names, only the global
// Qt.application and the imported Kirigami.Units, so no creation-context mock is
// needed. Its real logic is the level-dispatch in implicitHeight, the level-gated
// visibility of the Header / SubHeader / Label child, the level-1-only
// Layout.rightMargin/leftMargin, and the pressed() signal fired by the ghost
// buttons. We drive level/text/checked/tooltip and assert the observable effects.
import QtQuick
import QtQuick.Layouts
import QtTest

TestCase {
    id: root
    name: "HeaderSwitch"
    when: windowShown
    visible: true
    width: 300
    height: 80

    // The component sets Layout.rightMargin/leftMargin attached bindings; those only
    // resolve when the item is parented inside a real Layout, so create into this.
    ColumnLayout { id: host }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/components/HeaderSwitch.qml")

    function make(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, host, props || {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // The RowLayout (textElement) holds Header, SubHeader, Label in order. Reach
    // them through it so we can assert which one is visible per level.
    function textElement(obj) {
        // obj -> row (children[0]) -> textElement (children[0])
        return obj.children[0].children[0];
    }
    function headerChild(obj) { return textElement(obj).children[0]; }
    function subHeaderChild(obj) { return textElement(obj).children[1]; }
    function labelChild(obj) { return textElement(obj).children[2]; }

    // level 1 shows the Header; SubHeader and Label hidden.
    function test_level1_showsHeader() {
        const m = make({level: 1, text: "Top", checked: true});
        compare(headerChild(m).visible, true);
        compare(subHeaderChild(m).visible, false);
        compare(labelChild(m).visible, false);
        // text propagates to the visible Header.
        compare(headerChild(m).text, "Top");
    }

    // level 2 shows the SubHeader only.
    function test_level2_showsSubHeader() {
        const m = make({level: 2, text: "Mid", checked: true, isFirstSubCategory: true});
        compare(headerChild(m).visible, false);
        compare(subHeaderChild(m).visible, true);
        compare(labelChild(m).visible, false);
        compare(subHeaderChild(m).text, "Mid");
        compare(subHeaderChild(m).isFirstSubCategory, true);
    }

    // level > 2 shows the plain Label only.
    function test_level3_showsLabel() {
        const m = make({level: 3, text: "Leaf", checked: true});
        compare(headerChild(m).visible, false);
        compare(subHeaderChild(m).visible, false);
        compare(labelChild(m).visible, true);
        compare(labelChild(m).text, "Leaf");
    }

    // implicitHeight dispatches on level: each branch maxes its text child with the
    // switch. Changing level re-evaluates the binding to a different child's height.
    function test_implicitHeight_dispatchesOnLevel() {
        const m = make({level: 1, text: "X", checked: true});
        const h1 = m.implicitHeight;
        verify(h1 > 0, "level-1 implicitHeight should be positive");

        m.level = 2;
        const h2 = m.implicitHeight;
        verify(h2 > 0, "level-2 implicitHeight should be positive");

        m.level = 3;
        const h3 = m.implicitHeight;
        verify(h3 > 0, "level-3 implicitHeight should be positive");

        // Each branch is at least as tall as its own visible text child.
        verify(h1 >= headerChild(m).implicitHeight);
        verify(h2 >= subHeaderChild(m).implicitHeight);
        verify(h3 >= labelChild(m).implicitHeight);
    }

    // Layout.rightMargin / leftMargin are only non-zero at level 1 (LTR puts the gap
    // on the right). At other levels both collapse to 0.
    function test_layoutMargins_onlyAtLevel1() {
        const m = make({level: 1, text: "X"});
        // LTR default: rightMargin = 2*smallSpacing (>0), leftMargin = 0.
        verify(m.Layout.rightMargin > 0, "level-1 LTR rightMargin should be > 0");
        compare(m.Layout.leftMargin, 0);

        m.level = 2;
        compare(m.Layout.rightMargin, 0);
        compare(m.Layout.leftMargin, 0);
    }

    // checked drives the enabled state of the visible text child (item.checked && item.enabled).
    function test_checked_drivesChildEnabled() {
        const m = make({level: 1, text: "X", checked: false});
        compare(headerChild(m).enabled, false);
        m.checked = true;
        compare(headerChild(m).enabled, true);
    }

}
