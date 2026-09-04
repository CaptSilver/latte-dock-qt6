// Coverage for the basic item's IndicatorLevel from the staged (instrumented)
// package. The component is an anonymous AbilityItem.IndicatorLevel that reads
// the unqualified context name `abilityItem`, so -- as in tst_visibilitymanager_5
// -- this TestCase declares that name itself and the component resolves against
// it. The two size bindings are the subject: when the item is holding its initial
// position for a bounce/attention/new-window animation the indicator must keep
// the item's un-zoomed metrics, otherwise it follows the parabolic item. The mock
// numbers for the two arms are deliberately different so a binding that silently
// keeps its previous value cannot pass.
//
// There is no indicator plugin offscreen, so level.indicator stays null and the
// Loader's sourceComponent / Connections.enabled bindings log a caught
// "Cannot read property 'host' of null" on every instantiation. Harmless here:
// those bindings only decide whether to load an indicator, and the sizing
// bindings under test are independent of them.
import QtQuick
import QtTest
import Stage 1.0
import org.kde.plasma.core 2.0 as PlasmaCore

TestCase {
    id: root
    name: "IndicatorLevelBasicItem"
    when: windowShown
    visible: true
    width: 200
    height: 200

    QtObject {
        id: metricsObj
        property int iconSize: 36
        property QtObject totals: QtObject {
            property int thickness: 44
            property int lengthPaddings: 4
        }
        property QtObject margin: QtObject { property int length: 2 }
    }

    QtObject {
        id: parabolicItemObj
        property real length: 50
        property real thickness: 60
        property real zoom: 1
    }

    Item {
        id: abilityItem
        visible: false

        signal mousePressed(int x, int y, int button)
        signal mouseReleased(int x, int y, int button)
        signal taskLauncherActivated()
        signal taskGroupedWindowAdded()
        signal taskGroupedWindowRemoved()

        property int location: PlasmaCore.Types.BottomEdge
        property bool isHorizontal: true
        property bool isSeparator: false
        property bool isHidden: false
        property bool preserveIndicatorInInitialPosition: false
        property QtObject parabolicItem: parabolicItemObj
        property QtObject abilities: QtObject {
            property QtObject metrics: metricsObj
        }
    }

    readonly property url targetUrl: Stage.qmlModule("org/kde/latte/abilities/items/basicitem/IndicatorLevel.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    function reset() {
        abilityItem.preserveIndicatorInInitialPosition = false;
        abilityItem.isHorizontal = true;
        abilityItem.location = PlasmaCore.Types.BottomEdge;
    }

    // Following the parabolic item is the normal case: the indicator spans the
    // item's zoomed thickness and its length less the two side margins.
    function test_followsParabolicItemWhileNotPreserved() {
        reset();
        const obj = make();
        compare(obj.thickness, 60);            // parabolicItem.thickness
        compare(obj.length, 46);               // 50 - 2*1*2
        // horizontal dock: length runs along x, thickness across it
        compare(obj.width, 46);
        compare(obj.height, 60);
    }

    // Holding the initial position (launcher bounce, attention pulse, new-window
    // slide) must pin the indicator to the item's own un-zoomed metrics instead.
    // Both bindings have to flip; a read that resolves to undefined leaves its
    // property on the previous value and only one of them moves.
    function test_preservedIndicatorUsesUnzoomedMetrics() {
        reset();
        const obj = make();
        abilityItem.preserveIndicatorInInitialPosition = true;
        compare(obj.length, 40);               // iconSize + totals.lengthPaddings
        compare(obj.thickness, 44);            // totals.thickness
        compare(obj.width, 40);
        compare(obj.height, 44);
    }
}
