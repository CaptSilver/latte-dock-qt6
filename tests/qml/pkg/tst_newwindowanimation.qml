// Coverage for the plasmoid's NewWindowAnimation (and the BounceAnimation it
// loads through its Loader — the relative "newwindow/BounceAnimation.qml"
// source resolves inside the staged tree, so the loaded copy is instrumented
// too). The component reads the unqualified context names `taskItem` and
// `root.windowInAttentionEnabled` / `root.windowAddedInGroupEnabled`; QML
// resolves those against the creation context, so the TestCase is `id: root`
// and taskItem is a QtObject shaped like the real task item: the writable
// animation flags, the bounce offset properties BounceAnimation animates, the
// taskGroupedWindowAdded signal Component.onCompleted connects to, and the
// abilities subtree (speedFactor/duration/needThickness/indicators/myView/
// metrics). Side-effecting mock calls are recorded so every test asserts an
// observable effect.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "NewWindowAnimation"
    when: windowShown
    visible: true
    width: 200
    height: 200

    // ---- root.* the component reads ----
    property bool windowInAttentionEnabled: false
    property bool windowAddedInGroupEnabled: false

    // ---- recorded mock side effects ----
    property var _blockingCalls: []
    property int _addThicknessCalls: 0
    property int _removeThicknessCalls: 0
    property string _lastThicknessEvent: ""

    QtObject {
        id: taskItem
        property bool inAttention: false
        property bool containsMouse: false
        property bool inAttentionBuiltinAnimation: false
        property bool inNewWindowBuiltinAnimation: false
        property bool isVertical: false
        property real iconAnimatedOffsetX: 0
        property real iconAnimatedOffsetY: 0
        signal taskGroupedWindowAdded()
        function setBlockingAnimation(v) { root._blockingCalls.push(v); }

        property QtObject abilities: QtObject {
            property QtObject animations: QtObject {
                property QtObject speedFactor: QtObject { property real normal: 1.0 }
                // large enough that the ~5.4x speed bounce is still mid-flight
                // when a test stops it; the tests never wait for completion.
                property QtObject duration: QtObject { property int large: 250 }
                property QtObject needThickness: QtObject {
                    function addEvent(e) { root._addThicknessCalls++; root._lastThicknessEvent = String(e); }
                    function removeEvent(e) { root._removeThicknessCalls++; }
                }
            }
            property QtObject indicators: QtObject {
                property QtObject info: QtObject {
                    property bool providesInAttentionAnimation: false
                    property bool providesGroupedWindowAddedAnimation: false
                }
            }
            property QtObject myView: QtObject { property bool isHidden: false }
            property QtObject metrics: QtObject { property int iconSize: 48 }
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/task/animations/NewWindowAnimation.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // Reset the shared mock state so per-test call counts are unambiguous.
    function reset() {
        root.windowInAttentionEnabled = false;
        root.windowAddedInGroupEnabled = false;
        root._blockingCalls = [];
        root._addThicknessCalls = 0;
        root._removeThicknessCalls = 0;
        root._lastThicknessEvent = "";
        taskItem.inAttention = false;
        taskItem.inAttentionBuiltinAnimation = false;
        taskItem.inNewWindowBuiltinAnimation = false;
        taskItem.iconAnimatedOffsetX = 0;
        taskItem.iconAnimatedOffsetY = 0;
        taskItem.abilities.indicators.info.providesInAttentionAnimation = false;
        taskItem.abilities.indicators.info.providesGroupedWindowAddedAnimation = false;
        taskItem.abilities.myView.isHidden = false;
    }

    // init(): blocks the task, raises the builtin-animation flags and registers
    // the thickness event. isDemandingAttention is false here, so the attention
    // flag mirrors that.
    function test_init_registersThicknessEvent() {
        reset();
        const obj = make();
        obj.init();
        compare(root._blockingCalls.length, 1);
        compare(root._blockingCalls[0], true);
        compare(taskItem.inNewWindowBuiltinAnimation, true);
        compare(taskItem.inAttentionBuiltinAnimation, false);
        compare(root._addThicknessCalls, 1);
        verify(root._lastThicknessEvent.indexOf("_newwindow") !== -1,
               "thickness event key missing suffix: " + root._lastThicknessEvent);
    }

    // startNewWindowAnimation() through the real taskGroupedWindowAdded
    // connection (Component.onCompleted wired it to our mock signal): with
    // windowAddedInGroupEnabled the bounce starts and init()'s side effects land.
    function test_startNewWindowAnimation_viaGroupedWindowAdded() {
        reset();
        root.windowAddedInGroupEnabled = true;
        const obj = make();
        verify(!obj.running);
        taskItem.taskGroupedWindowAdded();
        compare(obj.running, true);
        compare(root._blockingCalls[root._blockingCalls.length - 1], true);
        compare(taskItem.inNewWindowBuiltinAnimation, true);
        obj.stop();
        compare(obj.running, false);
    }

    // startNewWindowAnimation(): when the indicator already provides the
    // grouped-window animation the early return keeps everything untouched.
    function test_startNewWindowAnimation_indicatorProvidesIt() {
        reset();
        root.windowAddedInGroupEnabled = true;
        taskItem.abilities.indicators.info.providesGroupedWindowAddedAnimation = true;
        const obj = make();
        obj.startNewWindowAnimation();
        compare(obj.running, false);
        compare(root._blockingCalls.length, 0);
        compare(root._addThicknessCalls, 0);
    }

    // stop() mid-bounce: clear() stops the loaded animation, which fires both
    // stopped handlers — the Connections one (removes the thickness event,
    // unblocks, drops the flags) and BounceAnimation's own (snaps the icon
    // offset back to 0 even though the bounce was interrupted mid-flight).
    function test_stop_clearsAndResetsBounceOffset() {
        reset();
        root.windowAddedInGroupEnabled = true;
        const obj = make();
        obj.startNewWindowAnimation();
        compare(obj.running, true);
        tryVerify(function() { return taskItem.iconAnimatedOffsetY > 0; }, 2000,
                  "bounce never moved the icon offset");
        verify(taskItem.iconAnimatedOffsetY > 0);
        obj.stop();
        compare(obj.running, false);
        compare(taskItem.iconAnimatedOffsetY, 0);
        compare(root._removeThicknessCalls, 1);
        compare(root._blockingCalls[root._blockingCalls.length - 1], false);
        compare(taskItem.inNewWindowBuiltinAnimation, false);
        compare(taskItem.inAttentionBuiltinAnimation, false);
    }

    // stop() while idle takes the !running branch: no clear(), so no blocking
    // call is recorded.
    function test_stop_whenNotRunning_isNoop() {
        reset();
        const obj = make();
        obj.stop();
        compare(obj.running, false);
        compare(root._blockingCalls.length, 0);
    }

    // Attention lifecycle: inAttention=true flips isDemandingAttention, whose
    // handler starts the animation with the attention flag; inAttention=false
    // then hits the Connections onInAttentionChanged guard (still running +
    // inAttentionBuiltinAnimation) and clear()s everything.
    function test_attention_cycle() {
        reset();
        root.windowInAttentionEnabled = true;
        const obj = make();
        taskItem.inAttention = true;
        compare(obj.running, true);
        compare(taskItem.inAttentionBuiltinAnimation, true);
        compare(root._blockingCalls[root._blockingCalls.length - 1], true);
        taskItem.inAttention = false;
        compare(obj.running, false);
        compare(taskItem.inAttentionBuiltinAnimation, false);
        compare(taskItem.inNewWindowBuiltinAnimation, false);
        compare(root._blockingCalls[root._blockingCalls.length - 1], false);
        compare(root._removeThicknessCalls, 1);
    }
}
