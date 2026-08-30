// Coverage for the settings dialog's DragCorner resize handle: a rotated
// Rectangle whose MouseArea rescales the dialog while dragging and resets the
// scale on double click. Everything it touches is an unqualified context name
// (plasmoid, dialog, viewConfig, universalSettings, latteView), declared here
// as document ids shaped like the real objects. MouseArea responds to
// synthetic mouse events offscreen, so press/release and the double-click
// sequence drive the real handlers.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "DragCorner"
    when: windowShown
    // TestCase defaults to visible:false and the window only delivers mouse
    // events to visible items.
    visible: true
    width: 300
    height: 300

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/controls/DragCorner.qml")

    // location 0 == Floating: none of the edge states matches, so the corner
    // keeps its default anchors and the LeftEdge special-casing stays off.
    QtObject {
        id: plasmoid
        property int location: 0
        property int formFactor: 0
    }

    QtObject {
        id: viewConfig
        property int x: 0
        property int y: 0
        property int syncCalls: 0
        function syncGeometry() { syncCalls++; }
    }

    QtObject {
        id: dialog
        property bool advancedLevel: true
        property int width: 400
        property int height: 300
        property real userScaleWidth: 1.4
        property real userScaleHeight: 1.2
    }

    QtObject {
        id: universalSettings
        property var scaleCalls: []
        function setScreenScales(screenName, scaleW, scaleH) {
            var calls = scaleCalls;
            calls.push({ screen: screenName, scaleW: scaleW, scaleH: scaleH });
            scaleCalls = calls;
        }
    }

    QtObject {
        id: latteView
        property QtObject positioner: QtObject {
            property string currentScreenName: "MOCK-SCREEN"
        }
    }

    // The corner anchors its center to the parent's top-right corner, so the
    // host sits deep enough in the window that the whole 22x22 rect (even
    // rotated 45 degrees) stays inside and synthetic events can reach it.
    Item {
        id: host
        x: 100
        y: 100
        width: 120
        height: 120
    }

    function init() {
        dialog.userScaleWidth = 1.4;
        dialog.userScaleHeight = 1.2;
        universalSettings.scaleCalls = [];
        viewConfig.syncCalls = 0;
    }

    function makeCorner() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, host, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // The MouseArea holding the drag state is an internal child (no alias);
    // find it by its `initialized` drag flag.
    function mouseAreaOf(corner) {
        for (var i = 0; i < corner.children.length; ++i) {
            if (corner.children[i].hasOwnProperty("initialized")) {
                return corner.children[i];
            }
        }
        return null;
    }

    // onPressed primes the drag state (initialized = true), onReleased clears
    // it — the release handler's whole job.
    function test_release_clearsDragState() {
        const corner = makeCorner();
        const area = mouseAreaOf(corner);
        verify(area, "drag-state MouseArea not found");
        compare(area.initialized, false);

        mousePress(corner, corner.width / 2, corner.height / 2);
        compare(area.initialized, true, "press must prime the drag state");

        mouseRelease(corner, corner.width / 2, corner.height / 2);
        compare(area.initialized, false, "release must clear the drag state");
    }

    // Double click resets both user scales to 1, persists them for the current
    // screen and re-syncs the dialog geometry.
    function test_doubleClick_resetsScales() {
        const corner = makeCorner();

        mouseDoubleClickSequence(corner, corner.width / 2, corner.height / 2, Qt.LeftButton);

        compare(dialog.userScaleWidth, 1, "double click must reset the width scale");
        compare(dialog.userScaleHeight, 1, "double click must reset the height scale");

        verify(universalSettings.scaleCalls.length >= 1, "scales must be persisted");
        const last = universalSettings.scaleCalls[universalSettings.scaleCalls.length - 1];
        compare(last.screen, "MOCK-SCREEN");
        compare(last.scaleW, 1);
        compare(last.scaleH, 1);

        verify(viewConfig.syncCalls >= 1, "geometry must be re-synced");
    }
}
