// Coverage spike: load the shell-package CompactApplet (the ToolTipArea that
// wraps an applet's compact/full representations) from the staged instrumented
// copy and drive its handlers headless. The component reads almost everything
// through a derived chain (appletItem -> .applet -> .plasmoid), so we inject a
// mock appletItem that supplies a fake hostedApplet/hostedPlasmoid, plus plain
// Items for the compact/full representation slots.
//
// Every test asserts an observable effect of the unit it claims: a derived
// readonly property, a reparent/binding side-effect, a status transition, or a
// signal (toolTipVisibleChanged). No execute-and-verify(true) credit.
import QtQuick
import QtTest

import org.kde.plasma.core 2.0 as PlasmaCore

TestCase {
    id: tc
    name: "CompactApplet24"
    when: windowShown
    visible: true
    width: 400
    height: 400

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/applet/CompactApplet.qml")

    // ---- mocks ---------------------------------------------------------------

    // The "configure" internal action: a QtObject carrying a triggered signal so
    // the Connections in CompactApplet can bind onTriggered to it.
    QtObject {
        id: configureActionMock
        signal triggered()
    }

    // hostedPlasmoid: the Plasma::Applet-side object. Carries location/status,
    // containmentDisplayHints, internalAction(), and the contextual-actions signal.
    QtObject {
        id: plasmoidMock
        property int location: 0
        property int status: 0
        property int containmentDisplayHints: 0
        function internalAction(name) { return name === "configure" ? configureActionMock : null; }
        signal contextualActionsAboutToShow()
    }

    // hostedApplet: the PlasmoidItem / AppletQuickItem graphic object. Carries the
    // representation-side members the tooltip + popup read, and .plasmoid.
    Item {
        id: appletMock
        property bool expanded: false
        property string toolTipMainText: "main"
        property string toolTipSubText: "sub"
        property int toolTipTextFormat: Text.AutoText
        property var toolTipItem: null
        property bool hideOnWindowDeactivate: false
        property var plasmoid: plasmoidMock
    }

    // appletItem: the wrapper. CompactApplet reads appletItem.applet and a pile of
    // icon* ability props through the Indicators API bindings.
    Item {
        id: appletItemMock
        property var applet: appletMock
        property real iconOffsetX: 0
        property real iconOffsetY: 0
        property int iconTransformOrigin: Item.Center
        property real iconOpacity: 1.0
        property real iconRotation: 0
        property real iconScale: 1.0
        property bool isSquare: true
        property bool pressed: false
        property bool originalAppletBehavior: false
        property var animations: QtObject {
            property var duration: QtObject { property int large: 200 }
            property var speedFactor: QtObject { property real current: 1.0 }
        }
        property var indicators: QtObject {
            property var info: QtObject { property bool providesClickedAnimation: false }
        }
    }

    // Plain representation Items.
    Item { id: compactRepMock; width: 32; height: 32 }
    Item {
        id: fullRepMock
        implicitWidth: 120
        implicitHeight: 90
        width: 120
        height: 90
    }

    SignalSpy { id: ttVisibleSpy }

    function make(extra) {
        const c = Qt.createComponent(targetUrl);
        if (c.status === Component.Error)
            fail("compile failed: " + c.errorString());
        compare(c.status, Component.Ready, "component not ready: " + c.errorString());
        var props = { appletItem: appletItemMock };
        if (extra) {
            for (var k in extra) props[k] = extra[k];
        }
        const obj = createTemporaryObject(c, tc, props);
        verify(obj, "instantiate failed: " + c.errorString());
        return obj;
    }

    function init() {
        // Reset mock state so tests are order-independent.
        appletMock.expanded = false;
        plasmoidMock.status = 0;
        appletItemMock.iconOffsetX = 0;
        appletItemMock.iconOffsetY = 0;
        appletItemMock.iconOpacity = 1.0;
        appletItemMock.iconRotation = 0;
        appletItemMock.iconScale = 1.0;
    }

    // Bare instantiation: confirms the ToolTipArea + LatteCore.Dialog + the
    // MultiEffect/animation block all load headless, and the derived hostedApplet/
    // hostedPlasmoid resolve through the injected appletItem. The text bindings
    // (mainText/subText/active) prove the derived chain feeds the ToolTipArea.
    function test_instantiate() {
        const o = make();
        verify(o !== null);
        compare(o.appletItem, appletItemMock);
        // hostedApplet/hostedPlasmoid are readonly derived props.
        compare(o.hostedApplet, appletMock);
        compare(o.hostedPlasmoid, plasmoidMock);
        // tooltip text bindings resolved through hostedApplet.
        compare(o.mainText, "main");
        compare(o.subText, "sub");
        // active is `!hostedApplet.expanded` -> true at rest.
        compare(o.active, true);
        // location is hostedPlasmoid.location (0).
        compare(o.location, 0);
    }

    // Fire onCompactRepresentationChanged: the handler reparents the rep into
    // root, binds its width/height to root, and flips it + root visible.
    function test_compactRepresentationChanged() {
        const o = make();
        o.compactRepresentation = compactRepMock;
        compare(o.compactRepresentation, compactRepMock);
        // handler reparented the rep onto root and flipped both visibles.
        compare(compactRepMock.parent, o);
        compare(compactRepMock.visible, true);
        compare(o.visible, true);
        // width/height were bound to root's geometry.
        compare(compactRepMock.width, o.width);
        compare(compactRepMock.height, o.height);
        // originalCompactRepresenationParent captured the prior parent (tc).
        compare(o.originalCompactRepresenationParent, tc);
    }

    // Fire onFullRepresentationChanged: with implicitWidth/Height > 0 the handler
    // binds popupWindow.mainItem.width/height to those implicit sizes, and
    // reparents fullRepresentation into appletParent (the popup mainItem).
    function test_fullRepresentationChanged() {
        const o = make();
        o.fullRepresentation = fullRepMock;
        compare(o.fullRepresentation, fullRepMock);
        const popup = findPopup(o);
        verify(popup, "could not find popupWindow");
        // mainItem.width/height bound to fullRep.implicitWidth/Height (120/90).
        compare(popup.mainItem.width, 120);
        compare(popup.mainItem.height, 90);
        // fullRepresentation was reparented into the popup's mainItem.
        compare(fullRepMock.parent, popup.mainItem);
    }

    // Drive the expandedSync timer (interval 500): its onTriggered syncs
    // hostedApplet.expanded to popupWindow.visible (false at rest).
    function test_expandedSyncTimer() {
        const o = make();
        // Pre-set expanded true; the timer must overwrite it with popup.visible.
        appletMock.expanded = true;
        const t = findTimer(o, 500);
        verify(t, "could not find expandedSync timer");
        t.interval = 1;
        t.restart();
        tryVerify(function() { return !t.running; }, 1000);
        // popupWindow.visible is false at rest, so expanded synced to false.
        compare(appletMock.expanded, false);
    }

    // Fire the configure-action Connections onTriggered: it sets expanded=false.
    function test_configureTriggered() {
        const o = make();
        appletMock.expanded = true;
        configureActionMock.triggered();
        compare(appletMock.expanded, false);
    }

    // Fire the contextual-actions Connections: the handler calls root.hideToolTip().
    // Show the tooltip first so the hide produces an observable toolTipVisibleChanged.
    function test_contextualActions() {
        const o = make();
        ttVisibleSpy.target = o;
        ttVisibleSpy.signalName = "toolTipVisibleChanged";
        o.showToolTip();
        // showing the tooltip is the first transition.
        tryVerify(function() { return ttVisibleSpy.count >= 1; }, 2000, "tooltip never became visible");
        const afterShow = ttVisibleSpy.count;
        // Firing the contextual-actions signal must drive the handler -> hideToolTip,
        // which (delayed) fires toolTipVisibleChanged again.
        plasmoidMock.contextualActionsAboutToShow();
        tryVerify(function() { return ttVisibleSpy.count > afterShow; }, 2000,
                  "contextual-actions did not hide the tooltip");
    }

    // Drive the popupWindow onVisibleChanged through both arms. With a
    // fullRepresentation present, toggling expanded flips popup.visible:
    //   show arm  -> captures oldStatus, sets status = RequiresAttention (4)
    //   hide arm  -> restores status to the captured oldStatus
    function test_popupVisibleChanged() {
        const o = make();
        o.fullRepresentation = fullRepMock;
        const popup = findPopup(o);
        verify(popup, "could not find popupWindow");
        // Distinct non-zero status so the restore can't be a coincidental 0.
        plasmoidMock.status = 2;

        appletMock.expanded = true;
        tryVerify(function() { return popup.visible === true; }, 1000, "popup never shown");
        // show arm set status to RequiresAttentionStatus.
        compare(plasmoidMock.status, PlasmaCore.Types.RequiresAttentionStatus);

        appletMock.expanded = false;
        tryVerify(function() { return popup.visible === false; }, 1000, "popup never hid");
        // hide arm restored the captured oldStatus (2), not a hardcoded value.
        compare(plasmoidMock.status, 2);
    }

    // Force active focus on the popup's mainItem (appletParent) to fire
    // onActiveFocusChanged, which forwards focus to fullRepresentation.
    function test_activeFocus() {
        const o = make();
        o.fullRepresentation = fullRepMock;
        appletMock.expanded = true;
        const popup = findPopup(o);
        verify(popup, "could not find popupWindow");
        tryVerify(function() { return popup.visible === true; }, 1000, "popup never shown");
        popup.mainItem.forceActiveFocus();
        // The handler called fullRepresentation.forceActiveFocus().
        tryVerify(function() { return fullRepMock.activeFocus === true; }, 1000,
                  "focus was not forwarded to fullRepresentation");
    }

    // Touch the icon ability props that feed the Indicators API bindings and
    // assert the bound targets actually re-evaluate: root.opacity/rotation/scale
    // and the compactRepresentation anchor offsets.
    function test_indicatorBindings() {
        const o = make();
        o.compactRepresentation = compactRepMock;
        appletItemMock.iconOffsetX = 5;
        appletItemMock.iconOffsetY = 7;
        appletItemMock.iconOpacity = 0.5;
        appletItemMock.iconRotation = 10;
        appletItemMock.iconScale = 1.2;
        // root-level bindings.
        tryCompare(o, "opacity", 0.5);
        compare(o.rotation, 10);
        compare(o.scale, 1.2);
        // compactRepresentation anchor-offset bindings.
        compare(compactRepMock.anchors.horizontalCenterOffset, 5);
        compare(compactRepMock.anchors.verticalCenterOffset, 7);
    }

    // --- helpers --------------------------------------------------------------

    function findTimer(o, ms) {
        const res = o.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].interval === ms && typeof res[i].restart === "function")
                return res[i];
        }
        return null;
    }

    function findPopup(o) {
        const res = o.resources;
        for (var i = 0; i < res.length; i++) {
            if (res[i] && res[i].objectName === "popupWindow")
                return res[i];
        }
        return null;
    }
}
