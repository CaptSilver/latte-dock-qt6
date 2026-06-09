// Coverage for the Latte client indicator LatteIndicator.qml. It subclasses
// LatteComponents.IndicatorItem, whose `indicator` is a readonly Item resolved
// from `parent.level.bridge`. So the creation context the component reads is the
// `indicator` object: every binding/handler dereferences indicator.<x>. We give
// it a real Item-typed indicator mock by parenting the component under an Item
// whose `level.bridge` is that mock; with that wired, indicator resolves and the
// firstPoint GlowPoint's handlers run against concrete values.
//
// firstPoint lives at obj.children[0] (mainIndicatorElement) -> .children[0]
// (flowItem) -> .children[0]. Its NumberAnimation (activeAndReverseAnimation) is
// the only resource carrying both `easing` and `duration`, so we find it that way
// and zero its duration to make the line-style active path settle synchronously.
//
// Headless limits, routed live-only (not gamed): onStateHeightChanged and the
// vertical branch of onScaleFactorChanged require firstPoint.vertical, which is
// Plasmoid.formFactor===Vertical — undefined without a live containment, so it is
// permanently false here and those handler bodies do no observable work. The
// Component.onDestruction handler only disconnects a signal at teardown, with no
// effect a headless assertion can observe.
import QtQuick
import QtTest
import org.kde.latte.core 0.2 as LatteCore

TestCase {
    id: root
    name: "LatteIndicator"
    when: windowShown
    visible: true
    width: 200
    height: 60

    // The indicator object the component reads through. Must be an Item because
    // IndicatorItem.indicator is typed `Item`; a QtObject is silently dropped.
    Item {
        id: indicatorMock
        property bool isMinimized: false
        property bool isActive: false
        property bool isGroup: false
        property bool isTask: true
        property bool isApplet: false
        property bool isLauncher: false
        property bool isEmptySpace: false
        property bool inRemoving: false
        property bool inAttention: false
        property bool hasActive: false
        property bool hasShown: false
        property bool hasMinimized: false
        property real currentIconSize: 48
        property real maxIconSize: 48
        property int screenEdgeMargin: 5
        property real durationTime: 2
        property real scaleFactor: 1.0
        property color shadowColor: "black"
        property QtObject configuration: QtObject {
            property bool enabledForApplets: true
            property real lengthPadding: 0.1
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

    // parent.level.bridge is how IndicatorItem discovers the indicator. Both
    // `level` and `bridge` are Item-typed in the base, so the mocks are Items.
    Item { id: hostParent; property Item level: levelMock }
    Item { id: levelMock; property Item bridge: indicatorMock }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/lib64/qt6/qml/org/kde/latte/abilities/client/indicators/LatteIndicator.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, hostParent, {width: 100, height: 40});
        verify(obj, "instantiate failed");
        verify(obj.indicator === indicatorMock, "indicator did not resolve to the mock");
        return obj;
    }

    // mainIndicatorElement -> flowItem -> firstPoint (the active-window GlowPoint).
    function firstPoint(obj) {
        return obj.children[0].children[0].children[0];
    }

    // activeAndReverseAnimation is the lone NumberAnimation in firstPoint's
    // resources; it is the only one with both an easing curve and a duration.
    function reverseAnim(p) {
        const res = p.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].hasOwnProperty("easing") && res[i].hasOwnProperty("duration"))
                return res[i];
        }
        return null;
    }

    function resetMock() {
        indicatorMock.hasActive = false;
        indicatorMock.isActive = false;
        indicatorMock.scaleFactor = 1.0;
        indicatorMock.currentIconSize = 48;
        indicatorMock.configuration.activeStyle = 0;
    }

    // colorBrightnessFromRGB is the W3C luminance formula; assert exact outputs.
    function test_colorBrightnessFromRGB() {
        resetMock();
        const m = make();
        // (255*299 + 255*587 + 255*114) / 1000 == 255
        compare(m.colorBrightnessFromRGB(255, 255, 255), 255);
        // pure red channel: 255*299/1000
        compare(m.colorBrightnessFromRGB(255, 0, 0), 76.245);
        compare(m.colorBrightnessFromRGB(0, 0, 0), 0);
    }

    // colorBrightness scales a QColor's 0..1 channels to 0..255 then calls
    // colorBrightnessFromRGB; white -> 255, black -> 0.
    function test_colorBrightness() {
        resetMock();
        const m = make();
        compare(m.colorBrightness(Qt.rgba(1, 1, 1, 1)), 255);
        compare(m.colorBrightness(Qt.rgba(0, 0, 0, 1)), 0);
        // green channel weight 587: 1.0 green -> 255*587/1000
        compare(m.colorBrightness(Qt.rgba(0, 1, 0, 1)), 149.685);
    }

    // updateInitialSizes sizes firstPoint from root.size when inactive, and to
    // stateWidth when active+line+horizontal. Drive both branches directly.
    function test_updateInitialSizes() {
        resetMock();
        const m = make();
        const p = firstPoint(m);
        const anim = reverseAnim(p);
        verify(anim, "activeAndReverseAnimation not found");
        anim.stop();

        // inactive: width and height collapse to root.size (= 0.08*48 -> 3).
        p.width = 999;
        p.height = 999;
        p.updateInitialSizes();
        compare(p.width, m.size);
        compare(p.height, m.size);

        // active + line + horizontal: width tracks stateWidth (root.width=100).
        indicatorMock.hasActive = true;       // isActive becomes true
        compare(p.isActive, true);
        anim.stop();                           // suppress the onIsActiveChanged animation
        p.width = 999;
        p.updateInitialSizes();
        compare(p.width, p.stateWidth);
        compare(p.stateWidth, 100);
    }

    // onIsActiveChanged starts activeAndReverseAnimation when activeStyle is Line.
    // Toggle the bound source (indicator.hasActive); assert the animation runs.
    function test_onIsActiveChanged_startsAnimation() {
        resetMock();
        const m = make();
        const p = firstPoint(m);
        const anim = reverseAnim(p);
        verify(anim, "activeAndReverseAnimation not found");
        anim.stop();
        compare(p.isActive, false);

        indicatorMock.hasActive = true;        // isActive false -> true fires the handler
        compare(p.isActive, true);
        verify(anim.running, "onIsActiveChanged did not start the reverse animation");
        anim.stop();
    }

    // onScaleFactorChanged (horizontal, active, not animating) sets width to
    // stateWidth. Pin the animation off, flip active, then change scaleFactor.
    function test_onScaleFactorChanged_setsWidth() {
        resetMock();
        const m = make();
        const p = firstPoint(m);
        const anim = reverseAnim(p);
        verify(anim, "activeAndReverseAnimation not found");

        indicatorMock.hasActive = true;        // active + line
        anim.stop();                           // not animating -> direct-set branch
        compare(p.isActive, true);

        p.width = 7;
        indicatorMock.scaleFactor = 2.0;       // fires onScaleFactorChanged
        compare(p.width, p.stateWidth);
        compare(p.width, 100);
    }

    // onStateWidthChanged (horizontal, active, not animating) re-applies stateWidth
    // to width. Bumping root.width recomputes stateWidth and fires the handler.
    function test_onStateWidthChanged_setsWidth() {
        resetMock();
        const m = make();
        const p = firstPoint(m);
        const anim = reverseAnim(p);
        verify(anim, "activeAndReverseAnimation not found");

        indicatorMock.hasActive = true;
        anim.stop();
        compare(p.isActive, true);

        p.width = 7;
        m.width = 150;                         // stateWidth = root.width - spacer.width
        compare(p.stateWidth, 150);
        compare(p.width, 150);
    }

    // Component.onCompleted connects indicator.currentIconSizeChanged to
    // updateInitialSizes. Assert that connection: bumping the icon size both
    // changes root.size and re-runs updateInitialSizes, resizing firstPoint.
    function test_onCompleted_connectsIconSize() {
        resetMock();
        const m = make();
        const p = firstPoint(m);
        const anim = reverseAnim(p);
        anim.stop();

        // inactive sizing: firstPoint width follows root.size = 0.08*currentIconSize.
        p.width = 999;
        indicatorMock.currentIconSize = 100;   // size -> 8, fires currentIconSizeChanged
        compare(m.size, 8);
        // the connected updateInitialSizes re-ran and pulled width back to size.
        compare(p.width, 8);
    }
}
