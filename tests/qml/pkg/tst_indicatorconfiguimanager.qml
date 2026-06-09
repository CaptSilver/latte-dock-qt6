// Drives the shell's IndicatorConfigUiManager through the three handlers that
// run honestly headless: hiddenIndicatorPage.Component.onCompleted and the two
// Connections (latteView.indicator.onPluginChanged, viewConfig.onIsReadyChanged).
// The component is loaded from the staged (instrumented) package by file URL so
// the Cov.tick calls fire, and every claimed unit asserts an observable effect:
// a mock side-effect recording the forwarded call with its arguments.
//
// The component reads these unqualified context names, which QML resolves
// against the creation context (this TestCase, id:root): viewConfig (with
// .indicatorUiManager and .isReady), latteView (with .indicator carrying .type
// and a pluginChanged signal), tabBar (selectTab), dialog (optionsWidth). It
// also reads its own `stackView` Item property in several bindings; we hand it a
// real Item mock with width/currentItem. Each name is shaped like the real
// object, not a catch-all.
//
// Live-only (reported, not faked): showNextIndicator@27. It is a method on the
// internal hiddenIndicatorPage that C++ (IndicatorUiManager) stores via
// setParentItem and invokes by metaobject; headless we hold only the root, the
// page has no parented indicator children, and there is no live stackView with
// replace()/forwardSliding, so both of its branches no-op and nothing is
// observable. It needs a live config dialog with real indicator config UIs.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "IndicatorConfigUiManager"
    when: windowShown

    // The C++ indicator-UI manager the component forwards into. Records the page
    // it was handed (setParentItem) and every ui(type, view) call so onCompleted
    // and the two Connections can be asserted. Not a catch-all: only the members
    // the target actually calls. index() exists because the binding context
    // references it (used by the live-only showNextIndicator path); it returns -1
    // like the empty real model.
    QtObject {
        id: indicatorUiManagerObj
        property var parentItem: null
        property int setParentItemCalls: 0
        property string lastUiType: ""
        property var lastUiView: null
        property int uiCalls: 0

        function setParentItem(item) { setParentItemCalls++; parentItem = item; }
        function ui(type, view) { uiCalls++; lastUiType = type; lastUiView = view; }
        function index(type) { return -1; }
    }

    QtObject {
        id: viewConfig
        property bool isReady: false
        property QtObject indicatorUiManager: indicatorUiManagerObj
    }

    // latteView.indicator is the Connections target for onPluginChanged and the
    // source of the indicator type read all over the file. Shaped with the
    // pluginChanged signal and a writable type string.
    QtObject {
        id: indicatorObj
        signal pluginChanged()
        property string type: "org.kde.latte.default"
    }

    QtObject {
        id: latteView
        property QtObject indicator: indicatorObj
    }

    // tabBar.selectTab(type) is called by onCompleted and both Connections.
    // Records the last requested tab + a call count so each handler is asserted.
    QtObject {
        id: tabBar
        property string lastTab: ""
        property int selectCalls: 0
        function selectTab(type) { selectCalls++; lastTab = type; }
    }

    // dialog.optionsWidth feeds the optionsWidth bindings on the page Items.
    QtObject {
        id: dialog
        property int optionsWidth: 240
    }

    // The component's own `property Item stackView` is read by the width /
    // nextPage / currentItem bindings. Give it a real Item with the members those
    // bindings touch.
    Component {
        id: stackViewComponent
        Item {
            width: 300
            property Item currentItem: null
            property bool forwardSliding: false
        }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/controls/IndicatorConfigUiManager.qml")

    function resetCounters() {
        indicatorUiManagerObj.setParentItemCalls = 0;
        indicatorUiManagerObj.uiCalls = 0;
        indicatorUiManagerObj.lastUiType = "";
        indicatorUiManagerObj.lastUiView = null;
        indicatorUiManagerObj.parentItem = null;
        tabBar.selectCalls = 0;
        tabBar.lastTab = "";
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const sv = createTemporaryObject(stackViewComponent, root, {});
        verify(sv, "stackView mock failed to instantiate");
        const obj = createTemporaryObject(c, root, {stackView: sv});
        verify(obj, "instantiate failed");
        return obj;
    }

    // hiddenIndicatorPage.Component.onCompleted unconditionally:
    //   viewConfig.indicatorUiManager.setParentItem(hiddenIndicatorPage)
    //   tabBar.selectTab(latteView.indicator.type)
    //   viewConfig.indicatorUiManager.ui(latteView.indicator.type, latteView)
    // Assert each landed with the right argument (the page is a child Item of the
    // root; the tab + ui type are the indicator type; the ui view is latteView).
    function test_onCompleted_forwardsParentTabAndUi() {
        resetCounters();
        viewConfig.isReady = false;
        indicatorObj.type = "org.kde.latte.default";

        const m = make();

        compare(indicatorUiManagerObj.setParentItemCalls, 1);
        // setParentItem was handed the internal hiddenIndicatorPage (an Item).
        verify(indicatorUiManagerObj.parentItem !== null);
        verify(indicatorUiManagerObj.parentItem instanceof Item);

        compare(tabBar.selectCalls, 1);
        compare(tabBar.lastTab, "org.kde.latte.default");

        compare(indicatorUiManagerObj.uiCalls, 1);
        compare(indicatorUiManagerObj.lastUiType, "org.kde.latte.default");
        compare(indicatorUiManagerObj.lastUiView, latteView);
    }

    // The Connections{ target: viewConfig } onIsReadyChanged handler runs its
    // body only when viewConfig.isReady is true. Build with isReady false, snap
    // the post-construction counts, flip to true, and assert the handler fired:
    // a fresh selectTab + ui for the current type beyond construction.
    function test_onIsReadyChanged_runsWhenReady() {
        resetCounters();
        viewConfig.isReady = false;
        indicatorObj.type = "org.kde.latte.plasma";
        const m = make();

        const tabsBefore = tabBar.selectCalls;
        const uiBefore = indicatorUiManagerObj.uiCalls;

        viewConfig.isReady = true;

        // handler fired its true-branch -> one more selectTab + ui for the type.
        compare(tabBar.selectCalls, tabsBefore + 1);
        compare(tabBar.lastTab, "org.kde.latte.plasma");
        compare(indicatorUiManagerObj.uiCalls, uiBefore + 1);
        compare(indicatorUiManagerObj.lastUiType, "org.kde.latte.plasma");
        compare(indicatorUiManagerObj.lastUiView, latteView);
    }

    // The same handler's guard: when isReady flips to false it ticks but the body
    // is skipped, so no further forwarding happens.
    function test_onIsReadyChanged_skipsWhenNotReady() {
        resetCounters();
        viewConfig.isReady = true;
        indicatorObj.type = "org.kde.latte.plasma";
        const m = make();

        const tabsBefore = tabBar.selectCalls;
        const uiBefore = indicatorUiManagerObj.uiCalls;

        viewConfig.isReady = false;

        // handler ran with the guard false -> no new calls.
        compare(tabBar.selectCalls, tabsBefore);
        compare(indicatorUiManagerObj.uiCalls, uiBefore);
    }

    // The Connections{ target: latteView.indicator } onPluginChanged handler.
    // With viewConfig.isReady true, emitting pluginChanged re-selects the tab and
    // re-builds the ui for the current indicator type. Assert the forwarded calls.
    function test_onPluginChanged_rebuildsWhenReady() {
        resetCounters();
        viewConfig.isReady = true;
        indicatorObj.type = "org.kde.latte.default";
        const m = make();

        const tabsBefore = tabBar.selectCalls;
        const uiBefore = indicatorUiManagerObj.uiCalls;

        indicatorObj.type = "org.kde.latte.custom";
        indicatorObj.pluginChanged();

        compare(tabBar.selectCalls, tabsBefore + 1);
        compare(tabBar.lastTab, "org.kde.latte.custom");
        compare(indicatorUiManagerObj.uiCalls, uiBefore + 1);
        compare(indicatorUiManagerObj.lastUiType, "org.kde.latte.custom");
        compare(indicatorUiManagerObj.lastUiView, latteView);
    }

    // onPluginChanged guard: not ready -> handler ticks but skips the body.
    function test_onPluginChanged_skipsWhenNotReady() {
        resetCounters();
        viewConfig.isReady = false;
        indicatorObj.type = "org.kde.latte.default";
        const m = make();

        const tabsBefore = tabBar.selectCalls;
        const uiBefore = indicatorUiManagerObj.uiCalls;

        indicatorObj.pluginChanged();

        compare(tabBar.selectCalls, tabsBefore);
        compare(indicatorUiManagerObj.uiCalls, uiBefore);
    }
}
