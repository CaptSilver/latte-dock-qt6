// The plasma-style indicator (org.kde.latte.plasma) picks its tasks-svg prefixes
// from the dock edge: taskPrefix("hover") returns ["south-hover", "hover"] on a
// bottom dock so KSvg can fall back from the edge-specific element to the plain
// one. taskPrefixHovered additionally expands "<prefix>-hover"/"hover" variants
// in fallback order. Headlessly there is no live applet, Plasmoid.location is
// unset and no edge branch fires, so the edge slot stays undefined while the
// plain-prefix fallback and the hover expansion order remain fully assertable.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "PlasmaIndicator"
    when: windowShown
    visible: true
    width: 200
    height: 60

    // The per-task indicator object; typed Item because IndicatorItem.indicator
    // and level.bridge are Item properties.
    Item {
        id: indicatorMock
        property bool isTask: true
        property bool isApplet: false
        property bool isEmptySpace: false
        property bool isGroup: false
        property bool progressVisible: false
        property real progress: 0
        property int screenEdgeMargin: 0
        property real currentIconSize: 48
        property real maxIconSize: 48
        property real scaleFactor: 1.0
        property QtObject configuration: QtObject {
            property real lengthPadding: 0.08
            property real backgroundCornerMargin: 1.0
            // false keeps providesClickedAnimation off so the FrontLayer loader
            // stays inactive; this test only targets the root's prefix helpers.
            property bool clickedAnimationEnabled: false
        }
    }

    // IndicatorItem resolves `indicator` from parent.level.bridge; onLevelChanged
    // writes level.requested.*, so the level mock provides a requested object.
    // isBackground/isForeground false keep every layer Loader unloaded.
    Item { id: hostParent; property Item level: levelMock }
    Item {
        id: levelMock
        property Item bridge: indicatorMock
        property bool isBackground: false
        property bool isForeground: false
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

    readonly property url targetUrl: Qt.resolvedUrl("../../indicators/org.kde.latte.plasma/package/ui/main.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, hostParent, {width: 100, height: 40});
        verify(obj, "instantiate failed");
        verify(obj.indicator === indicatorMock, "indicator did not resolve to the mock");
        return obj;
    }

    function test_taskPrefix_keepsPlainPrefixAsFallback() {
        const m = make();
        const res = m.taskPrefix("hover");
        compare(res.length, 2);
        compare(res[1], "hover");
        // No live applet means no dock edge, so the edge-specific slot is empty.
        verify(res[0] === undefined, "unexpected edge prefix without a Plasmoid location: " + res[0]);
    }

    function test_taskPrefixHovered_expandsHoverVariantsInFallbackOrder() {
        const m = make();
        const res = m.taskPrefixHovered("progress");
        // [prefix-hover pair, plain hover pair, plain prefix pair]
        compare(res.length, 6);
        compare(res[1], "progress-hover");
        compare(res[3], "hover");
        compare(res[5], "progress");
    }

    function test_taskPrefixHovered_emptyPrefixSkipsHoverExpansion() {
        const m = make();
        const res = m.taskPrefixHovered("");
        compare(res.length, 2);
        compare(res[1], "");
    }
}
