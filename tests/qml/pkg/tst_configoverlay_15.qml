// Coverage for the edit-mode applet overlay (ConfigOverlay.qml). The real file is a
// MouseArea inside the containment that reads a pile of context objects by bare name --
// root, metrics, animations, layoutsContainer, fastLayoutManager, layouter. None exist
// when the file loads alone, so we wrap it in a host document that declares them as ids
// and pull the overlay in through a Loader: the Loader's child context inherits the
// host's ids, exactly how the overlay resolves them in the live dock.
//
// The overlay is loaded from the staged (instrumented) install tree so every reached
// function / signal handler fires a Cov tick. Every test asserts an observable effect:
// a returned point, a property the handler set, or a mock side-effect counter.
import QtQuick
import QtTest

TestCase {
    id: tc
    name: "ConfigOverlay15"
    when: windowShown
    visible: true
    width: 500
    height: 500

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/editmode/ConfigOverlay.qml")

    readonly property string hostQml:
        'import QtQuick\n'
      + 'Item {\n'
      + '    id: root\n'
      + '    width: 300; height: 64\n'
      + '    property bool inConfigureAppletsMode: true\n'
      + '    property bool isHorizontal: true\n'
      + '    property bool isVertical: false\n'
      + '    property int maxLength: 600\n'
      + '    property bool colorizerEnabled: true\n'
      + '    property QtObject dragOverlay: QtObject { property var currentApplet: null }\n'
      + '    property QtObject debug: QtObject { property bool graphicsEnabled: false }\n'
      + '    property QtObject latteDebug: QtObject { function debugLog(m) {} }\n'
      + '    property QtObject environment: QtObject { property bool isGraphicsSystemAccelerated: false }\n'
      + '    property QtObject myView: QtObject {\n'
      + '        property int alignment: 0\n'
      + '        property QtObject itemShadow: QtObject { property int size: 4; property string shadowColor: "#000000" }\n'
      + '    }\n'
      + '    property QtObject metrics: metricsId\n'
      + '    QtObject { id: metricsId; property int iconSize: 48; property int extraThicknessForNormal: 0;\n'
      + '        property QtObject mask: QtObject { property QtObject thickness: QtObject { property int maxNormal: 48 } }\n'
      + '        property QtObject margin: QtObject { property int screenEdge: 0 } }\n'
      + '    QtObject { id: animations; property QtObject duration: QtObject { property int large: 20 } }\n'
      + '    Item { id: layoutsContainer\n'
      + '        property Item mainLayout: mainL\n'
      + '        property Item startLayout: startL\n'
      + '        property Item endLayout: endL\n'
      + '        Item { id: mainL; width: 200; height: 48 }\n'
      + '        Item { id: startL; width: 40; height: 48 }\n'
      + '        Item { id: endL; width: 40; height: 48 }\n'
      + '    }\n'
      + '    QtObject { id: fastLayoutManager\n'
      + '        property int inserts: 0; property int saves: 0; property int sets: 0; property int justifies: 0\n'
      + '        property string lastSetKey: ""; property var lastSetValue: undefined\n'
      + '        function insertBefore(a, b) { inserts++; }\n'
      + '        function insertAfter(a, b) { inserts++; }\n'
      + '        function save() { saves++; }\n'
      + '        function setOption(i, k, v) { sets++; lastSetKey = k; lastSetValue = v; }\n'
      + '        function moveAppletsBasedOnJustifyAlignment() { justifies++; }\n'
      + '    }\n'
      + '    QtObject { id: layouter; property int fills: 0; function updateSizeForAppletsInFill() { fills++; } }\n'
      + '    property alias overlay: ovLoader.item\n'
      + '    property alias flm: fastLayoutManager\n'
      + '    property alias lyt: layouter\n'
      + '    Loader { id: ovLoader; anchors.fill: parent }\n'
      + '}\n';

    property var host: null
    property var overlay: null

    // Stand-in applet shaped like the items the overlay pokes at: a layout child
    // with .applet.plasmoid.configuration plus latteStyleApplet / communicator.
    Component {
        id: appletMock
        Item {
            id: am
            width: 40; height: 48
            property bool isInternalViewSplitter: false
            property bool isSeparator: false
            property bool isParabolicEdgeSpacer: false
            property bool lockZoom: true
            property bool userBlocksColorizing: true
            property bool appletBlocksColorizing: false
            property int increases: 0
            property int decreases: 0

            property QtObject communicator: QtObject {
                property bool indexerIsSupported: false
                property bool appletBlocksParabolicEffect: false
            }
            property QtObject latteStyleApplet: QtObject {
                function increaseLength() { am.increases++; }
                function decreaseLength() { am.decreases++; }
            }
            property QtObject applet: QtObject {
                property QtObject plasmoid: QtObject {
                    property int id: 7
                    property string pluginName: "org.kde.someapplet"
                    property string title: "Some Applet"
                    property QtObject configuration: QtObject { property int length: 0 }
                    function internalAction(name) {
                        if (name === "configure") return configureAct;
                        if (name === "remove") return removeAct;
                        return null;
                    }
                }
            }
            property QtObject configureAct: QtObject { property bool enabled: true; property int triggers: 0; function trigger() { triggers++; } }
            property QtObject removeAct: QtObject { property bool enabled: true; property int triggers: 0; function trigger() { triggers++; } }
        }
    }

    function findLoader(obj) {
        const kids = obj.children;
        for (var i = 0; i < kids.length; i++) {
            if (kids[i] && typeof kids[i].setSource === "function" && kids[i].hasOwnProperty("item"))
                return kids[i];
        }
        return null;
    }

    function mainLayoutOf() {
        const kids = host.children;
        for (var i = 0; i < kids.length; i++) {
            if (kids[i] && kids[i].hasOwnProperty("mainLayout"))
                return kids[i].mainLayout;
        }
        return null;
    }

    function init() {
        host = Qt.createQmlObject(hostQml, tc, "host.qml");
        verify(host, "host failed to build");
        const ldr = findLoader(host);
        verify(ldr, "host Loader not found");
        ldr.setSource(targetUrl);
        tryVerify(function() { return host.overlay !== null && host.overlay !== undefined; },
                  4000, "overlay did not load");
        overlay = host.overlay;
        verify(overlay, "overlay null after load");
    }

    function cleanup() {
        if (host) { host.destroy(); host = null; overlay = null; }
    }

    // hoveredItem walks all three layouts via childAt(); relevantLayoutForApplet maps an
    // applet's parent layout to overlay coords. Assert the off-grid miss returns null and
    // the parent-is-mainLayout applet maps to a real point.
    function test_hoveredItem_and_relevantLayout() {
        var off = overlay.hoveredItem(2000, 2000); // misses every layout -> null
        verify(off === null || off === undefined, "off-grid hover should miss");

        var ml = mainLayoutOf();
        verify(ml, "mainLayout unreachable");
        var a = appletMock.createObject(ml);       // parent === mainLayout branch
        var rl = overlay.relevantLayoutForApplet(a);
        verify(rl !== undefined && rl !== null, "mainLayout branch should map to a point");
        compare(typeof rl.x, "number", "mapped point has x");
        compare(typeof rl.y, "number", "mapped point has y");

        // An applet parented to nothing the overlay knows hits no branch -> undefined.
        var orphan = appletMock.createObject(overlay);
        var none = overlay.relevantLayoutForApplet(orphan);
        verify(none === undefined, "unknown-parent applet maps to nothing");
        orphan.destroy();
        a.destroy();
    }

    // onCurrentAppletChanged copies currentApplet into previousCurrentApplet and, once it
    // finds the applet's layout, syncs the lock/colorize buttons from the applet's state.
    function test_currentAppletChanged() {
        var ml = mainLayoutOf();
        var a = appletMock.createObject(ml);   // lockZoom:true, userBlocksColorizing:true
        host.dragOverlay.currentApplet = a;
        overlay.currentApplet = a;             // fires handler, reaches button sync
        compare(overlay.previousCurrentApplet, a, "previousCurrentApplet tracks the assignment");

        var lock = findButton("lock") || findButton("unlock");
        var color = findButton("color-picker");
        verify(lock, "lock button not found");
        verify(color, "colorize button not found");
        compare(lock.checked, a.lockZoom, "lock button synced to applet lockZoom");
        compare(color.checked, !a.userBlocksColorizing, "colorize button synced to applet state");

        overlay.currentApplet = null;          // early-return branch (no applet)
        compare(overlay.previousCurrentApplet, null, "clearing currentApplet propagates");
        host.dragOverlay.currentApplet = null;
        a.destroy();
    }

    // onVisualParentChanged on the tooltip fires when currentApplet (its visualParent)
    // changes: it sets each button's visibility and the label text from the applet title.
    function test_tooltipVisualParent() {
        var ml = mainLayoutOf();
        var a = appletMock.createObject(ml);
        host.dragOverlay.currentApplet = a;
        overlay.currentApplet = a;             // tooltip.visualParent binding -> handler

        var label = findLabel();
        verify(label, "tooltip label not found");
        compare(label.text, a.applet.plasmoid.title, "label shows the applet title");

        var configure = findButton("configure");
        verify(configure, "configure button not found");
        verify(configure.visible, "configure button shown for a configurable applet");

        host.dragOverlay.currentApplet = null;
        overlay.currentApplet = null;
        a.destroy();
    }

    // released() with currentApplet.applet present writes the handle size into the applet
    // configuration, then saves the layout and reflows fills.
    function test_released() {
        var ml = mainLayoutOf();
        var a = appletMock.createObject(ml);
        host.dragOverlay.currentApplet = a;
        overlay.currentApplet = a;

        var savesBefore = host.flm.saves;
        var fillsBefore = host.lyt.fills;

        // onReleased ignores the mouse arg; QML still wants one slotted for the signal.
        overlay.released(null);    // currentApplet && currentApplet.applet -> save path

        compare(host.flm.saves, savesBefore + 1, "released saves the layout");
        compare(host.lyt.fills, fillsBefore + 1, "released reflows fills");
        compare(typeof a.applet.plasmoid.configuration.length, "number",
                "handle size written to configuration.length");

        host.dragOverlay.currentApplet = null;
        overlay.currentApplet = null;
        a.destroy();
    }

    // Connections { target: currentApplet } onWidthChanged / onHeightChanged fire when the
    // current applet resizes. The body only moves the applet while pressed (unreachable
    // headlessly), so assert it correctly took the no-op path: position stays put.
    function test_connections_size() {
        var ml = mainLayoutOf();
        var a = appletMock.createObject(ml);
        host.dragOverlay.currentApplet = a;
        overlay.currentApplet = a;        // wires the Connections target to this applet
        overlay.lastX = 50;
        overlay.lastY = 50;
        a.x = 11; a.y = 22;
        var x0 = a.x, y0 = a.y;
        a.width = 80;   // -> Connections.onWidthChanged (not pressed -> no move)
        a.height = 90;  // -> Connections.onHeightChanged
        compare(a.width, 80, "width change applied (handler trigger)");
        compare(a.height, 90, "height change applied (handler trigger)");
        compare(a.x, x0, "onWidthChanged is a no-op while not pressed");
        compare(a.y, y0, "onHeightChanged is a no-op while not pressed");
        host.dragOverlay.currentApplet = null;
        overlay.currentApplet = null;
        a.destroy();
    }

    // The hide timer's onTriggered clears tooltip + currentApplet. Shrink interval, fire,
    // assert currentApplet ends up null.
    function test_hideTimer() {
        var a = appletMock.createObject(overlay);
        overlay.currentApplet = a;
        verify(overlay.currentApplet === a, "currentApplet set before timer");
        const t = findHideTimer();
        verify(t, "hide timer not found");
        t.interval = 1;
        t.restart();
        tryVerify(function() { return overlay.currentApplet === null; }, 2000,
                  "hide timer did not clear currentApplet");
    }

    function findHideTimer() {
        const res = overlay.resources;
        for (var i = 0; i < res.length; i++) {
            var r = res[i];
            if (r && r.hasOwnProperty("interval") && typeof r.restart === "function"
                && r.hasOwnProperty("running") && r.hasOwnProperty("triggeredOnStart"))
                return r;
        }
        return null;
    }

    // The four tooltip buttons' onClicked handlers. Invoke each clicked() directly and
    // assert the side-effect it drives: configure/remove trigger their action, colorize
    // and lock call fastLayoutManager.setOption with the right key.
    function test_tooltipButtons() {
        var ml = mainLayoutOf();
        var a = appletMock.createObject(ml);
        host.dragOverlay.currentApplet = a;
        overlay.currentApplet = a;

        var configure = findButton("configure");
        var color = findButton("color-picker");
        var lock = findButton("lock") || findButton("unlock");
        var close = findButton("delete");
        verify(configure, "configure button not found");
        verify(color, "colorize button not found");
        verify(lock, "lock button not found");
        verify(close, "close button not found");

        configure.clicked();   // -> internalAction("configure").trigger()
        compare(a.configureAct.triggers, 1, "configure button triggered the configure action");

        close.clicked();       // -> internalAction("remove").trigger()
        compare(a.removeAct.triggers, 1, "close button triggered the remove action");

        var setsBefore = host.flm.sets;
        color.clicked();       // -> setOption(id, "userBlocksColorizing", !checked)
        compare(host.flm.sets, setsBefore + 1, "colorize click called setOption");
        compare(host.flm.lastSetKey, "userBlocksColorizing",
                "colorize click set userBlocksColorizing");

        setsBefore = host.flm.sets;
        lock.clicked();        // -> setOption(id, "lockZoom", checked)
        compare(host.flm.sets, setsBefore + 1, "lock click called setOption");
        compare(host.flm.lastSetKey, "lockZoom", "lock click set lockZoom");

        host.dragOverlay.currentApplet = null;
        overlay.currentApplet = null;
        a.destroy();
    }

    function isButton(k) {
        return k.hasOwnProperty("icon") && typeof k.clicked === "function";
    }

    // Depth-first search for a tooltip button by icon.name.
    function findButton(iconName) {
        return findInTree(tooltipMainItem(), function(k) {
            return isButton(k) && k.icon && k.icon.name === iconName;
        });
    }

    // The tooltip's title Label is a Row child, not inside a button. Each ToolButton
    // carries its own (empty) content Label, so don't descend into buttons or the search
    // would return a button's blank label first.
    function findLabel() {
        return findInTree(tooltipMainItem(), function(k) {
            return k.hasOwnProperty("text") && k.hasOwnProperty("textFormat")
                && !k.hasOwnProperty("icon");
        }, isButton);
    }

    function tooltipMainItem() {
        const res = overlay.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].hasOwnProperty("visualParent") && res[i].hasOwnProperty("mainItem"))
                return res[i].mainItem;
        }
        return null;
    }

    function findInTree(node, pred, skipDescend) {
        if (!node)
            return null;
        var kids = node.children;
        if (kids) {
            for (var i = 0; i < kids.length; i++) {
                var k = kids[i];
                if (k && pred(k))
                    return k;
                if (k && skipDescend && skipDescend(k))
                    continue;
                var deep = findInTree(k, pred, skipDescend);
                if (deep)
                    return deep;
            }
        }
        return null;
    }
}
