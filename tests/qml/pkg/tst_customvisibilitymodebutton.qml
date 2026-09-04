// Coverage for the shell's CustomVisibilityModeButton, the visibility-mode
// picker in the settings dialog. It is a ComboBoxButton subclass whose handlers
// all funnel into latteView.visibility.mode, the only unqualified context name
// it reads — declared here as a document id so the dynamically created
// component resolves it up the creation-context chain. `modes` IS a declared
// property, so each instance receives a mock mode list as an initial value
// (pluginId stands in for the LatteCore.Types visibility ints; the component
// only compares them for equality).
import QtQuick
import QtTest
import Stage 1.0

TestCase {
    id: root
    name: "CustomVisibilityModeButton"
    when: windowShown
    // TestCase defaults to visible:false. The popup Connections' bare `visible`
    // resolves up the scope chain to the button's own visible (not the popup's),
    // which is true in the real dialog — mirror that here or the handler's
    // if(visible) is permanently false.
    visible: true
    width: 300
    height: 100

    readonly property url targetUrl: Stage.share("plasma/shells/org.kde.latte.shell/contents/controls/CustomVisibilityModeButton.qml")

    readonly property var testModes: [
        { name: "Windows Go Below", tooltip: "wgb", pluginId: 10 },
        { name: "Dodge Active", tooltip: "da", pluginId: 11 },
        { name: "Always Visible", tooltip: "av", pluginId: 12 }
    ]

    QtObject {
        id: latteView
        property QtObject visibility: QtObject {
            property int mode: 10
        }
    }

    SignalSpy {
        id: relevantSpy
        signalName: "viewRelevantVisibilityModeChanged"
    }

    function init() {
        latteView.visibility.mode = 10;
        relevantSpy.target = null;
        relevantSpy.clear();
    }

    function makeItem(mode) {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, { modes: testModes, mode: mode });
        verify(obj, "instantiate failed");
        return obj;
    }

    // The main button's clicked signal flows through the Connections onClicked
    // and writes the picked mode into the view.
    function test_buttonClicked_appliesMode() {
        const item = makeItem(12);
        compare(latteView.visibility.mode, 10);
        item.button.clicked();
        compare(latteView.visibility.mode, 12, "clicking the button must apply custom.mode to the view");
    }

    // The latteView.visibility Connections: a mode change that matches one of
    // this button's modes emits viewRelevantVisibilityModeChanged; an unknown
    // mode walks the whole list and emits nothing.
    function test_viewModeChange_signalsOnlyRelevantModes() {
        const item = makeItem(10);
        relevantSpy.target = item;
        relevantSpy.clear();

        latteView.visibility.mode = 11; // in testModes
        compare(relevantSpy.count, 1, "a relevant mode change must be signalled");

        latteView.visibility.mode = 99; // not in testModes
        compare(relevantSpy.count, 1, "an irrelevant mode change must not be signalled");
    }

    // The comboBox Connections onActivated(index): a row selection writes that
    // row's pluginId into the view; a negative index is guarded out.
    function test_comboActivated_writesModelPluginId() {
        const item = makeItem(10);
        item.comboBox.activated(2);
        compare(latteView.visibility.mode, 12, "activating row 2 must apply its pluginId");

        latteView.visibility.mode = 10;
        item.comboBox.activated(-1);
        compare(latteView.visibility.mode, 10, "a negative index must leave the mode untouched");
    }

    // emptyModel(): rebuilds the model from the defaults but deliberately drops
    // the selection, unlike reloadModel which re-selects the chosen type.
    function test_emptyModel_dropsSelection() {
        const item = makeItem(11);
        // Component.onCompleted -> reloadModel selected the chosen mode (row 1).
        compare(item.comboBox.currentIndex, 1);

        item.emptyModel();
        compare(item.comboBox.currentIndex, -1, "emptyModel must clear the selection");
        compare(item.comboBox.model.count, 3, "emptyModel re-appends all the defaults");
    }

    // The popup Connections onVisibleChanged: opening the popup re-selects the
    // chosen type so the highlighted row matches the current mode.
    function test_popupOpen_selectsChosenType() {
        const item = makeItem(12);
        item.comboBox.currentIndex = -1; // scramble so only the handler can fix it

        item.comboBox.popup.open();
        tryCompare(item.comboBox, "currentIndex", 2, 2000,
                   "opening the popup must re-select the chosen mode's row");
        item.comboBox.popup.close();
    }
}
