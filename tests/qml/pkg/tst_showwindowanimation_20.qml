// Coverage for the plasmoid's ShowWindowAnimation, driven from the staged
// (instrumented) package. ShowWindowAnimation is a SequentialAnimation that
// reads a pile of unqualified containment context names (root / taskItem /
// tasksModel / tasksExtendedManager / activityInfo / icList / index / isWindow
// / isLauncher / isForcedHidden / publishGeometryTimer) plus
// LatteCore.WindowSystem. QML resolves the unqualified names against the
// component's creation context, so we name the TestCase `id: root` and declare
// each one here as a property / lowercase-id'd QtObject shaped like the real
// object. Every test calls a unit and asserts an observable side effect on
// these mocks (a property write, a mock function call count, an animation
// start), so the coverage is honest.
import QtQuick
import QtTest
import org.kde.latte.core 0.2 as LatteCore

TestCase {
    id: root
    name: "ShowWindowAnimation"
    when: windowShown
    visible: true
    width: 200
    height: 200

    // ---- unqualified context the component reads as `root.*` ----
    property bool newWindowSlidingEnabled: true
    property bool vertical: false
    property bool inActivityChange: false
    property bool showWindowsOnlyFromLaunchers: false
    property bool disableAllWindowsFunctionality: false
    property bool inDraggingPhase: false

    // execute() writes the bare name `isForcedHidden` and reads the bare names
    // `isWindow` / `isLauncher` / `index`; declare them as writable root props.
    property bool isForcedHidden: false
    property bool isWindow: false
    property bool isLauncher: false
    property int index: 0

    // icList.childAtIndex(i) — return undefined (what the real ListView helper
    // gives for an out-of-range index) so the reorder block's
    // `!== undefined` guard short-circuits before dereferencing a neighbour.
    QtObject {
        id: icList
        function childAtIndex(i) { return undefined; }
    }

    // tasksModel.launcherPosition / launcherActivities.
    property int _launcherPositionReturn: -1
    property var _launcherActivitiesReturn: []
    QtObject {
        id: tasksModel
        function launcherPosition(url) { return root._launcherPositionReturn; }
        function launcherActivities(url) { return root._launcherActivitiesReturn; }
    }

    // activityInfo current/previous activity ids.
    QtObject {
        id: activityInfo
        property string currentActivity: "act-current"
        property string previousActivity: "act-previous"
    }

    // tasksExtendedManager — count the mutating calls so tests can assert them.
    property int _removeToBeAddedCalls: 0
    property int _removeImmediateCalls: 0
    property int _removeFrozenCalls: 0
    property bool _toBeAddedExists: false
    property bool _immediateExists: false
    property var _frozenTaskReturn: null
    QtObject {
        id: tasksExtendedManager
        function toBeAddedLauncherExists(url) { return root._toBeAddedExists; }
        function removeToBeAddedLauncher(url) { root._removeToBeAddedCalls++; }
        function immediateLauncherExists(url) { return root._immediateExists; }
        function removeImmediateLauncher(url) { root._removeImmediateCalls++; }
        function getFrozenTask(url) { return root._frozenTaskReturn; }
        function removeFrozenTask(url) { root._removeFrozenCalls++; }
    }

    // publishGeometryTimer.start() — onStopped calls it for windows/startups.
    property int _publishTimerStarts: 0
    QtObject {
        id: publishGeometryTimer
        function start() { root._publishTimerStarts++; }
    }

    // ---- the taskItem the animation targets ----
    // needLength.addEvent / removeEvent are counted; the parabolicItem holds the
    // animated properties; the abilities tree feeds the `speed` binding and the
    // launchers.inCurrentActivity guard inside execute().
    property int _addEventCalls: 0
    property int _removeEventCalls: 0
    property bool _inCurrentActivity: false
    property int _groupedWindowAddedCalls: 0

    QtObject {
        id: parabolicItemObj
        property real opacity: 1
        property real zoomLength: 1
        property real zoomThickness: 1
        property real zoom: 1
    }

    QtObject {
        id: taskItem
        property string launcherUrl: "applications:firefox.desktop"
        property string launcherUrlWithIcon: "applications:firefox.desktop?iconData=x"
        property bool isVertical: false
        property bool isWindow: false
        property bool isLauncher: false
        property bool isStartup: false
        property bool isSeparator: false
        property bool isDemandingAttention: false
        property bool inAnimation: false
        property bool inAddRemoveAnimation: false
        property bool visible: true
        property real iconAnimatedOffsetX: 0
        property real iconAnimatedOffsetY: 0
        property QtObject parabolicItem: parabolicItemObj
        function taskGroupedWindowAdded() { root._groupedWindowAddedCalls++; }

        property QtObject abilities: QtObject {
            property QtObject animations: QtObject {
                property QtObject speedFactor: QtObject { property real current: 1.0; property real normal: 1.0 }
                property QtObject duration: QtObject { property int large: 200 }
                property QtObject needLength: QtObject {
                    function addEvent(e) { root._addEventCalls++; }
                    function removeEvent(e) { root._removeEventCalls++; }
                }
            }
            property QtObject launchers: QtObject {
                function inCurrentActivity(url) { return root._inCurrentActivity; }
            }
            property QtObject metrics: QtObject { property int iconSize: 48 }
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/task/animations/ShowWindowAnimation.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // Reset the mock state every test so deltas are unambiguous.
    function reset() {
        root.newWindowSlidingEnabled = true;
        root.vertical = false;
        root.inActivityChange = false;
        root.showWindowsOnlyFromLaunchers = false;
        root.disableAllWindowsFunctionality = false;
        root.inDraggingPhase = false;
        root.isForcedHidden = false;
        root.isWindow = false;
        root.isLauncher = false;
        root.index = 0;
        root._launcherPositionReturn = -1;
        root._launcherActivitiesReturn = [];
        root._removeToBeAddedCalls = 0;
        root._removeImmediateCalls = 0;
        root._removeFrozenCalls = 0;
        root._toBeAddedExists = false;
        root._immediateExists = false;
        root._frozenTaskReturn = null;
        root._publishTimerStarts = 0;
        root._addEventCalls = 0;
        root._removeEventCalls = 0;
        root._inCurrentActivity = false;
        root._groupedWindowAddedCalls = 0;
        taskItem.isWindow = false;
        taskItem.isLauncher = false;
        taskItem.isStartup = false;
        taskItem.isSeparator = false;
        taskItem.isDemandingAttention = false;
        taskItem.inAnimation = false;
        taskItem.inAddRemoveAnimation = false;
        taskItem.visible = true;
        parabolicItemObj.opacity = 1;
        parabolicItemObj.zoomLength = 1;
        parabolicItemObj.zoomThickness = 1;
        parabolicItemObj.zoom = 1;
        taskItem.abilities.animations.speedFactor.current = 1.0;
    }

    // The object builds and the `speed` binding evaluates from the mocked
    // abilities tree: 1.2 * normal(1) * large(200) = 240.
    function test_construct_and_speed() {
        reset();
        const obj = make();
        compare(obj.animationSent, false);
        compare(obj.speed, Math.round(1.2 * 1.0 * 200));
        // newWindowSlidingEnabled=false drives the speed binding to its 0 branch.
        root.newWindowSlidingEnabled = false;
        compare(obj.speed, 0);
        root.newWindowSlidingEnabled = true;
    }

    // execute(): hideWindow branch. showWindowsOnlyFromLaunchers + window + no
    // launcher shown => the task is force-hidden.
    function test_execute_hideWindow() {
        reset();
        root.showWindowsOnlyFromLaunchers = true;
        taskItem.isWindow = true;
        root.isWindow = true;            // gates the icList reorder block (childAtIndex->null)
        const obj = make();
        obj.execute();
        compare(root.isForcedHidden, true);
        compare(taskItem.visible, false);
        compare(parabolicItemObj.zoomLength, 0.0);
        compare(parabolicItemObj.zoomThickness, 0.0);
        compare(parabolicItemObj.opacity, 0);
        compare(taskItem.inAnimation, false);
    }

    // execute(): the separator / non-compositing / dragging branch. isSeparator
    // makes the OR true regardless of compositing, fully revealing the task.
    function test_execute_separatorBranch() {
        reset();
        taskItem.isSeparator = true;
        const obj = make();
        obj.execute();
        compare(root.isForcedHidden, false);
        compare(taskItem.visible, true);
        compare(parabolicItemObj.zoomLength, 1.0);
        compare(parabolicItemObj.zoomThickness, 1.0);
        compare(parabolicItemObj.zoom, 1);
        compare(parabolicItemObj.opacity, 1);
        compare(taskItem.inAnimation, false);
    }

    // execute(): the start()-the-animation branch. A window with no shown
    // launcher and compositing active (true offscreen) and speedFactor!=0 and
    // not already-shown => zoom collapses to 0 and the animation starts.
    function test_execute_startsAnimation() {
        reset();
        taskItem.isWindow = true;
        root._inCurrentActivity = false;   // !inCurrentActivity -> animation2 true
        const obj = make();
        verify(!obj.running);
        obj.execute();
        compare(root.isForcedHidden, false);
        compare(taskItem.visible, true);
        compare(parabolicItemObj.zoomLength, 0.0);
        compare(parabolicItemObj.zoomThickness, 0.0);
        verify(obj.running);
        obj.stop();
    }

    // execute(): the final else branch (no frozen task). speedFactor.current=0
    // fails the animation else-if, so it falls through to the "just reveal"
    // path with zoomLength/zoomThickness=1.
    function test_execute_elseNoFrozen() {
        reset();
        taskItem.abilities.animations.speedFactor.current = 0;
        parabolicItemObj.zoomLength = 0;
        parabolicItemObj.zoomThickness = 0;
        const obj = make();
        obj.execute();
        compare(parabolicItemObj.zoomLength, 1.0);
        compare(parabolicItemObj.zoomThickness, 1.0);
        compare(parabolicItemObj.opacity, 1);
        compare(taskItem.inAnimation, false);
    }

    // execute(): the frozen-task restore branch. A frozen task with zoom>1
    // restores that zoom and removes the frozen entry.
    function test_execute_frozenRestore() {
        reset();
        taskItem.abilities.animations.speedFactor.current = 0;
        root._frozenTaskReturn = { zoom: 1.6 };
        const obj = make();
        obj.execute();
        compare(parabolicItemObj.zoom, 1.6);
        compare(root._removeFrozenCalls, 1);
    }

    // execute() also clears a pending immediate-launcher for a launcher item.
    function test_execute_removesImmediateLauncher() {
        reset();
        taskItem.isLauncher = true;
        root.isLauncher = true;
        root._immediateExists = true;
        taskItem.abilities.animations.speedFactor.current = 0;
        const obj = make();
        obj.execute();
        compare(root._removeImmediateCalls, 1);
    }

    // showWindow() delegates straight to execute(); assert via the same hideWindow
    // side effect so both entries are genuinely covered.
    function test_showWindow_delegates() {
        reset();
        root.disableAllWindowsFunctionality = true;
        taskItem.isWindow = true;
        const obj = make();
        obj.showWindow();
        compare(root.isForcedHidden, true);
        compare(taskItem.visible, false);
    }

    // onStopped: stopping the animation runs the completion handler. It clears
    // the add/remove + in-animation flags, starts the publish timer for a
    // window, fires taskGroupedWindowAdded for a demanding-attention window, and
    // removes the length event it had registered.
    function test_onStopped_windowPath() {
        reset();
        taskItem.isWindow = true;
        root._inCurrentActivity = false;       // animation2 path -> start() + addEvent
        taskItem.isDemandingAttention = true;
        taskItem.inAddRemoveAnimation = true;
        const obj = make();
        obj.execute();                          // starts it; the ScriptAction sends addEvent
        verify(obj.running);
        tryVerify(function() { return root._addEventCalls >= 1; }, 2000, "length event never added");
        obj.stop();                             // -> onStopped
        compare(taskItem.inAddRemoveAnimation, false);
        compare(taskItem.inAnimation, false);
        compare(root._publishTimerStarts, 1);
        compare(root._groupedWindowAddedCalls, 1);
        // animationSent was set by the ScriptAction; onStopped removes the event
        // and clears the flag.
        compare(obj.animationSent, false);
        verify(root._removeEventCalls >= 1);
    }

    // onStopped also drops a to-be-added launcher when one is pending.
    function test_onStopped_dropsToBeAdded() {
        reset();
        taskItem.isWindow = true;
        root._inCurrentActivity = false;
        root._toBeAddedExists = true;           // onStopped should remove it
        const obj = make();
        obj.execute();
        verify(obj.running);
        obj.stop();
        compare(root._removeToBeAddedCalls, 1);
    }

    // Component.onDestruction: when animationSent is still true at teardown, the
    // guard removes the registered length event. Drive it by starting the
    // animation (ScriptAction sets animationSent + addEvent), then destroying
    // before it stops.
    function test_onDestruction_removesEvent() {
        reset();
        taskItem.isWindow = true;
        root._inCurrentActivity = false;
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, c.errorString());
        const obj = c.createObject(root, {});
        verify(obj, "instantiate failed");
        obj.execute();
        tryVerify(function() { return obj.animationSent === true; }, 2000, "animationSent never set");
        const before = root._removeEventCalls;
        obj.destroy();
        tryVerify(function() { return root._removeEventCalls > before; }, 2000,
                  "onDestruction did not remove the length event");
    }
}
