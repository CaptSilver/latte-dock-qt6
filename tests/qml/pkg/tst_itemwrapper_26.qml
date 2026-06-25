// Coverage for the containment ItemWrapper delegate, loaded from the staged
// (instrumented) package.
//
// ItemWrapper is an AppletItem child that resolves a pile of unqualified
// context names (root, appletItem, applet, communicator, appletColorizer,
// parabolic, latteView, visibilityManager, layoutsContainer, indicators, plus
// the separator flags) lexically against its creation context. QML resolves
// those names against THIS file's root object, so the TestCase is named
// `id: root` and every name the component reads is declared here as a property
// or lowercase-id'd QtObject shaped like the real thing. With the context
// supplied the public function and the property-change handlers run for real
// and we assert their observable side effects on the mocks.
import QtQuick
import QtQuick.Layouts
import QtTest

TestCase {
    id: root
    name: "ItemWrapper26"
    when: windowShown
    visible: true
    width: 400
    height: 400

    // ---- bare context names the component reads unqualified -------------------
    property bool isHorizontal: true
    property bool isSeparator: false
    property bool isMarginsAreaSeparator: false
    property bool isInternalViewSplitter: false
    property bool isSpacer: false
    property bool isHidden: false
    property bool inConfigureAppletsMode: false
    property bool inStartup: false
    property bool forceTransparentPanel: false
    property bool forcePanelForBusyBackground: false
    property int  minAppletLengthInConfigure: 16
    property int  maxJustifySplitterSize: 64
    property var  latteView: null
    property var  dragOverlay: QtObject {
        property QtObject currentApplet: QtObject { property bool isInternalViewSplitter: false }
        property QtObject draggedPlaceHolder: QtObject { property real length: 0 }
    }

    // ---- side-effect counters the mock methods bump so tests can assert -------
    property int parabolicSupportedCalls: 0
    property int autoFillCalls: 0
    property int directRenderingCalls: 0
    property int addEventCalls: 0
    property int removeEventCalls: 0

    // applet: an Item so the real attached Layout carries the minimum/preferred/
    // maximum size hints the component reads (applet.Layout.minimumWidth ...).
    // Changing width/height/Layout.* recomputes appletWidth/appletHeight and the
    // appletLength/appletThickness/appletMinimum*/appletPreferred* properties the
    // handlers watch.
    Item {
        id: appletMock
        width: 40
        height: 44
        property string pluginName: "org.kde.test"
        Layout.minimumWidth: 30
        Layout.minimumHeight: 30
        Layout.preferredWidth: 40
        Layout.preferredHeight: 44
        Layout.maximumWidth: 80
        Layout.maximumHeight: 80
        Layout.fillWidth: false
        Layout.fillHeight: false
    }
    property var applet: appletMock

    QtObject {
        id: parabolicFactor
        property real zoom: 1.6
        property real marginThicknessZoomInPercentage: 0.5
    }
    QtObject {
        id: parabolic
        property bool isEnabled: false
        property QtObject factor: parabolicFactor
    }

    QtObject {
        id: appletColorizer
        property bool mustBeShown: false
    }

    QtObject {
        id: indicators
        property QtObject info: QtObject { property bool needsIconColors: false }
    }

    QtObject {
        id: communicator
        property bool parabolicEffectIsSupported: false
        property bool indexerIsSupported: false
        property bool indexerIsSupportedFlag: false
        property bool inStartup: false
        property bool appletMainIconIsFound: false
        property var appletIconItem: null
        property var appletImageItem: null
        property QtObject requires: QtObject { property bool lengthMarginsEnabled: true }
    }

    QtObject {
        id: layoutsContainer
        property QtObject startLayout: QtObject {}
        property QtObject endLayout: QtObject {}
        property QtObject mainLayout: QtObject { property real length: 0 }
    }

    QtObject {
        id: visibilityManager
        property bool inRelocationHiding: false
    }

    // appletItem: the big one. parabolic/animations/metrics/myView/environment/
    // debug/layouter sub-objects plus the call-recording methods.
    QtObject {
        id: appletItemParabolic
        property real zoom: 1.6
        property bool directRenderingEnabled: false
        property bool isEnabled: false
        property QtObject factor: parabolicFactor
        function setDirectRenderingEnabled(v) { root.directRenderingCalls++; }
    }
    QtObject {
        id: appletItemNeedBothAxis
        function addEvent(o) { root.addEventCalls++; }
        function removeEvent(o) { root.removeEventCalls++; }
    }
    QtObject {
        id: appletItemAnimations
        property int animationTime: 0
        property QtObject duration: QtObject { property int large: 0; property int proposed: 0 }
        property QtObject needBothAxis: appletItemNeedBothAxis
    }
    QtObject {
        id: appletItemMetrics
        property int iconSize: 48
        property QtObject totals: QtObject {
            property real thickness: 44
            property int thicknessEdges: 4
        }
        property QtObject margin: QtObject {
            property int length: 2
            property int screenEdge: 0
            property int tailThickness: 1
            property int headThickness: 1
        }
        property QtObject marginsArea: QtObject {
            property int iconSize: 32
            property int thicknessEdges: 2
            property int tailThickness: 1
            property int headThickness: 1
        }
        property QtObject padding: QtObject { property int length: 2 }
    }
    QtObject {
        id: appletItemLayouter
        property bool maxMetricsInHigherPriority: false
        property real maxLength: 200
        property QtObject startLayout: QtObject { property real lengthWithoutSplitters: 0 }
        property QtObject endLayout: QtObject { property real lengthWithoutSplitters: 0 }
        function updateSizeForAppletsInFill() { root.autoFillCalls++; }
    }
    QtObject {
        id: appletItem
        property bool isScheduledForDestruction: false
        property bool isInternalViewSplitter: false
        property bool isMarginsAreaSeparator: false
        property bool isAutoFillApplet: false
        property bool inConfigureAppletsDragging: false
        property bool inMarginsArea: false
        property bool canFillThickness: false
        property bool canFillScreenEdge: false
        property bool isZoomed: false
        property bool indexerIsSupported: false
        property bool parabolicEffectIsSupported: false
        property bool originalAppletBehavior: false
        property bool lockZoom: false
        property real minAutoFillLength: 0
        property real maxAutoFillLength: -1
        property real lengthAppletFullMargins: 4
        property int  index: 3
        property int  animationTime: 0
        property color highlightColor: "white"
        property var  parent: null
        property var  applet: appletMock
        property QtObject parabolic: appletItemParabolic
        property QtObject animations: appletItemAnimations
        property QtObject metrics: appletItemMetrics
        property QtObject layouter: appletItemLayouter
        property QtObject communicator: communicator
        property QtObject myView: QtObject {
            property QtObject itemShadow: QtObject {
                property bool isEnabled: false
                property color shadowColor: "black"
                property int size: 4
            }
        }
        property QtObject environment: QtObject { property bool isGraphicsSystemAccelerated: false }
        property QtObject debug: QtObject {
            property bool graphicsEnabled: false
            property bool overloadedIconsEnabled: false
        }
        function updateParabolicEffectIsSupported() { root.parabolicSupportedCalls++; }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/applet/ItemWrapper.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // The delegate builds with its full context supplied, and its plain + computed
    // properties carry real values derived from the mocks.
    function test_a_construct_and_computed() {
        const w = make();
        verify(w !== null);
        // plain writable defaults
        compare(w.zoomScale, 1);
        compare(w.disableLengthScale, false);
        compare(w.disableThicknessScale, false);
        compare(w.marginsLength, 0);
        // appletWidth/appletHeight track the applet mock (40 x 44)
        compare(w.appletWidth, 40);
        compare(w.appletHeight, 44);
        // isHorizontal => length tracks width, thickness tracks height
        compare(w.appletLength, 40);
        compare(w.appletThickness, 44);
        // index aliases appletItem.index
        compare(w.index, 3);
        // zoomMarginScale = 1 + (zoomScale-1)*factor; at zoomScale 1 that is 1
        compare(w.zoomMarginScale, 1);
        // scale-disable flags off => the scale factors equal zoomScale (1)
        compare(w.zoomScaleLength, 1);
        compare(w.zoomScaleThickness, 1);
    }

    // appletMinimumLength/PreferredLength read the applet Layout (non-splitter,
    // horizontal => the *Width values).
    function test_b_minimum_preferred_length_from_layout() {
        const w = make();
        compare(w.appletMinimumLength, 30);
        compare(w.appletPreferredLength, 40);
        compare(w.appletMinimumThickness, 30);
    }

    // updateAutoFillLength(): only calls the layouter when isAutoFillApplet.
    function test_c_updateAutoFillLength_gated() {
        const w = make();
        // not an autofill applet -> no layouter call
        appletItem.isAutoFillApplet = false;
        const base = root.autoFillCalls;
        w.updateAutoFillLength();
        compare(root.autoFillCalls, base, "non-autofill applet must not size-fill");
        // autofill applet -> layouter.updateSizeForAppletsInFill() runs
        appletItem.isAutoFillApplet = true;
        w.updateAutoFillLength();
        compare(root.autoFillCalls, base + 1, "autofill applet must trigger size-fill");
        appletItem.isAutoFillApplet = false;
    }

    // onZoomScaleChanged: zoomScale defaults 1; animationTime 0 makes the Behavior
    // settle instantly. Driving it to the parabolic zoom value (>1) flips isZoomed
    // and enables direct rendering; driving back to 1 clears it.
    function test_d_onZoomScaleChanged_zoomIn() {
        const w = make();
        appletItem.isZoomed = false;
        const drBase = root.directRenderingCalls;
        const addBase = root.addEventCalls;
        // zoom up to the configured zoom factor (1.6)
        w.zoomScale = appletItemParabolic.zoom;
        tryVerify(function() { return appletItem.isZoomed === true; }, 2000,
                  "zooming past 1 must set isZoomed");
        // reaching exactly the zoom factor with directRendering off enables it
        verify(root.directRenderingCalls > drBase, "setDirectRenderingEnabled must fire at full zoom");
        // crossing >1 registers the both-axis animation event
        verify(root.addEventCalls > addBase, "zoom-in must add a both-axis event");
    }

    function test_e_onZoomScaleChanged_zoomOut() {
        const w = make();
        // start zoomed in
        appletItem.isZoomed = true;
        w.zoomScale = appletItemParabolic.zoom;
        wait(0);
        const removeBase = root.removeEventCalls;
        // back to rest -> isZoomed cleared, removeEvent fires
        w.zoomScale = 1.0;
        tryVerify(function() { return appletItem.isZoomed === false; }, 2000,
                  "returning to scale 1 must clear isZoomed");
        verify(root.removeEventCalls > removeBase, "zoom-out must remove the both-axis event");
    }

    // onAppletLengthChanged: appletLength = appletWidth (horizontal). Changing the
    // applet width recomputes it and, at zoomScale 1, calls
    // appletItem.updateParabolicEffectIsSupported().
    function test_f_onAppletLengthChanged() {
        const w = make();
        compare(w.zoomScale, 1);
        const base = root.parabolicSupportedCalls;
        appletMock.width = 72;       // -> appletWidth 72 -> appletLength changes
        compare(w.appletLength, 72);
        verify(root.parabolicSupportedCalls > base,
               "applet length change at rest must refresh parabolic support");
        appletMock.width = 40;
    }

    // onAppletThicknessChanged: appletThickness = appletHeight (horizontal).
    function test_g_onAppletThicknessChanged() {
        const w = make();
        const base = root.parabolicSupportedCalls;
        appletMock.height = 60;      // -> appletHeight 60 -> appletThickness changes
        compare(w.appletThickness, 60);
        verify(root.parabolicSupportedCalls > base,
               "applet thickness change at rest must refresh parabolic support");
        appletMock.height = 44;
    }

    // onAppletMinimumLengthChanged: fires updateParabolicEffectIsSupported (zoom 1)
    // AND updateAutoFillLength. Drive via the applet Layout minimumWidth.
    function test_h_onAppletMinimumLengthChanged() {
        const w = make();
        appletItem.isAutoFillApplet = true;   // so updateAutoFillLength reaches the layouter
        const pBase = root.parabolicSupportedCalls;
        const aBase = root.autoFillCalls;
        appletMock.Layout.minimumWidth = 55;  // -> appletMinimumWidth -> appletMinimumLength
        compare(w.appletMinimumLength, 55);
        verify(root.parabolicSupportedCalls > pBase,
               "minimum-length change at rest must refresh parabolic support");
        verify(root.autoFillCalls > aBase,
               "minimum-length change must re-run autofill sizing");
        appletMock.Layout.minimumWidth = 30;
        appletItem.isAutoFillApplet = false;
    }

    // onAppletMinimumThicknessChanged: calls updateParabolicEffectIsSupported at
    // rest. appletMinimumThickness = appletMinimumHeight (horizontal).
    function test_i_onAppletMinimumThicknessChanged() {
        const w = make();
        const base = root.parabolicSupportedCalls;
        appletMock.Layout.minimumHeight = 50; // -> appletMinimumHeight -> appletMinimumThickness
        compare(w.appletMinimumThickness, 50);
        verify(root.parabolicSupportedCalls > base,
               "minimum-thickness change at rest must refresh parabolic support");
        appletMock.Layout.minimumHeight = 30;
    }

    // onAppletPreferredLengthChanged: calls updateAutoFillLength only.
    function test_j_onAppletPreferredLengthChanged() {
        const w = make();
        appletItem.isAutoFillApplet = true;
        const base = root.autoFillCalls;
        appletMock.Layout.preferredWidth = 90; // -> appletPreferredWidth -> appletPreferredLength
        compare(w.appletPreferredLength, 90);
        verify(root.autoFillCalls > base,
               "preferred-length change must re-run autofill sizing");
        appletMock.Layout.preferredWidth = 40;
        appletItem.isAutoFillApplet = false;
    }

    // onAppletMaximumLengthChanged: the non-splitter branch resolves to the applet's
    // Layout maximum (horizontal => maximumWidth = 80) -- a finite number. (A QML
    // script binding yields its last expression even without an explicit `return`,
    // so this stays a real value, not NaN; the explicit return in the source just
    // makes that robust against a later trailing statement.) Flipping
    // isInternalViewSplitter drives it to Infinity, which fires the handler ->
    // updateAutoFillLength.
    function test_k_onAppletMaximumLengthChanged() {
        const w = make();
        appletItem.isAutoFillApplet = true;
        root.isInternalViewSplitter = false;
        // The non-splitter maximum length is the finite Layout.maximumWidth, never NaN.
        verify(!isNaN(w.appletMaximumLength), "appletMaximumLength must not be NaN");
        compare(w.appletMaximumLength, 80);
        const base = root.autoFillCalls;
        root.isInternalViewSplitter = true;   // appletMaximumLength -> Infinity
        compare(w.appletMaximumLength, Infinity);
        verify(root.autoFillCalls > base,
               "maximum-length change must re-run autofill sizing");
        root.isInternalViewSplitter = false;
        appletItem.isAutoFillApplet = false;
    }

    // The scale-disable flags are plain writable bools; setting them rebinds
    // zoomScaleLength/zoomScaleThickness to 1 regardless of zoomScale.
    function test_l_scale_disable_flags() {
        const w = make();
        // force a zoom so the difference from 1 is observable
        appletItem.animationTime = 0;
        w.zoomScale = 1.5;
        wait(0);
        w.disableLengthScale = true;
        w.disableThicknessScale = true;
        compare(w.zoomScaleLength, 1, "disabled length scale pins factor to 1");
        compare(w.zoomScaleThickness, 1, "disabled thickness scale pins factor to 1");
        w.disableLengthScale = false;
        w.disableThicknessScale = false;
        // re-enabled, the factors track zoomScale again
        compare(w.zoomScaleLength, w.zoomScale);
        compare(w.zoomScaleThickness, w.zoomScale);
        w.zoomScale = 1.0;
    }
}
