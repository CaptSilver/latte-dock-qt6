// Drives the instrumented units of the containment background MultiLayered.qml
// (a BackgroundProperties/Item with one KSvg.FrameSvgItem child, `solidBackground`,
// that owns all the multi-statement handlers/functions the coverage tool injects
// Cov.tick into). The component is loaded from the staged (instrumented) package by
// file URL so the ticks fire, and every claimed unit asserts an observable effect:
// a timer state change, a mock side-effect, or a property reset.
//
// MultiLayered reads a long list of UNQUALIFIED creation-context names (root.*,
// myView, metrics, animations, indicators, background, colorizerManager,
// layoutsContainerItem, latteView, themeExtended, kirigamiLibraryIsFound, and the
// root layout flags). QML resolves those against the component's creation context,
// so we name the TestCase `id: root` and declare each as a shaped property / id'd
// QtObject. The shapes match what the target actually dereferences — never a
// catch-all. `root.updateEffectsArea` is declared as a real signal because
// Component.onCompleted does `root.updateEffectsArea.connect(updateEffectsArea)`.
//
// The file also reads the QUALIFIED attached object Plasmoid.configuration.* /
// Plasmoid.location in several declarative bindings; those cannot be shadowed by a
// mock (Plasmoid is an attached property, not a creation-context name), so those
// bindings emit benign TypeErrors at construction and the two units whose bodies
// depend on them (onThemeChanged, Component.onDestruction) are reported live-only.
import QtQuick
import QtTest

import org.kde.latte.core 0.2 as LatteCore

TestCase {
    id: root
    name: "MultiLayered"
    when: windowShown
    width: 1000
    height: 64

    // --- root.* layout / state flags the bindings and handlers read ---
    property bool isHorizontal: true
    property bool isVertical: false
    property bool useThemePanel: true
    property bool behaveAsPlasmaPanel: false
    property bool behaveAsDockWithMask: true
    property bool screenEdgeMarginEnabled: false
    property real maxLength: 1000
    property real minLength: 100
    property real offset: 0
    property bool hasExpandedApplet: false
    property bool plasmaBackgroundForPopups: false
    property bool forceSolidPanel: false
    property bool forceTransparentPanel: false
    property bool forcePanelForBusyBackground: false
    property bool userShowPanelBackground: true
    property bool panelOutline: false
    property bool panelShadowsActive: true
    property bool kirigamiLibraryIsFound: true

    // solidBackground's Component.onCompleted connects this signal to its
    // updateEffectsArea(); emitting it must reach that slot. Declared as a real
    // signal (not a property) so .connect() works.
    signal updateEffectsArea()

    QtObject {
        id: myView
        property int alignment: 0
        property real backgroundStoredOpacity: 1
    }

    QtObject {
        id: metrics
        property int iconSize: 48
        property int maxIconSize: 48
        property QtObject margin: QtObject {
            property real screenEdge: 0
            property real length: 0
            property real tailThickness: 0
            property real maxTailThickness: 0
        }
        property QtObject totals: QtObject { property real thickness: 64 }
    }

    QtObject {
        id: animations
        property QtObject speedFactor: QtObject { property real current: 1 }
        property QtObject duration: QtObject { property int small: 100 }
    }

    QtObject {
        id: indicators
        property QtObject info: QtObject { property real backgroundCornerMargin: 1 }
    }

    QtObject {
        id: background
        property real length: 100
        property int thickness: 64
        property real offset: 0
        property QtObject totals: QtObject {
            property real visualThickness: 64
            property real shadowsLength: 0
            property real shadowsThickness: 0
            property real paddingsLength: 0
            property real minThickness: 0
        }
    }

    QtObject {
        id: colorizerManager
        property color backgroundColor: "black"
        property color outlineColor: "white"
        property bool mustBeShown: false
        property var applyTheme: undefined
    }

    property var themeExtended: null

    QtObject {
        id: layoutsContainerItem
        property QtObject mainLayout: QtObject {
            property real length: 100
            property real parabolicOffsetting: 0
        }
    }

    // Shaped latteView mock. invUpdateEffectsArea reads latteView.visibility.isHidden
    // and writes latteView.effects.rect — we observe that write as the side-effect.
    QtObject {
        id: latteView
        property QtObject effects: QtObject {
            property rect rect: Qt.rect(-9, -9, 9, 9)
            property int enabledBorders: 0
        }
        property QtObject visibility: QtObject { property bool isHidden: false }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/background/MultiLayered.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // The 11ms effects-area Timer lives in solidBackground.resources (non-visual),
    // identified by its distinctive interval so this survives reordering.
    function effectsTimer(sb) {
        const res = sb.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].interval === 11)
                return res[i];
        }
        return null;
    }

    // updateEffectsArea(): when the 11ms timer is not already running, it starts it.
    // Stop the timer first, call the function, assert it transitioned to running.
    function test_updateEffectsArea_startsTimer() {
        const obj = make();
        const sb = obj.panelBackgroundSvg;
        const tmr = effectsTimer(sb);
        verify(tmr, "could not find updateEffectsAreaTimer (interval 11)");

        tmr.stop();
        verify(!tmr.running);
        sb.updateEffectsArea();
        verify(tmr.running);
    }

    // Component.onCompleted connected root.updateEffectsArea to
    // solidBackground.updateEffectsArea. Emitting the root signal must reach that
    // slot and (re)start the timer — that propagation is the observable proof the
    // connect ran. Stop the timer, emit, assert it started.
    function test_onCompleted_connectsUpdateSignal() {
        const obj = make();
        const sb = obj.panelBackgroundSvg;
        const tmr = effectsTimer(sb);
        verify(tmr, "could not find updateEffectsAreaTimer (interval 11)");

        tmr.stop();
        verify(!tmr.running);
        root.updateEffectsArea();   // routed through the onCompleted connection
        verify(tmr.running);
    }

    // invUpdateEffectsArea(): with a live-ish latteView mock and compositing active
    // (real KWindowSystem reports true on this session) it takes the visible,
    // non-plasma-panel branch, maps geometry, and writes latteView.effects.rect.
    // Seed the rect to a sentinel and assert the function overwrote it with the
    // computed geometry (x=0, y=0, width/height from solidBackground's size).
    function test_invUpdateEffectsArea_writesEffectsRect() {
        const obj = make();
        const sb = obj.panelBackgroundSvg;

        latteView.effects.rect = Qt.rect(-9, -9, 9, 9);
        sb.invUpdateEffectsArea();
        compare(latteView.effects.rect.x, 0);
        compare(latteView.effects.rect.y, 0);
        verify(latteView.effects.rect.width > 0);
        verify(latteView.effects.rect.height > 0);
    }

    // adjustPrefix(): rebuilds solidBackground.prefix from Plasmoid.location. Headless
    // the location resolves to no edge, so it lands on the default and yields the
    // ["",""] prefix. Dirty the prefix to a sentinel, call adjustPrefix, assert it
    // was reset to ["",""] (proof the function body executed, not just its entry).
    function test_adjustPrefix_resetsPrefix() {
        const obj = make();
        const sb = obj.panelBackgroundSvg;

        sb.prefix = "BOGUS";
        sb.adjustPrefix();
        compare(sb.prefix, ["", ""]);
    }

    // onRepaintNeeded: the FrameSvgItem repaintNeeded handler calls adjustPrefix()
    // only inside `if (root.behaveAsPlasmaPanel)`. Drive both legs: with the flag on,
    // a dirtied prefix is reset by adjustPrefix; with it off, the same dirtied prefix
    // is left untouched. Both branches assert the gated effect.
    function test_onRepaintNeeded_adjustsPrefixWhenPlasmaPanel() {
        const obj = make();
        const sb = obj.panelBackgroundSvg;

        root.behaveAsPlasmaPanel = true;
        sb.prefix = "DIRTY";
        sb.repaintNeeded();
        compare(sb.prefix, ["", ""]);   // adjustPrefix ran

        root.behaveAsPlasmaPanel = false;
        sb.prefix = "KEEP";             // FrameSvgItem normalizes this to ["KEEP"]
        sb.repaintNeeded();
        compare(sb.prefix, ["KEEP"]);   // gated out, prefix preserved (not reset)
    }
}
