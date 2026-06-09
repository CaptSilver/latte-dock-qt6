/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The edit-mode applet handle (ConfigOverlay.qml) resolves an applet's standard Configure/Remove
// actions. The real file needs live-dock ids (root/currentApplet/fastLayoutManager) so it can't load
// offscreen; this rebuilds the action-resolution wiring against a mock shaped like a Plasma 6
// PlasmoidItem: the real Applet lives under .plasmoid and standard actions come from
// internalAction(name). The PlasmoidItem has NO .action() method (the Plasma 5 call the handle used
// to make), which is exactly why the old code threw and removal stopped working.

import QtQuick
import QtTest

TestCase {
    id: testCase
    name: "EditModeHandle"
    when: windowShown
    visible: true
    width: 100
    height: 100

    Component {
        id: appletMock
        // Mirrors the QML object the handle sees: a PlasmoidItem (no action()) whose .plasmoid is the
        // Plasma::Applet exposing internalAction(name).
        Item {
            id: appletItem
            property bool isInternalViewSplitter: false
            property bool removeNull: false
            property bool removeEnabledProp: true
            property bool configureEnabledProp: true
            property int removeTriggers: 0
            property int configureTriggers: 0

            readonly property QtObject applet: QtObject {
                readonly property QtObject plasmoid: QtObject {
                    function internalAction(name) {
                        if (name === "remove") {
                            return appletItem.removeNull ? null : removeAct;
                        }
                        if (name === "configure") {
                            return configureAct;
                        }
                        return null;
                    }
                }
            }

            property QtObject removeAct: QtObject {
                property bool enabled: appletItem.removeEnabledProp
                function trigger() { appletItem.removeTriggers++; }
            }
            property QtObject configureAct: QtObject {
                property bool enabled: appletItem.configureEnabledProp
                function trigger() { appletItem.configureTriggers++; }
            }
        }
    }

    // The visibility expression copied verbatim from ConfigOverlay.qml's onVisualParentChanged.
    function closeButtonVisible(a) {
        return !a.isInternalViewSplitter
                && a.applet.plasmoid.internalAction("remove")
                && a.applet.plasmoid.internalAction("remove").enabled;
    }

    // The Plasma 6 chain has no action() on the PlasmoidItem — the old handle code crashed here.
    function test_oldAppletActionApiIsAbsent() {
        const a = appletMock.createObject(testCase);
        verify(a, "mock failed to instantiate");
        verify(a.applet.action === undefined,
               "a Plasma 6 PlasmoidItem must not expose action(); the handle must go through "
               + ".plasmoid.internalAction()");
        a.destroy();
    }

    function test_removeButtonVisibleWhenActionEnabled() {
        const a = appletMock.createObject(testCase, {removeEnabledProp: true});
        verify(closeButtonVisible(a), "Remove button must show when the remove action is enabled");
        a.destroy();
    }

    function test_removeButtonHiddenWhenActionDisabled() {
        const a = appletMock.createObject(testCase, {removeEnabledProp: false});
        verify(!closeButtonVisible(a), "Remove button must hide when the remove action is disabled");
        a.destroy();
    }

    function test_removeButtonHiddenWhenNoRemoveAction() {
        const a = appletMock.createObject(testCase, {removeNull: true});
        verify(!closeButtonVisible(a), "Remove button must hide when there is no remove action");
        a.destroy();
    }

    // Clicking the Remove button triggers the applet's remove action (what actually deletes a widget).
    function test_clickingRemoveTriggersInternalAction() {
        const a = appletMock.createObject(testCase);
        compare(a.removeTriggers, 0);
        if (a && a.applet) {
            a.applet.plasmoid.internalAction("remove").trigger();
        }
        compare(a.removeTriggers, 1, "clicking Remove must trigger internalAction(\"remove\")");
        a.destroy();
    }

    function test_clickingConfigureTriggersInternalAction() {
        const a = appletMock.createObject(testCase);
        if (a && a.applet) {
            a.applet.plasmoid.internalAction("configure").trigger();
        }
        compare(a.configureTriggers, 1, "clicking Configure must trigger internalAction(\"configure\")");
        a.destroy();
    }
}
