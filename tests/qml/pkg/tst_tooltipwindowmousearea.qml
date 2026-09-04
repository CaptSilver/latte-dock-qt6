// Drives the previews ToolTipWindowMouseArea (a hover-tracking MouseArea)
// through its onContainsMouseChanged handler with synthetic mouse moves.
//
// Unqualified creation-context names: only `root.windowsHovered` is reached by
// the hover handler; it is declared here as a recording sink. The area's
// `enabled` binding needs a non-zero winId offscreen (the platform is not
// wayland, so the X11-style `winId != 0` leg gates it) — pass winId at
// creation.
//
// onClicked is an arrow-function handler the instrumenter does not tick, and
// its body needs tasksModel/windowsPreviewDlg from a live previews dialog; it
// earns no unit here so it is not driven.
import QtQuick
import QtTest
import Stage 1.0

TestCase {
    id: root
    name: "ToolTipWindowMouseArea"
    when: windowShown
    visible: true
    width: 200
    height: 200

    readonly property url targetUrl: Stage.share("plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/previews/ToolTipWindowMouseArea.qml")

    // The handler's only effect: root.windowsHovered([winId], containsMouse).
    property var hoveredCalls: []
    function windowsHovered(ids, hovered) { hoveredCalls.push([ids, hovered]); }

    function test_containsMouseForwardsWindowsHovered() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const area = createTemporaryObject(c, root, { winId: 42, width: 100, height: 100 });
        verify(area, "instantiate failed");
        verify(area.enabled, "area must be enabled for hover tracking");
        root.hoveredCalls = [];

        // Enter: hover onto the area -> containsMouse true -> forward [42], true.
        mouseMove(area, 50, 50);
        tryVerify(function() { return area.containsMouse; }, 2000);
        compare(hoveredCalls.length, 1);
        compare(hoveredCalls[0][0].length, 1);
        compare(hoveredCalls[0][0][0], 42);
        compare(hoveredCalls[0][1], true);

        // Leave: hover off to a corner of the TestCase outside the 100x100 area.
        mouseMove(root, 190, 190);
        tryVerify(function() { return !area.containsMouse; }, 2000);
        compare(hoveredCalls.length, 2);
        compare(hoveredCalls[1][0][0], 42);
        compare(hoveredCalls[1][1], false);
    }
}
