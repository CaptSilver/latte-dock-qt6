// The default indicator's firstPoint grows into a line when its item becomes
// active under the Line style and shrinks back on deactivation. The transition
// is armed by onIsActiveStateForAnimationChanged, which raises inGrowAnimation /
// inShrinkAnimation; the width/height Behaviors then animate towards the state
// size. Assertions run synchronously right after each property flip — before the
// event loop lets the animation move — so the armed flags are still observable.
//
// The mock context mirrors tst_runningindicatorcolor.qml: IndicatorItem resolves
// `indicator` through parent.level.indicator.publicApi, so the host parent carries
// a level whose indicator exposes the indicator mock.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "DefaultIndicatorAnimation"
    when: windowShown
    visible: true
    width: 200
    height: 60

    QtObject {
        id: palette
        property color buttonFocusColor: "#27ae60"
        property color focusColor: "#3daee9"
        property color textColor: "#ffffff"
        property color negativeTextColor: "#da4453"
    }

    // The per-task indicator object the package reads through. Must be an Item:
    // LatteComponents.IndicatorItem.indicator is typed Item.
    Item {
        id: indicatorMock
        property bool isTask: true
        property bool isApplet: false
        property bool isEmptySpace: false
        property bool isLauncher: false
        property bool isWindow: true
        property bool isActive: false
        property bool isGroup: false
        property bool isHovered: false
        property bool isMinimized: false
        property bool isPressed: false
        property bool inAttention: false
        property bool inRemoving: false
        property bool hasActive: false
        property bool hasMinimized: false
        property bool hasShown: false
        property int windowsCount: 1
        property int windowsMinimizedCount: 0
        property real currentIconSize: 48
        property real maxIconSize: 48
        property int screenEdgeMargin: 0
        property real durationTime: 2
        property real scaleFactor: 1.0
        property color shadowColor: "black"
        property QtObject colorPalette: palette
        property QtObject configuration: QtObject {
            property bool enabledForApplets: true
            property real lengthPadding: 0.08
            property real backgroundCornerMargin: 1.0
            property real size: 0.13
            property real thickMargin: 0.0
            property bool reversed: false
            property bool extraDotOnActive: false
            property bool minimizedTaskColoredDifferently: false
            property int activeStyle: 0     // Line
            property bool glowEnabled: false
            property bool glow3D: false
            property int glowApplyTo: 0
            property real glowOpacity: 0.5
        }
    }

    Item { id: hostParent; property Item level: levelMock }
    Item { id: indicatorHolder; readonly property Item publicApi: indicatorMock }
    Item {
        id: levelMock
        property Item indicator: indicatorHolder
        property QtObject requested: QtObject {
            property int iconOffsetX: 0
            property int iconOffsetY: 0
            property int iconTransformOrigin: 0
            property real iconOpacity: 1.0
            property real iconRotation: 0
            property real iconScale: 1.0
            property bool isTaskLauncherAnimationRunning: false
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../indicators/default/package/ui/main.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, hostParent, {width: 100, height: 40});
        verify(obj, "instantiate failed");
        verify(obj.indicator === indicatorMock, "indicator did not resolve to the mock");
        return obj;
    }

    // firstPoint is the only descendant carrying the animation flags.
    function firstPoint(obj) {
        const stack = [obj];
        while (stack.length) {
            const o = stack.pop();
            if (!o)
                continue;
            if (o.inGrowAnimation !== undefined)
                return o;
            const kids = o.children;
            if (kids !== undefined && kids !== null) {
                for (var i = 0; i < kids.length; i++)
                    stack.push(kids[i]);
            }
        }
        return null;
    }

    function test_growShrinkStateMachine() {
        indicatorMock.isActive = false;
        indicatorMock.configuration.activeStyle = 0;   // Line

        const m = make();
        const fp = firstPoint(m);
        verify(fp, "firstPoint not found");
        compare(fp.inGrowAnimation, false);
        compare(fp.inShrinkAnimation, false);

        // false -> true under Line style: the grow animation arms.
        indicatorMock.isActive = true;
        compare(fp.inGrowAnimation, true, "activation must arm the grow animation");
        compare(fp.inShrinkAnimation, false);

        // true -> false under Line style: grow disarms, shrink arms.
        indicatorMock.isActive = false;
        compare(fp.inGrowAnimation, false, "deactivation must disarm grow");
        compare(fp.inShrinkAnimation, true, "deactivation must arm the shrink animation");

        // Leaving the Line style while active drops the animation state without
        // arming shrink — that is the handler's non-Line branch.
        indicatorMock.isActive = true;
        compare(fp.inGrowAnimation, true);
        indicatorMock.configuration.activeStyle = 1;   // Dot
        compare(fp.inGrowAnimation, false, "leaving Line style must clear grow");
        compare(fp.inShrinkAnimation, false, "leaving Line style must not arm shrink");
    }
}
