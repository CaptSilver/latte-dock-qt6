// FrontLayer of the plasma-style indicator draws two things: the growing-circle
// click animation (driven by the level's mousePressed signal) and the group
// expander arrow whose svg element is picked per dock edge by elementForLocation.
//
// FrontLayer is a plain Item loaded inside the indicator's main.qml, so all its
// context names (level, indicator, root.clickedAnimationEnabled) resolve from the
// creation context — mockable with lowercase ids here. The level mock declares the
// real LevelOptions signal signature (int x, int y, int button), so emitting it
// drives the Connections handler exactly like a real dock press. Headlessly
// Plasmoid.formFactor/location are unset, so the handler takes the non-edge
// branches and its one unconditional effect — clickedCenter's
// verticalCenterOffset — proves the body ran to its end.
import QtQuick
import QtTest
import org.kde.plasma.core as PlasmaCore

TestCase {
    id: root
    name: "PlasmaIndicatorFrontLayer"
    when: windowShown
    visible: true
    width: 200
    height: 60

    // In the real package "root" is the IndicatorItem; the Connections is gated
    // on root.clickedAnimationEnabled, so the TestCase stands in with it true.
    property bool clickedAnimationEnabled: true

    // QtObject on purpose: an Item mock would shadow indicator.resources with
    // Item's built-in resources list and break the groupSvg binding.
    QtObject {
        id: indicator
        property real scaleFactor: 1.0
        property real currentIconSize: 48
        property int screenEdgeMargin: 0
        property bool isApplet: false
        property bool isGroup: true
        property QtObject resources: QtObject { property var svgs: [] }
    }

    QtObject {
        id: level
        signal mousePressed(int x, int y, int button)
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../indicators/org.kde.latte.plasma/package/ui/FrontLayer.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // The SequentialAnimation is a non-visual resource; find it by its animation API.
    function findClickedAnimation(obj) {
        for (var i = 0; i < obj.data.length; ++i) {
            const o = obj.data[i];
            if (o && typeof o.complete === "function" && typeof o.start === "function") {
                return o;
            }
        }
        return null;
    }

    function test_onMousePressed_recentersAndStartsClickAnimation() {
        const obj = make();
        const relevantItem = obj.children[0];
        const clickedCenter = relevantItem.children[0];
        verify(clickedCenter.hasOwnProperty("center"), "clickedCenter not where expected");
        const clickedRectangle = relevantItem.children[1];

        compare(clickedCenter.anchors.verticalCenterOffset, 0);

        level.mousePressed(11, 37, Qt.LeftButton);

        // Without a formFactor the handler keeps fixedY = y and always writes the
        // vertical offset last, so this proves the whole body executed.
        compare(clickedCenter.anchors.verticalCenterOffset, 37);

        const anim = findClickedAnimation(obj);
        verify(anim, "clicked animation not found");
        verify(anim.running, "clickedAnimation.start() did not run");

        // Jump to the end values instead of waiting out the 700ms animation.
        anim.complete();
        const expectedWidth = Math.min(indicator.currentIconSize * 10,
                                       Math.max(relevantItem.width, relevantItem.height)) * 2;
        compare(clickedRectangle.width, expectedWidth);
        compare(clickedRectangle.opacity, 0);
    }

    function test_elementForLocation_mapsEdgesToExpanderElements() {
        const obj = make();
        const loader = obj.children[1];
        verify(loader, "arrow loader missing");
        verify(loader.item, "arrow loader did not load");

        var arrow = null;
        for (var i = 0; i < loader.item.children.length; ++i) {
            if (typeof loader.item.children[i].elementForLocation === "function") {
                arrow = loader.item.children[i];
            }
        }
        verify(arrow, "arrow item with elementForLocation not found");

        compare(arrow.elementForLocation(PlasmaCore.Types.LeftEdge), "group-expander-left");
        compare(arrow.elementForLocation(PlasmaCore.Types.TopEdge), "group-expander-top");
        compare(arrow.elementForLocation(PlasmaCore.Types.RightEdge), "group-expander-right");
        compare(arrow.elementForLocation(PlasmaCore.Types.BottomEdge), "group-expander-bottom");
        // Anything that is not an edge falls back to the bottom expander.
        compare(arrow.elementForLocation(PlasmaCore.Types.Floating), "group-expander-bottom");
    }
}
