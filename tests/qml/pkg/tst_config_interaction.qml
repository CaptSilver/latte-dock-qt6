// Coverage for the tasks-plasmoid Interaction config page. The page reads two
// ambient names from its creation context: `plasmoid` (formFactor + the
// configuration object) and PlasmaCore.Types. QML resolves a component's free
// identifiers against its creation context, so we name the TestCase `id: root`
// and declare `plasmoid` as a lowercase-id'd QtObject; the staged, instrumented
// page is parented into root and its handlers run against the stand-in.
//
// The real logic lives in the Hover combo: its currentIndex binding maps
// plasmoid.configuration.hoverAction (a LatteTasks.Types.TaskAction enum) to a
// row 0..3, and onCurrentIndexChanged writes the matching enum back. Both
// directions are asserted on their observable effect (the resolved index, the
// config write). The `vertical` property alias is driven via plasmoid.formFactor
// to cover that binding too.
import QtQuick
import QtTest

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.latte.private.tasks 0.1 as LatteTasks

TestCase {
    id: root
    name: "ConfigInteraction"
    when: windowShown
    visible: true
    width: 400
    height: 500

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/config/ConfigInteraction.qml")

    // The config object the page binds its aliases and the hover switch against.
    QtObject {
        id: configuration
        property int hoverAction: LatteTasks.Types.NoneAction
        property bool wheelEnabled: false
        property int middleClickAction: 0
        property bool showOnlyCurrentScreen: false
        property bool showOnlyCurrentDesktop: false
        property bool showOnlyCurrentActivity: false
        property bool showInfoBadge: false
        property bool showWindowActions: false
    }

    QtObject {
        id: plasmoid
        property var configuration: configuration
        property int formFactor: PlasmaCore.Types.Horizontal
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = c.createObject(root, { visible: true });
        verify(obj, "instantiate failed: " + c.errorString());
        return obj;
    }

    // Depth-first walk collecting every descendant the predicate accepts.
    function collect(node, pred) {
        const out = [];
        const stack = [node];
        const seen = [];
        while (stack.length) {
            const o = stack.pop();
            if (!o || seen.indexOf(o) !== -1)
                continue;
            seen.push(o);
            if (pred(o))
                out.push(o);
            const kids = o.children;
            if (kids !== undefined)
                for (var i = 0; i < kids.length; i++)
                    stack.push(kids[i]);
            const res = o.resources;
            if (res !== undefined)
                for (var j = 0; j < res.length; j++)
                    stack.push(res[j]);
            if (o.contentItem !== undefined && o.contentItem !== null)
                stack.push(o.contentItem);
        }
        return out;
    }

    // The Hover combo is the ComboBox whose model is the 4-entry hover-action
    // array; the Middle-Click combo's model has 4 entries too, so disambiguate
    // by the presence of the currentIndex-mapping behavior: only the hover combo
    // changes plasmoid.configuration.hoverAction. We instead key off model[1]
    // text not being stable across translations, so find both 4-entry combos and
    // pick the one whose currentIndex tracks the hoverAction enum.
    function comboBoxes(page) {
        return collect(page, function(o) {
            return typeof o.currentIndex === "number"
                && o.model !== undefined && o.model.length === 4;
        });
    }

    // Identify the hover combo: set hoverAction to HighlightWindows and the hover
    // combo's binding resolves currentIndex to 2; the middle-click combo ignores
    // hoverAction entirely.
    function hoverCombo(page) {
        const combos = comboBoxes(page);
        configuration.hoverAction = LatteTasks.Types.HighlightWindows;
        for (var i = 0; i < combos.length; i++) {
            if (combos[i].currentIndex === 2)
                return combos[i];
        }
        return null;
    }

    // ---- Tests ----------------------------------------------------------------

    // The Hover combo's currentIndex binding maps each TaskAction enum to its row.
    function test_hover_combo_index_from_enum() {
        const page = make();
        const combo = hoverCombo(page);
        verify(combo, "hover combo not found");

        configuration.hoverAction = LatteTasks.Types.NoneAction;
        compare(combo.currentIndex, 0);

        configuration.hoverAction = LatteTasks.Types.PreviewWindows;
        compare(combo.currentIndex, 1);

        configuration.hoverAction = LatteTasks.Types.HighlightWindows;
        compare(combo.currentIndex, 2);

        configuration.hoverAction = LatteTasks.Types.PreviewAndHighlightWindows;
        compare(combo.currentIndex, 3);

        page.destroy();
    }

    // An enum value outside the switch's four cases falls through to row 0.
    function test_hover_combo_index_default_fallback() {
        const page = make();
        const combo = hoverCombo(page);
        verify(combo, "hover combo not found");

        // Close (1) is a TaskAction the switch doesn't handle -> default return 0.
        configuration.hoverAction = LatteTasks.Types.Close;
        compare(combo.currentIndex, 0);

        page.destroy();
    }

    // onCurrentIndexChanged writes the matching TaskAction enum back into config.
    function test_hover_combo_writes_config() {
        const page = make();
        const combo = hoverCombo(page);
        verify(combo, "hover combo not found");

        combo.currentIndex = 0;
        compare(configuration.hoverAction, LatteTasks.Types.NoneAction);

        combo.currentIndex = 1;
        compare(configuration.hoverAction, LatteTasks.Types.PreviewWindows);

        combo.currentIndex = 2;
        compare(configuration.hoverAction, LatteTasks.Types.HighlightWindows);

        combo.currentIndex = 3;
        compare(configuration.hoverAction, LatteTasks.Types.PreviewAndHighlightWindows);

        page.destroy();
    }

    // The page-level `vertical` property tracks plasmoid.formFactor against
    // PlasmaCore.Types.Vertical.
    function test_vertical_tracks_formfactor() {
        plasmoid.formFactor = PlasmaCore.Types.Horizontal;
        const page = make();
        compare(page.vertical, false);

        plasmoid.formFactor = PlasmaCore.Types.Vertical;
        compare(page.vertical, true);

        plasmoid.formFactor = PlasmaCore.Types.Horizontal; // restore
        page.destroy();
    }
}
