// Coverage for the dock settings dialog (LatteDockConfiguration.qml).
//
// The file's root is a Loader whose `active: plasmoid && plasmoid.configuration
// && latteView`. We supply those three ambient objects (plus universalSettings,
// viewConfig, layoutsManager) as id'd objects in this file's scope, shaped like
// the real C++ types, so the Loader goes active and instantiates the inner
// FocusScope dialog inside this creation context. The dialog's free identifiers
// resolve against our mocks, its construction handlers and functions run for
// real, and we then call the public functions / fire the signals directly and
// assert the observable effect each one produces.
//
// Live-only (not claimed here, see tests/coverage/live-only.md): the actions
// combo's onActivated handler (manual activated() emit on the Templates.ComboBox
// does not reach the Connections), the tab buttons' onCheckedChanged page swaps
// (the StackView never settles offscreen because the child config pages throw on
// their own ambient reads, so depth stays 0), and the advanced-label MouseArea
// onClicked (not uniquely locatable among ~40 sibling MouseAreas).
import QtQuick
import QtTest

TestCase {
    id: root
    name: "LatteDockConfiguration14"
    when: windowShown
    visible: false
    width: 600
    height: 700

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/shells/org.kde.latte.shell/contents/configuration/LatteDockConfiguration.qml")

    // ---- recorded side-effects --------------------------------------------
    property int setStickerCalls: 0
    property var lastSetSticker: null
    property int newViewCalls: 0
    property var lastNewViewTemplate: null
    property int duplicateViewCalls: 0
    property int hideConfigCalls: 0

    // Mutable backing for the scale getters so updateScales() re-reads are observable.
    property real wScale: 0.9
    property real hScale: 0.8

    // ---- fake ambient context (ids match the dialog's free identifiers) ----

    QtObject {
        id: geometryObj
        property real width: 1920
        property real height: 1080
    }

    QtObject {
        id: positioner
        property string currentScreenName: "DP-1"
        property var canvasGeometry: geometryObj
    }

    QtObject {
        id: extendedInterface
        // Repeater models accept 0 as "no items"; keep it integer so the tasks
        // repeaters build zero rows rather than choking on a null model.
        property int latteTasksModel: 0
    }

    QtObject {
        id: latteView
        property var positioner: positioner
        property var screenGeometry: geometryObj
        property var extendedInterface: extendedInterface
        property int type: 0                 // LatteCore.Types.DockView == 0
        property bool behaveAsPlasmaPanel: false
        property bool screenEdgeMarginEnabled: false
        property int screenEdgeMargin: 0
        property int maxNormalThickness: 36
        function newView(templateId) { root.newViewCalls++; root.lastNewViewTemplate = templateId; }
        function duplicateView() { root.duplicateViewCalls++; }
        function removeView() {}
        // `type` is a plain property; assigning it auto-emits typeChanged, which
        // the dialog's Connections{ target: latteView; onTypeChanged } listens to.
    }

    QtObject {
        id: configuration
        property bool configurationSticker: false
    }

    QtObject {
        id: plasmoid
        property var configuration: configuration
        property int formFactor: 2           // PlasmaCore.Types.Horizontal == 2
    }

    QtObject {
        id: availableGeom
        property real height: 1000
        property real width: 1900
    }

    QtObject {
        id: viewConfig
        property var availableScreenGeometry: availableGeom
        property int enabledBorders: 0
        property bool isReady: false
        function syncGeometry() {}
        function updateEffects() {}
        function setSticker(value) { root.setStickerCalls++; root.lastSetSticker = value; }
        function hideConfigWindow() { root.hideConfigCalls++; }
    }

    QtObject {
        id: universalSettings
        property bool inAdvancedModeForEditSettings: false
        property bool inConfigureAppletsMode: false
        function screenWidthScale(name) { return root.wScale; }
        function screenHeightScale(name) { return root.hScale; }
        function trademarkPath() { return ""; }
        function trademarkIconPath() { return ""; }
    }

    // viewTemplateIds/Names drive updateModel()'s append loop.
    QtObject {
        id: layoutsManager
        function viewTemplateIds() { return ["tpl-a", "tpl-b"]; }
        function viewTemplateNames() { return ["Template A", "Template B"]; }
        signal viewTemplatesChanged()
    }

    // ---- helpers ----------------------------------------------------------

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const loader = createTemporaryObject(c, root, { width: 580, height: 660, visible: true });
        verify(loader, "instantiate failed: " + c.errorString());
        verify(loader.active, "Loader should be active given the mocked globals");
        verify(loader.item !== null, "inner dialog should instantiate; got null");
        return loader;
    }

    function collectAll(start) {
        const out = [];
        const stack = [start];
        const seen = [];
        while (stack.length) {
            const o = stack.pop();
            if (!o || seen.indexOf(o) !== -1)
                continue;
            seen.push(o);
            out.push(o);
            const kids = o.children;
            if (kids !== undefined && kids !== null) {
                for (var i = 0; i < kids.length; i++)
                    stack.push(kids[i]);
            }
            const res = o.resources;
            if (res !== undefined && res !== null) {
                for (var j = 0; j < res.length; j++)
                    stack.push(res[j]);
            }
            if (o.item !== undefined && o.item !== null)
                stack.push(o.item);
            if (o.contentItem !== undefined && o.contentItem !== null)
                stack.push(o.contentItem);
        }
        return out;
    }

    function findOne(all, pred) {
        for (var i = 0; i < all.length; i++) {
            if (all[i] && pred(all[i]))
                return all[i];
        }
        return null;
    }

    function findCombo(all) {
        return findOne(all, function (o) {
            return typeof o.updateModel === "function"
                && typeof o.emptyModel === "function"
                && typeof o.updateDuplicateText === "function";
        });
    }

    // ---- actions combo: updateModel / emptyModel / updateDuplicateText -----
    function test_actionsModelHelpers() {
        const loader = make();
        const all = collectAll(loader);
        const combo = findCombo(all);
        verify(combo, "actionsComboBtn not found in tree");

        // updateModel(): duplicate + separator + one row per template id (2) = 4.
        combo.updateModel();
        const model = combo.comboBox.model;
        compare(model.count, 4, "updateModel should build duplicate + separator + 2 templates");
        // Templates are appended in reverse, so the last id ends up first.
        compare(model.get(0).actionId, "duplicate:");
        compare(model.get(1).isSeparator, true);
        compare(model.get(2).templateId, "tpl-b");
        compare(model.get(3).templateId, "tpl-a");
        // updateDuplicateText (chained from updateModel) filled the duplicate name.
        verify(model.get(0).name.length > 0, "duplicate row name should be set");

        // emptyModel(): clears the whole list.
        combo.emptyModel();
        compare(model.count, 0, "emptyModel should clear the model");
    }

    function test_updateDuplicateTextSwitchesOnViewType() {
        const loader = make();
        const all = collectAll(loader);
        const combo = findCombo(all);
        verify(combo);

        latteView.type = 0; // DockView
        combo.updateModel();
        const model = combo.comboBox.model;
        const dockName = model.get(0).name;
        verify(dockName.length > 0);

        // PanelView: updateDuplicateText rewrites the duplicate row to the panel text.
        latteView.type = 1;
        combo.updateDuplicateText();
        const panelName = model.get(0).name;
        verify(panelName.length > 0);
        verify(panelName !== dockName, "duplicate text should differ between dock and panel");
    }

    // onEnabledChanged of the combobox: enabled -> updateModel(); disabling ->
    // emptyModel(). Re-enabling after an empty rebuilds the model.
    function test_comboEnabledRebuildsModel() {
        const loader = make();
        const all = collectAll(loader);
        const combo = findCombo(all);
        verify(combo);
        const model = combo.comboBox.model;

        combo.comboBox.enabled = false;
        combo.emptyModel();                 // force-empty so the rebuild is observable
        compare(model.count, 0);
        combo.comboBox.enabled = true;      // onEnabledChanged -> updateModel()
        compare(model.count, 4, "re-enabling the combo should rebuild the actions model");
    }

    // Connections{ target: layoutsManager; onViewTemplatesChanged: updateModel() }
    // and Connections{ target: latteView; onTypeChanged: updateDuplicateText() }.
    function test_externalSignalsDriveModel() {
        const loader = make();
        const all = collectAll(loader);
        const combo = findCombo(all);
        verify(combo);
        const model = combo.comboBox.model;

        combo.emptyModel();
        compare(model.count, 0);
        layoutsManager.viewTemplatesChanged();
        compare(model.count, 4, "viewTemplatesChanged should rebuild the model");

        latteView.type = 0;
        combo.updateModel();
        const nameDock = model.get(0).name;
        latteView.type = 1;                 // emits typeChanged -> updateDuplicateText
        const namePanel = model.get(0).name;
        verify(namePanel !== nameDock, "typeChanged should rewrite the duplicate row text");
    }

    // viewConfig.isReady flip drives onIsReadyChanged: the actions combo
    // updateModel() runs again.
    function test_isReadyRebuildsModel() {
        const loader = make();
        const all = collectAll(loader);
        const combo = findCombo(all);
        verify(combo);
        combo.emptyModel();
        compare(combo.comboBox.model.count, 0);

        viewConfig.isReady = false;
        viewConfig.isReady = true;          // emits isReadyChanged -> updateModel()
        compare(combo.comboBox.model.count, 4, "isReady true should rebuild the actions model");
    }

    // The dialog's updateScales() reads the two universalSettings scale getters
    // and assigns them to userScaleWidth / userScaleHeight.
    function test_updateScales() {
        const loader = make();
        const dialog = loader.item;
        // Component.onCompleted already ran updateScales once; assert it landed.
        compare(dialog.userScaleWidth, 0.9);
        compare(dialog.userScaleHeight, 0.8);

        // Change the backing values and call again to prove it re-reads them.
        root.wScale = 0.5;
        root.hScale = 0.4;
        dialog.updateScales();
        compare(dialog.userScaleWidth, 0.5);
        compare(dialog.userScaleHeight, 0.4);
    }

    // colorBrightnessFromRGB(r,g,b) = (r*299 + g*587 + b*114) / 1000, and
    // colorBrightness(color) feeds it color.r*255, color.g*255, color.b*255.
    function test_colorBrightnessHelpers() {
        const loader = make();
        const all = collectAll(loader);
        const lbl = findOne(all, function (o) {
            return typeof o.colorBrightness === "function"
                && typeof o.colorBrightnessFromRGB === "function";
        });
        verify(lbl, "advanced label with brightness helpers not found");

        // Pure white -> (255*299 + 255*587 + 255*114)/1000 = 255.
        fuzzyCompare(lbl.colorBrightnessFromRGB(255, 255, 255), 255, 0.001);
        // Single red channel -> 255*299/1000 = 76.245.
        fuzzyCompare(lbl.colorBrightnessFromRGB(255, 0, 0), 76.245, 0.001);

        // colorBrightness(white) routes through the same formula.
        fuzzyCompare(lbl.colorBrightness(Qt.rgba(1, 1, 1, 1)), 255, 0.001);
        // colorBrightness(green) -> 587*255/1000 / ... i.e. 149.685.
        fuzzyCompare(lbl.colorBrightness(Qt.rgba(0, 1, 0, 1)), 149.685, 0.001);
    }

    // pinButton.Component.onCompleted seeds the toggle from the saved sticker
    // state and pushes it back through viewConfig.setSticker(). Constructing with
    // the sticker on should leave the button checked and record the setSticker call.
    function test_pinButtonSeedsFromConfiguration() {
        configuration.configurationSticker = true;
        const before = root.setStickerCalls;

        const loader = make();
        const all = collectAll(loader);
        const pin = findOne(all, function (o) {
            return o.checkable === true && o.hasOwnProperty("inStartup");
        });
        verify(pin, "pinButton not found");

        // onCompleted set checked = configuration.configurationSticker (true)...
        compare(pin.checked, true, "pin should be checked when sticker was saved on");
        // ...and called viewConfig.setSticker(true) at least once during construction.
        verify(root.setStickerCalls > before, "construction should call viewConfig.setSticker");
        compare(root.lastSetSticker, true, "setSticker should be passed the saved sticker state");
    }

    // The advanced switch's onCheckedChanged writes universalSettings.
    // inAdvancedModeForEditSettings when viewConfig.isReady. The switch is the
    // empty-text checkable control (not the pin button, which has inStartup) whose
    // toggle propagates to the setting; locate it by that observable effect.
    function test_advancedSwitchWritesSetting() {
        const loader = make();
        const all = collectAll(loader);

        viewConfig.isReady = true;
        universalSettings.inAdvancedModeForEditSettings = false;

        var wrote = false;
        for (var i = 0; i < all.length; i++) {
            const o = all[i];
            if (!o || typeof o.checked !== "boolean")
                continue;
            if (!o.hasOwnProperty("checkable") || o.checkable !== true)
                continue;
            if (!o.hasOwnProperty("text") || o.text !== "")
                continue;
            if (o.hasOwnProperty("inStartup"))   // that's the pin button
                continue;
            const was = universalSettings.inAdvancedModeForEditSettings;
            o.checked = !o.checked;
            if (universalSettings.inAdvancedModeForEditSettings !== was) {
                wrote = true;
                break;
            }
        }
        verify(wrote, "toggling the advanced switch should write inAdvancedModeForEditSettings");
    }
}
