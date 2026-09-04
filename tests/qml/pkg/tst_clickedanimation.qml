// Coverage for the plasmoid's ClickedAnimation. Its `pressed` is the
// animation's OWN bool property bound to the unqualified `taskItem.pressed` —
// a plain creation-context name, not a QQC2 Button's read-only pressed — so
// flipping the mock property re-evaluates the binding and genuinely fires
// onPressedChanged. The two PropertyAnimations target the unqualified
// `brightnessTaskEffect`, mocked here as a QtObject with a brightness
// property so the started animation has an observable effect beyond
// `running`.
import QtQuick
import QtTest
import Stage 1.0

TestCase {
    id: root
    name: "ClickedAnimation"
    when: windowShown
    visible: true
    width: 200
    height: 200

    QtObject {
        id: brightnessTaskEffect
        property real brightness: 0
    }

    QtObject {
        id: taskItem
        property bool pressed: false
        property int lastButtonClicked: Qt.LeftButton
        property QtObject abilities: QtObject {
            property QtObject animations: QtObject {
                property QtObject speedFactor: QtObject { property real current: 1.0 }
                // 1s per phase keeps the brightness dip observable for ~2.9s,
                // so the tryVerify poll cannot miss the running animation.
                property QtObject duration: QtObject { property int large: 1000 }
            }
            property QtObject indicators: QtObject {
                property QtObject info: QtObject { property bool providesClickedAnimation: false }
            }
        }
    }

    readonly property url targetUrl: Stage.share("plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/task/animations/ClickedAnimation.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    function reset() {
        taskItem.pressed = false;
        taskItem.lastButtonClicked = Qt.LeftButton;
        taskItem.abilities.indicators.info.providesClickedAnimation = false;
        brightnessTaskEffect.brightness = 0;
    }

    // Left-button press starts the brightness pulse; the release re-runs the
    // handler with pressed=false, which must not restart or kill the running
    // animation.
    function test_leftPress_startsBrightnessPulse() {
        reset();
        const obj = make();
        compare(obj.running, false);
        taskItem.pressed = true;
        compare(obj.running, true);
        tryVerify(function() { return brightnessTaskEffect.brightness < 0; }, 2000,
                  "brightness never dipped below 0");
        taskItem.pressed = false;
        compare(obj.running, true);
        obj.stop();
        compare(obj.running, false);
    }

    // When the indicator provides its own clicked animation the press is
    // ignored: nothing starts, the mock brightness never moves.
    function test_press_ignoredWhenIndicatorProvidesIt() {
        reset();
        taskItem.abilities.indicators.info.providesClickedAnimation = true;
        const obj = make();
        taskItem.pressed = true;
        compare(obj.running, false);
        compare(brightnessTaskEffect.brightness, 0);
    }

    // Only left/middle clicks pulse; a right-button press falls through the
    // button guard.
    function test_press_ignoredForRightButton() {
        reset();
        taskItem.lastButtonClicked = Qt.RightButton;
        const obj = make();
        taskItem.pressed = true;
        compare(obj.running, false);
        compare(brightnessTaskEffect.brightness, 0);
    }
}
