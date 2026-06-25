// Coverage for the maxlength ruler's MouseArea (RulerMouseArea.qml).
//
// The component is a bare MouseArea whose only real logic is updateMaxLength(step):
// it reads/writes plasmoid.configuration (maxLength/minLength/offset/alignment),
// clamps the new max length to 30..100, optionally drags minLength along when the two
// were equal, and rewrites offset when the dock would run off the 100-unit canvas.
// It also reads root.isHorizontal (cursorShape) and tooltip.visible (onVisibleChanged).
//
// We name the TestCase `id: root` so the component's unqualified `root.isHorizontal`
// resolves here, and supply `plasmoid`, `tooltip` and the LatteCore alignment values
// as shaped mocks. updateMaxLength runs for real against the writable configuration
// object; each test asserts the resulting config values.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "RulerMouseArea"
    when: windowShown
    visible: false
    width: 200
    height: 200

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/configuration/canvas/maxlength/RulerMouseArea.qml")

    // root.isHorizontal drives the cursorShape binding.
    property bool isHorizontal: true

    // onVisibleChanged reads tooltip.visible.
    QtObject {
        id: tooltip
        property bool visible: true
    }

    // Writable stand-in for plasmoid.configuration. Defaults reset per test via reset().
    QtObject {
        id: configuration
        property int maxLength: 50
        property int minLength: 30
        property int offset: 0
        property int alignment: 0
    }

    QtObject {
        id: plasmoid
        property var configuration: configuration
    }

    function reset(maxL, minL, off, align) {
        configuration.maxLength = maxL;
        configuration.minLength = minL;
        configuration.offset = off;
        configuration.alignment = align;
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed: " + c.errorString());
        return obj;
    }

    // A left-aligned step up adds the step to maxLength without touching offset.
    function test_stepUpAddsStep() {
        reset(50, 30, 0, 1 /* Left, not Center/Justify */);
        const area = make();
        area.updateMaxLength(6);
        compare(configuration.maxLength, 56, "maxLength should advance by the step");
        compare(configuration.offset, 0, "offset stays put while inside the canvas");
        compare(configuration.minLength, 30, "minLength untouched when it differed from maxLength");
    }

    // updateMaxLength clamps the new max length at the 100 ceiling.
    function test_clampUpperBound() {
        reset(98, 30, 0, 1);
        const area = make();
        area.updateMaxLength(6); // 98+6 = 104 -> clamped to 100
        compare(configuration.maxLength, 100, "maxLength should clamp to 100");
    }

    // updateMaxLength clamps the new max length at the 30 floor.
    function test_clampLowerBound() {
        reset(34, 30, 0, 1);
        const area = make();
        area.updateMaxLength(-6); // 34-6 = 28 -> clamped to 30
        compare(configuration.maxLength, 30, "maxLength should clamp to 30");
    }

    // When maxLength == minLength the step drags minLength along with it.
    function test_minLengthFollowsWhenEqual() {
        reset(50, 50, 0, 1);
        const area = make();
        area.updateMaxLength(6);
        compare(configuration.maxLength, 56, "maxLength advances");
        compare(configuration.minLength, 56, "minLength follows maxLength when they were equal");
    }

    // The result is floored at the current minLength even after clamping.
    function test_minLengthFloorsValue() {
        reset(40, 80, 0, 1);
        const area = make();
        area.updateMaxLength(-6); // 40-6=34, but minLength is 80
        compare(configuration.maxLength, 80, "value cannot drop below minLength");
    }

    // A left-aligned dock running past the canvas pulls offset back to fit.
    function test_offsetPulledBackForSideAlignment() {
        reset(98, 30, 20, 1 /* Left */);
        const area = make();
        area.updateMaxLength(6); // value=100, newTotal=20+100=120 > 100
        compare(configuration.maxLength, 100);
        compare(configuration.offset, 0, "offset rewound to max(0, 100-value) = 0");
    }

    // Centered alignment uses the symmetric suggested-offset path. A large centered
    // length with a negative offset gets a clamped negative suggestion.
    function test_centeredAlignmentRewritesOffset() {
        reset(94, 30, -40, 0 /* Center */);
        const area = make();
        area.updateMaxLength(6); // value=100; centeredCheck true -> suggested = -(50 - 100/2) = 0
        compare(configuration.maxLength, 100);
        compare(configuration.offset, 0, "centered overrun collapses offset to the symmetric limit");
    }
}
