// Coverage for the containment's IndicatorsPrivate ability. The component extends
// AbilityHost.Indicators (so isEnabled and the IndicatorInfo `info` block are real
// inherited members) and adds a `view` slot plus the View::Indicator bridge: two
// Connections that push info.svgPaths into view.indicator.resources when enabled, and a
// stack of Bindings that forward info.* into view.indicator(.info).* when view.indicator
// is live. The component reads no external context names — only its own `view` property
// and the inherited `info`/`isEnabled` — so the honest mock is just the `view` object,
// shaped like the real View with an `indicator`, `indicator.info` and
// `indicator.resources.setSvgImagePaths`.
//
// Loaded from the staged (instrumented) package by file URL so each unit fires a Cov tick.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "IndicatorsPrivate"
    when: windowShown

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.containment/contents/ui/abilities/privates/IndicatorsPrivate.qml")

    // Mock View::Indicator::Info — the writable forwarding target for the info.* Bindings.
    // Properties start at sentinel values opposite the source so a successful forward is
    // observable.
    Component {
        id: indicatorInfoComp
        QtObject {
            property bool needsIconColors: true
            property bool needsMouseEventCoordinates: true
            property bool providesClickedAnimation: true
            property bool providesHoveredAnimation: true
            property bool providesInAttentionAnimation: true
            property bool providesTaskLauncherAnimation: true
            property bool providesGroupedWindowAddedAnimation: true
            property bool providesGroupedWindowRemovedAnimation: true
            property bool providesFrontLayer: true
            property int extraMaskThickness: -1
            property real minLengthPadding: -1
            property real minThicknessPadding: -1
        }
    }

    // Mock View::Indicator::resources — records the last svgPaths handed to
    // setSvgImagePaths so the Connections handlers' side-effect is observable.
    Component {
        id: resourcesComp
        QtObject {
            property var lastSvgPaths: undefined
            property int setCalls: 0
            function setSvgImagePaths(paths) {
                lastSvgPaths = paths;
                setCalls += 1;
            }
        }
    }

    // Mock View::Indicator — carries enabledForApplets (forwarded directly onto it), plus
    // its info and resources children.
    Component {
        id: indicatorComp
        QtObject {
            property bool enabledForApplets: false
            property QtObject info
            property QtObject resources
        }
    }

    // Mock View — only `indicator` is read by the component.
    Component {
        id: viewComp
        QtObject {
            property QtObject indicator
        }
    }

    function makeView() {
        const ind = createTemporaryObject(indicatorComp, root);
        ind.info = createTemporaryObject(indicatorInfoComp, root);
        ind.resources = createTemporaryObject(resourcesComp, root);
        const v = createTemporaryObject(viewComp, root);
        v.indicator = ind;
        return v;
    }

    function make(props) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, props || {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // view=null keeps every Binding's target null (the `view && view.indicator` guard) and
    // leaves the Connections handlers inert (isEnabled defaults false). The inherited
    // IndicatorInfo defaults must load and the readonly `padding` derive from them.
    function test_hostDefaults_viewNull() {
        const m = make({view: null});
        compare(m.view, null);
        compare(m.isEnabled, false);
        verify(m.info, "info block missing");
        compare(m.info.enabledForApplets, true);
        compare(m.info.needsIconColors, false);
        compare(m.info.extraMaskThickness, 0);
        // padding = max(minLengthPadding, lengthPadding) = max(0, 0.08)
        compare(m.padding, 0.08);
    }

    // With a live view.indicator, the info.* Bindings forward each source value onto
    // view.indicator / view.indicator.info, overwriting the opposite sentinels.
    function test_bindings_forward_info_to_view() {
        const v = makeView();
        const m = make({view: v});

        // enabledForApplets forwards onto view.indicator itself.
        compare(v.indicator.enabledForApplets, m.info.enabledForApplets);
        compare(v.indicator.enabledForApplets, true);

        // the rest forward onto view.indicator.info.
        const vi = v.indicator.info;
        compare(vi.needsIconColors, m.info.needsIconColors);
        compare(vi.needsMouseEventCoordinates, m.info.needsMouseEventCoordinates);
        compare(vi.providesClickedAnimation, m.info.providesClickedAnimation);
        compare(vi.providesHoveredAnimation, m.info.providesHoveredAnimation);
        compare(vi.providesInAttentionAnimation, m.info.providesInAttentionAnimation);
        compare(vi.providesTaskLauncherAnimation, m.info.providesTaskLauncherAnimation);
        compare(vi.providesGroupedWindowAddedAnimation, m.info.providesGroupedWindowAddedAnimation);
        compare(vi.providesGroupedWindowRemovedAnimation, m.info.providesGroupedWindowRemovedAnimation);
        compare(vi.providesFrontLayer, m.info.providesFrontLayer);
        compare(vi.extraMaskThickness, m.info.extraMaskThickness);
        compare(vi.minLengthPadding, m.info.minLengthPadding);
        compare(vi.minThicknessPadding, m.info.minThicknessPadding);
    }

    // Bindings are live: mutate a source info.* value and the forwarded view value tracks.
    function test_bindings_track_source_changes() {
        const v = makeView();
        const m = make({view: v});
        const vi = v.indicator.info;

        m.info.needsIconColors = true;
        compare(vi.needsIconColors, true);
        m.info.needsIconColors = false;
        compare(vi.needsIconColors, false);

        m.info.extraMaskThickness = 7;
        compare(vi.extraMaskThickness, 7);

        m.info.enabledForApplets = false;
        compare(v.indicator.enabledForApplets, false);
    }

    // onIsEnabledChanged: flipping isEnabled true with a live view pushes the current
    // info.svgPaths through view.indicator.resources.setSvgImagePaths.
    function test_onIsEnabledChanged_pushesSvgPaths() {
        const v = makeView();
        const m = make({view: v});
        const res = v.indicator.resources;

        m.info.svgPaths = ["/a/on.svg", "/a/off.svg"];
        const before = res.setCalls;

        m.isEnabled = true;
        verify(res.setCalls > before, "setSvgImagePaths not called on enable");
        compare(res.lastSvgPaths, ["/a/on.svg", "/a/off.svg"]);
    }

    // onSvgPathsChanged: once enabled, changing info.svgPaths re-pushes the new paths.
    function test_onSvgPathsChanged_pushesNewPaths() {
        const v = makeView();
        const m = make({view: v});
        const res = v.indicator.resources;

        m.isEnabled = true;
        m.info.svgPaths = ["/b/first.svg"];
        const callsAfterFirst = res.setCalls;
        compare(res.lastSvgPaths, ["/b/first.svg"]);

        m.info.svgPaths = ["/b/second.svg", "/b/third.svg"];
        verify(res.setCalls > callsAfterFirst, "svgPaths change did not re-push");
        compare(res.lastSvgPaths, ["/b/second.svg", "/b/third.svg"]);
    }

    // onSvgPathsChanged guard: while disabled, changing info.svgPaths must NOT push (the
    // handler's isEnabled guard short-circuits).
    function test_svgPathsChange_whileDisabled_noPush() {
        const v = makeView();
        const m = make({view: v});
        const res = v.indicator.resources;

        compare(m.isEnabled, false);
        const before = res.setCalls;
        m.info.svgPaths = ["/c/ignored.svg"];
        compare(res.setCalls, before);
    }
}
