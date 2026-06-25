// The running/active indicator dot for a task draws in indicator.colorPalette
// .buttonFocusColor (the default indicator package, main.qml). On a dock without
// custom colorization, abilities MyView hands the indicator a raw Kirigami.Theme,
// which has no buttonFocusColor — so the binding evaluated to undefined and the
// dot painted opaque black: invisible on a dark panel. The fix mirrors the
// colorizer Manager (Manager.qml: buttonFocusColor -> Kirigami.Theme.focusColor):
// fall back to the palette's focusColor when buttonFocusColor is absent.
//
// This loads the REAL default indicator package and reads its isActiveColor under
// two palettes: a Kirigami.Theme-shaped one (no buttonFocusColor) and a Plasma-
// shaped one (with buttonFocusColor). Both must yield a visible color, and each
// must prefer the right source.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "RunningIndicatorColor"
    when: windowShown
    visible: true
    width: 200
    height: 60

    // Kirigami.Theme-shaped palette: has focusColor, no buttonFocusColor. This is
    // what abilities MyView falls back to when colorization is off (the default).
    QtObject {
        id: kirigamiLikePalette
        property color focusColor: "#3daee9"
        property color textColor: "#ffffff"
        property color negativeTextColor: "#da4453"
    }

    // Plasma-shaped palette (the Latte colorizer): exposes buttonFocusColor, which
    // must win over focusColor when present.
    QtObject {
        id: plasmaLikePalette
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
        property QtObject colorPalette: kirigamiLikePalette
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

    // IndicatorItem resolves `indicator` from parent.level.bridge; onLevelChanged
    // writes level.requested.*, so the level mock provides a requested object.
    Item { id: hostParent; property Item level: levelMock }
    Item {
        id: levelMock
        property Item bridge: indicatorMock
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

    function isBlack(c) {
        // undefined -> color resolves to (0,0,0); also treat fully transparent as invisible.
        return (c.r === 0 && c.g === 0 && c.b === 0) || c.a === 0;
    }

    // The regression: with a palette that lacks buttonFocusColor, the dot must
    // still resolve to a visible color (the palette's focusColor), not black.
    function test_visibleColor_whenPaletteHasNoButtonFocusColor() {
        indicatorMock.colorPalette = kirigamiLikePalette;
        const m = make();
        verify(!isBlack(m.isActiveColor),
               "isActiveColor is black/transparent with a Kirigami.Theme-style palette");
        compare(m.isActiveColor, kirigamiLikePalette.focusColor);
    }

    // When the palette DOES provide buttonFocusColor (the Latte colorizer), it wins.
    function test_prefersButtonFocusColor_whenPaletteProvidesIt() {
        indicatorMock.colorPalette = plasmaLikePalette;
        const m = make();
        compare(m.isActiveColor, plasmaLikePalette.buttonFocusColor);
    }
}
