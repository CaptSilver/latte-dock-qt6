// Coverage for the plasmoid's LauncherAnimation (the launcher-bounce wrapper)
// and the BounceAnimation it loads, both from the staged (instrumented)
// package. LauncherAnimation reads the unqualified names taskItem / root /
// icList / tasksExtendedManager from its creation context, so the TestCase is
// id: root and declares each one shaped like the real object. The Loader
// resolves launcher/BounceAnimation.qml relative to the staged file, so the
// bounce we start and stop is the instrumented copy too. Every test drives a
// unit and asserts its effect on these mocks (state flags, call counts, the
// icon offset the stop handler must clear).
import QtQuick
import QtTest

TestCase {
    id: root
    name: "LauncherAnimation"
    when: windowShown
    visible: true
    width: 200
    height: 200

    // ---- unqualified context the component reads as `root.*` ----
    property int noTasksInAnimation: 0
    property bool vertical: false
    property bool launcherBouncingEnabled: true

    // init() peeks at the previous ListView task before re-anchoring; returning
    // undefined (the real helper's out-of-range answer) short-circuits that
    // block — anchoring onto a QtObject mock would throw.
    QtObject {
        id: icList
        function childAtIndex(i) { return undefined; }
    }

    property int _removeWaitingCalls: 0
    property string _lastRemoveWaitingUrl: ""
    QtObject {
        id: tasksExtendedManager
        function removeWaitingLauncher(url) {
            root._removeWaitingCalls++;
            root._lastRemoveWaitingUrl = url;
        }
    }

    // ---- the taskItem the animation drives ----
    property int _invkClearZoomCalls: 0
    property int _directRenderingCalls: 0
    property int _addThicknessEvents: 0
    property int _removeThicknessEvents: 0
    property int _animationStartedCalls: 0
    property int _animationEndedCalls: 0
    property var _lastBlockingArg: undefined
    property bool _providesLauncherAnimation: false

    QtObject {
        id: parabolicItemObj
        property real opacity: 1
        property real zoom: 1
    }

    QtObject {
        id: taskItem
        // Component.onCompleted connects startLauncherAnimation to this, so
        // emitting it exercises the real activation path.
        signal taskLauncherActivated()

        property bool inRemoveStage: false
        property bool inBouncingAnimation: false
        // written by the file's Binding element tracking `running`
        property bool isLauncherBuiltinAnimationRunning: false
        property bool isVertical: false
        property real iconAnimatedOffsetX: 0
        property real iconAnimatedOffsetY: 0
        property int lastValidIndex: 0
        property string launcherUrl: "applications:firefox.desktop"
        property QtObject parabolicItem: parabolicItemObj

        function setBlockingAnimation(v) { root._lastBlockingArg = v; }
        function animationStarted() { root._animationStartedCalls++; }
        function animationEnded() { root._animationEndedCalls++; }

        property QtObject abilities: QtObject {
            property QtObject animations: QtObject {
                property QtObject speedFactor: QtObject { property real current: 1.0; property real normal: 1.0 }
                property QtObject duration: QtObject { property int large: 200 }
                property QtObject needThickness: QtObject {
                    function addEvent(e) { root._addThicknessEvents++; }
                    function removeEvent(e) { root._removeThicknessEvents++; }
                }
            }
            property QtObject parabolic: QtObject {
                function invkClearZoom() { root._invkClearZoomCalls++; }
                function setDirectRenderingEnabled(v) { root._directRenderingCalls++; }
            }
            property QtObject indicators: QtObject {
                property QtObject info: QtObject {
                    property bool providesTaskLauncherAnimation: root._providesLauncherAnimation
                }
            }
            property QtObject metrics: QtObject { property int iconSize: 48 }
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/task/animations/LauncherAnimation.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // Reset all mock state so per-test deltas are unambiguous (teardown of a
    // still-bouncing object mutates the counters after the test body returns).
    function reset() {
        root.noTasksInAnimation = 0;
        root.vertical = false;
        root.launcherBouncingEnabled = true;
        root._removeWaitingCalls = 0;
        root._lastRemoveWaitingUrl = "";
        root._invkClearZoomCalls = 0;
        root._directRenderingCalls = 0;
        root._addThicknessEvents = 0;
        root._removeThicknessEvents = 0;
        root._animationStartedCalls = 0;
        root._animationEndedCalls = 0;
        root._lastBlockingArg = undefined;
        root._providesLauncherAnimation = false;
        taskItem.inRemoveStage = false;
        taskItem.inBouncingAnimation = false;
        taskItem.isVertical = false;
        taskItem.iconAnimatedOffsetX = 0;
        taskItem.iconAnimatedOffsetY = 0;
        parabolicItemObj.opacity = 1;
        parabolicItemObj.zoom = 1;
    }

    // init(): the first call registers the whole bouncing state; launchedAlready
    // then gates any re-entry into a no-op.
    function test_init_registersBouncingState() {
        reset();
        const obj = make();
        compare(obj.launchedAlready, false);
        obj.init();
        compare(obj.launchedAlready, true);
        compare(root.noTasksInAnimation, 1);
        compare(taskItem.inBouncingAnimation, true);
        compare(root._invkClearZoomCalls, 1);
        compare(root._addThicknessEvents, 1);
        compare(root._directRenderingCalls, 1);
        compare(root._lastBlockingArg, true);
        obj.init();
        compare(root.noTasksInAnimation, 1);
        compare(root._addThicknessEvents, 1);
    }

    // startLauncherAnimation() via the real taskLauncherActivated connection:
    // the bouncing branch notifies the task, runs init() and starts the loaded
    // BounceAnimation; `running` flows through the Binding into
    // taskItem.isLauncherBuiltinAnimationRunning.
    function test_activationSignal_startsBounce() {
        reset();
        const obj = make();
        verify(!obj.running);
        taskItem.taskLauncherActivated();
        compare(root._animationStartedCalls, 1);
        compare(obj.launchedAlready, true);
        verify(obj.running);
        compare(taskItem.isLauncherBuiltinAnimationRunning, true);
        // stop through the disabled-bouncing branch so the test does not tear
        // down mid-flight
        root.launcherBouncingEnabled = false;
        obj.startLauncherAnimation();
        verify(!obj.running);
        compare(taskItem.isLauncherBuiltinAnimationRunning, false);
    }

    // When the indicator provides its own launcher animation the whole function
    // is a guarded no-op: no task notification, nothing starts.
    function test_indicatorProvidedAnimation_skipsBounce() {
        reset();
        root._providesLauncherAnimation = true;
        const obj = make();
        obj.startLauncherAnimation();
        compare(root._animationStartedCalls, 0);
        compare(obj.launchedAlready, false);
        verify(!obj.running);
    }

    // Stopping the bounce runs both stop handlers: BounceAnimation.onStopped
    // resets the bounced icon offset, and LauncherAnimation's Connections
    // handler unwinds every piece of bouncing state and notifies the task.
    function test_stop_restoresTaskState() {
        reset();
        const obj = make();
        taskItem.taskLauncherActivated();
        verify(obj.running);
        compare(root.noTasksInAnimation, 1);

        // plant a value the bounce stop handler must clear (horizontal task ->
        // iconAnimatedOffsetY is the bounce axis)
        taskItem.iconAnimatedOffsetY = 37;

        root.launcherBouncingEnabled = false;
        obj.startLauncherAnimation();   // else branch -> item.stop() -> onStopped

        verify(!obj.running);
        compare(taskItem.iconAnimatedOffsetY, 0);
        compare(taskItem.inBouncingAnimation, false);
        compare(root._removeWaitingCalls, 1);
        compare(root._lastRemoveWaitingUrl, taskItem.launcherUrl);
        compare(obj.launchedAlready, false);       // clearAnimationsSignals() ran
        compare(root.noTasksInAnimation, 0);
        compare(root._removeThicknessEvents, 1);
        compare(root._lastBlockingArg, false);
        compare(root._animationEndedCalls, 1);
        compare(root._directRenderingCalls, 2);    // init + stop handler
    }

    // A vertical task bounces on iconAnimatedOffsetX; the stop handler resets
    // that axis instead.
    function test_stop_vertical_resetsOffsetX() {
        reset();
        taskItem.isVertical = true;
        const obj = make();
        taskItem.taskLauncherActivated();
        verify(obj.running);
        taskItem.iconAnimatedOffsetX = 21;
        root.launcherBouncingEnabled = false;
        obj.startLauncherAnimation();
        verify(!obj.running);
        compare(taskItem.iconAnimatedOffsetX, 0);
    }
}
