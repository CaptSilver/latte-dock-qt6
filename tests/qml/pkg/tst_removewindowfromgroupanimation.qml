// Coverage for the plasmoid's RemoveWindowFromGroupAnimation from the staged
// (instrumented) package. removeTask() is connected to
// taskItem.taskGroupedWindowRemoved at construction; init() then clones a
// ghost icon of the removed window into `root` and start() slides it off the
// dock edge, destroying it when the slide stops. The TestCase (id: root, an
// Item) doubles as the plasmoid root the ghost is parented and mapped into;
// taskIcon / taskIconItem / taskIconContainer / icList and the abilities tree
// are declared here shaped like the real objects. Assertions track the ghost
// itself: where init() placed it, the slide target start() computed, and its
// destruction from the animation's stop handler.
import QtQuick
import QtTest
import org.kde.plasma.core 2.0 as PlasmaCore

TestCase {
    id: root
    name: "RemoveWindowFromGroupAnimation"
    when: windowShown
    visible: true
    width: 300
    height: 300

    property int location: PlasmaCore.Types.BottomEdge
    property bool vertical: false
    property bool windowRemovedFromGroupEnabled: true

    // init() maps taskIcon's origin into root coordinates, so it must be a
    // positioned Item child (a QtObject mock would break mapFromItem).
    Item {
        id: taskIcon
        x: 10
        y: 20
        width: 48
        height: 48
        visible: false
    }

    // the ghost's Kirigami.Icon copies width/source from the task's icon item
    Item {
        id: taskIconItem
        width: 48
        visible: false
        property string source: ""
    }

    QtObject {
        id: taskIconContainer
        property bool toBeDestroyed: false
    }

    QtObject {
        id: icList
        property int orientation: Qt.Horizontal
    }

    property bool _providesRemovedAnimation: false
    QtObject {
        id: taskItem

        signal taskGroupedWindowRemoved()

        property QtObject abilities: QtObject {
            property QtObject indicators: QtObject {
                property QtObject info: QtObject {
                    property bool providesGroupedWindowRemovedAnimation: root._providesRemovedAnimation
                }
            }
            // shadow disabled keeps the ShadowedItem loader inactive; the ghost
            // still builds its icon and desaturation effect
            property QtObject myView: QtObject {
                property QtObject itemShadow: QtObject {
                    property bool isEnabled: false
                    property int size: 6
                }
            }
            property QtObject environment: QtObject {
                property bool isGraphicsSystemAccelerated: false
            }
            property QtObject metrics: QtObject {
                property int iconSize: 48
                property QtObject margin: QtObject { property int thickness: 4 }
            }
            property QtObject animations: QtObject {
                // slide duration = 2 * 0.25 * 120 = 60ms: long enough to catch
                // it mid-flight, short enough that the destroy check is quick
                property QtObject speedFactor: QtObject { property real normal: 0.25 }
                property QtObject duration: QtObject { property int large: 120 }
            }
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/task/animations/RemoveWindowFromGroupAnimation.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    function reset() {
        root.location = PlasmaCore.Types.BottomEdge;
        root.vertical = false;
        root.windowRemovedFromGroupEnabled = true;
        root._providesRemovedAnimation = false;
    }

    // The slide animation is a resource of the ghost, not a child; identify it
    // by its toPoint property.
    function slideAnimationOf(ghost) {
        for (var i = 0; i < ghost.data.length; ++i) {
            if (ghost.data[i] && ('toPoint' in ghost.data[i])) {
                return ghost.data[i];
            }
        }
        return null;
    }

    // Both removeTask() guards reject without spawning a ghost: the indicator
    // already provides a removal animation, or the user disabled the effect.
    function test_removeTask_guardsCreateNothing() {
        reset();
        const obj = make();
        const before = root.children.length;
        root._providesRemovedAnimation = true;
        taskItem.taskGroupedWindowRemoved();
        compare(root.children.length, before);
        root._providesRemovedAnimation = false;
        root.windowRemovedFromGroupEnabled = false;
        taskItem.taskGroupedWindowRemoved();
        compare(root.children.length, before);
    }

    // The full pipeline via the real signal connection: removeTask() -> init()
    // creates the ghost at taskIcon's mapped position, start() reveals it and
    // aims the slide one iconSize outward (downward for a bottom dock), and the
    // animation's stop handler destroys the ghost.
    function test_removeTask_spawnsGhostAndDestroysItOnStop() {
        reset();
        const obj = make();
        const before = root.children.length;

        taskItem.taskGroupedWindowRemoved();

        compare(root.children.length, before + 1);
        const ghost = root.children[root.children.length - 1];
        verify(typeof ghost.start === "function", "last child is not the ghost");
        compare(ghost.x, taskIcon.x);
        compare(ghost.y, taskIcon.y);
        compare(ghost.visible, true);
        compare(ghost.width, taskIcon.width);

        const anim = slideAnimationOf(ghost);
        verify(anim, "slide animation not found on the ghost");
        verify(anim.running);
        compare(anim.toPoint, taskIcon.y + taskItem.abilities.metrics.iconSize);

        tryVerify(function() { return root.children.length === before; }, 4000,
                  "ghost was not destroyed when the slide stopped");
    }

    // A top dock slides the ghost the other way: toPoint is one iconSize above
    // the ghost's start position.
    function test_start_slidesUpForTopEdge() {
        reset();
        root.location = PlasmaCore.Types.TopEdge;
        const obj = make();
        const before = root.children.length;
        taskItem.taskGroupedWindowRemoved();
        compare(root.children.length, before + 1);
        const ghost = root.children[root.children.length - 1];
        const anim = slideAnimationOf(ghost);
        verify(anim, "slide animation not found on the ghost");
        compare(anim.toPoint, taskIcon.y - taskItem.abilities.metrics.iconSize);
        tryVerify(function() { return root.children.length === before; }, 4000,
                  "ghost was not destroyed when the slide stopped");
    }
}
