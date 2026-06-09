// Coverage spike: drive the shell's CustomIndicatorButton control through its
// public functions and signal handlers. The control is a ComboBoxButton
// subclass that reads two undefined context globals -- `latteView` and
// `viewConfig` -- plus i18n. Those can't be passed as initial properties
// (they're not declared on the component), so the target is loaded inside a
// wrapper Item that declares them as properties; QML resolves the bare names
// up the component-context chain into the wrapper. A Loader sourced by the
// staged (instrumented) file URL gives us the live object whose every executed
// function/handler fires a Cov tick.
import QtQuick
import QtTest

TestCase {
    id: tc
    name: "CustomIndicatorButton"
    when: windowShown
    width: 300
    height: 100

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/controls/CustomIndicatorButton.qml")

    // Build the wrapper host inline so the wrapper scope is in effect when the
    // staged control is created inside the Loader; latteView/viewConfig then
    // resolve to host's own properties.
    function makeHost(pluginsCount, ids, names, localIds, customType, indType) {
        const qml =
            'import QtQuick\n' +
            'Item {\n' +
            '  id: host\n' +
            '  property QtObject uiMgr: QtObject {\n' +
            '    property int added: 0\n' +
            '    property int downloaded: 0\n' +
            '    property var removed: []\n' +
            '    function addIndicator() { added++; }\n' +
            '    function downloadIndicator() { downloaded++; }\n' +
            '    function removeIndicator(id) { var a = removed; a.push(id); removed = a; }\n' +
            '  }\n' +
            '  property QtObject viewConfig: QtObject {\n' +
            '    property bool isReady: false\n' +
            '    property QtObject indicatorUiManager: host.uiMgr\n' +
            '  }\n' +
            '  property QtObject latteView: QtObject {\n' +
            '    property QtObject indicator: QtObject {\n' +
            '      property int customPluginsCount: ' + pluginsCount + '\n' +
            '      property string type: "' + indType + '"\n' +
            '      property string customType: "' + customType + '"\n' +
            '      property var customPluginIds: ' + JSON.stringify(ids) + '\n' +
            '      property var customPluginNames: ' + JSON.stringify(names) + '\n' +
            '      property var customLocalPluginIds: ' + JSON.stringify(localIds) + '\n' +
            '    }\n' +
            '  }\n' +
            '  property var item: ld.item\n' +
            '  property int loaderStatus: ld.status\n' +
            '  Loader { id: ld; source: "' + targetUrl + '" }\n' +
            '}\n';
        const host = Qt.createQmlObject(qml, tc, "hostInline");
        verify(host, "host create failed");
        verify(host.item, "loader item null: status=" + host.loaderStatus);
        return host;
    }

    // No plugins: Component.onCompleted -> reloadModel (empty branch) +
    // appendDefaults, then updateButtonInformation -> "install:" branch.
    function test_emptyState() {
        const host = makeHost(0, [], [], [], "", "");
        const item = host.item;
        compare(item.type, "install:");
        compare(item.checkable, false);
        compare(item.buttonIsTriggeringMenu, true);
        compare(item.comboBoxButtonIsVisible, false);
        // emptyModel(): clears + appendDefaults again. Only add:/download:
        // remain, so the model holds exactly the two defaults.
        item.emptyModel();
        compare(item.comboBox.currentIndex, -1);
        compare(item.comboBox.model.count, 2);
        // onButtonIsPressed with type == "install:" -> early-out: indicator.type
        // is left untouched.
        host.latteView.indicator.type = "sentinel";
        item.onButtonIsPressed();
        compare(host.latteView.indicator.type, "sentinel");
        host.destroy();
    }

    // Plugins present: reloadModel builds the model (including the removable /
    // local branch), updateButtonInformation takes the else branch and picks
    // the current custom plugin by index.
    function test_populatedState() {
        const ids = ["org.foo.a", "org.bar.b", "org.baz.c"];
        const names = ["Alpha", "Beta", "Gamma"];
        const local = ["org.bar.b"]; // b is removable
        const host = makeHost(3, ids, names, local, "org.bar.b", "org.foo.a");
        const item = host.item;
        compare(item.comboBoxButtonIsVisible, true);
        compare(item.checkable, true);
        // customType "org.bar.b" -> index 1 -> buttonText "Beta", type "org.bar.b".
        compare(item.type, "org.bar.b");
        compare(item.buttonText, "Beta");

        // reloadModel again exercises the build loop (3 plugins + add:/download:
        // = 5 rows). custom.type "org.bar.b" != indicator.type "org.foo.a", so
        // it takes the currentIndex = -1 branch, not selectChosenType.
        item.reloadModel();
        compare(item.comboBox.model.count, 5);
        compare(item.comboBox.currentIndex, -1);

        // onButtonIsPressed with a non-install type writes indicator.type.
        item.onButtonIsPressed();
        compare(host.latteView.indicator.type, "org.bar.b");
        host.destroy();
    }

    // updateButtonInformation fallback: customType not in the list -> index 0.
    function test_fallbackToFirst() {
        const ids = ["org.foo.a", "org.bar.b"];
        const names = ["Alpha", "Beta"];
        const host = makeHost(2, ids, names, [], "org.missing.x", "org.foo.a");
        const item = host.item;
        // curCustomIndex < 0 -> falls back to actionsModel.get(0).
        compare(item.buttonText, "Alpha");
        compare(item.type, "org.foo.a");
        // selectChosenType found path: type matches indicator.type "org.foo.a".
        item.selectChosenType();
        compare(item.comboBox.currentIndex, 0);
        // selectChosenType not-found path: flip type to something absent.
        item.type = "nope:";
        item.selectChosenType();
        compare(item.comboBox.currentIndex, -1);
        host.destroy();
    }

    // The comboBox.activated signal handler: add / download / select branches.
    function test_comboBoxActivated() {
        const ids = ["org.foo.a", "org.bar.b"];
        const names = ["Alpha", "Beta"];
        const host = makeHost(2, ids, names, ["org.bar.b"], "org.foo.a", "org.foo.a");
        const item = host.item;
        const combo = item.comboBox;
        verify(combo, "comboBox missing");

        // Model layout after reloadModel(): [a, b, add:, download:].
        // index of "add:" = 2, "download:" = 3, plugin "b" = 1.
        const addIdx = 2, dlIdx = 3, selIdx = 1;

        combo.activated(addIdx);
        compare(host.uiMgr.added, 1);

        combo.activated(dlIdx);
        compare(host.uiMgr.downloaded, 1);

        // Selecting a real plugin sets indicator.type.
        combo.activated(selIdx);
        compare(host.latteView.indicator.type, "org.bar.b");

        // Negative index -> the if(index>=0) guard is skipped (no add/download/
        // select), but updateButtonInformation still runs and re-derives the
        // button from customType "org.foo.a" (index 0).
        const addedBefore = host.uiMgr.added;
        const dlBefore = host.uiMgr.downloaded;
        combo.activated(-1);
        compare(host.uiMgr.added, addedBefore);
        compare(host.uiMgr.downloaded, dlBefore);
        compare(item.type, "org.foo.a");
        compare(item.buttonText, "Alpha");
        host.destroy();
    }

    // iconClicked: a local/removable plugin triggers removeIndicator + popup
    // close; a non-removable one is a no-op.
    function test_iconClicked() {
        const ids = ["org.foo.a", "org.bar.b"];
        const names = ["Alpha", "Beta"];
        const host = makeHost(2, ids, names, ["org.bar.b"], "org.foo.a", "org.foo.a");
        const item = host.item;
        const combo = item.comboBox;

        // index 1 == "org.bar.b" which is in customLocalPluginIds -> remove.
        combo.iconClicked(1);
        compare(host.uiMgr.removed.length, 1);
        compare(host.uiMgr.removed[0], "org.bar.b");

        // index 0 == "org.foo.a", not local -> no remove.
        combo.iconClicked(0);
        compare(host.uiMgr.removed.length, 1);

        // negative -> the if(index>=0) guard skips the body, so no remove fires.
        combo.iconClicked(-1);
        compare(host.uiMgr.removed.length, 1);
        host.destroy();
    }

    // The button's clicked signal flows through the Connections onClicked ->
    // onButtonIsPressed.
    function test_buttonClicked() {
        const ids = ["org.foo.a"];
        const names = ["Alpha"];
        const host = makeHost(1, ids, names, [], "org.foo.a", "org.foo.a");
        const item = host.item;
        verify(item.button, "button missing");
        host.latteView.indicator.type = "stale";
        item.button.clicked();
        compare(host.latteView.indicator.type, "org.foo.a");
        host.destroy();
    }

    // customPluginsCountChanged -> reloadModel + updateButtonInformation.
    // Flip the count and check the install: state collapses to a populated one.
    function test_pluginsCountChanged() {
        const host = makeHost(0, [], [], [], "", "");
        const item = host.item;
        compare(item.type, "install:");

        const ind = host.latteView.indicator;
        ind.customPluginIds = ["org.new.x"];
        ind.customPluginNames = ["NewX"];
        ind.customLocalPluginIds = [];
        ind.customType = "org.new.x";
        ind.customPluginsCount = 1; // emits customPluginsCountChanged

        tryVerify(function() { return item.type === "org.new.x"; }, 2000,
                  "count change did not refresh the button");
        compare(item.buttonText, "NewX");
        host.destroy();
    }

    // viewConfig.isReadyChanged with isReady true -> updateButtonInformation.
    function test_viewConfigReady() {
        const host = makeHost(1, ["org.foo.a"], ["Alpha"], [], "org.foo.a", "org.foo.a");
        const item = host.item;
        item.buttonText = "scratch";
        host.viewConfig.isReady = true; // emits isReadyChanged
        tryVerify(function() { return item.buttonText === "Alpha"; }, 2000,
                  "isReady did not refresh the button");
        host.destroy();
    }
}
