/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Where does the alignment offset land? Each aligned state of AppletsContainer anchors
// one end of the dock to its edge and leaves the opposite end floating, so the offset
// belongs on the anchored side and the floating side must stay flush at 0.
//
// The floating side used to read `appletsContainer.lastMargin`, a property deleted in
// 2019. Nothing warned: QML answers an undeclared member with `undefined`, and every
// QQuickAnchors side margin has a RESET, so the binding quietly reset the margin. This
// pins what those bindings are worth, so a value can no longer arrive there by accident.
//
// The names below are what AppletsContainer.qml reads unqualified on a live dock.

import QtQuick
import QtTest

import org.kde.latte.core 0.2 as LatteCore

TestCase {
    id: root
    name: "AppletsContainerMargins"
    when: windowShown
    visible: true
    width: 400
    height: 400

    readonly property url targetUrl: Qt.resolvedUrl("../../containment/package/contents/ui/layouts/AppletsContainer.qml")

    property bool isVertical: false
    property bool isHorizontal: true
    property bool inConfigureAppletsMode: false

    property Item myView: Item {
        property int alignment: 0
    }

    property Item layoutsContainer: Item {
        property Item mainLayout: Item {
            property bool isCoveredFromSideLayouts: false
        }
    }

    //! only read while dragging an applet in edit mode, which this test never enters
    property Item dragOverlay: null

    Item {
        id: hostItem
        anchors.fill: parent
    }

    function make() {
        const component = Qt.createComponent(targetUrl);
        verify(component.status !== Component.Error, "AppletsContainer failed to load: " + component.errorString());
        const obj = createTemporaryObject(component, hostItem);
        verify(obj, "AppletsContainer was not created");
        return obj;
    }

    //! alignment, the margin that carries the offset, the margin on the free edge
    readonly property var alignedStates: [
        [LatteCore.Types.LeftEdgeTopAlign, "topMargin", "bottomMargin"],
        [LatteCore.Types.LeftEdgeBottomAlign, "bottomMargin", "topMargin"],
        [LatteCore.Types.RightEdgeTopAlign, "topMargin", "bottomMargin"],
        [LatteCore.Types.RightEdgeBottomAlign, "bottomMargin", "topMargin"],
        [LatteCore.Types.BottomEdgeLeftAlign, "leftMargin", "rightMargin"],
        [LatteCore.Types.BottomEdgeRightAlign, "rightMargin", "leftMargin"],
        [LatteCore.Types.TopEdgeLeftAlign, "leftMargin", "rightMargin"],
        [LatteCore.Types.TopEdgeRightAlign, "rightMargin", "leftMargin"]
    ]

    function test_offsetLandsOnTheAnchoredEdgeOnly() {
        const obj = make();
        obj.offset = 7;

        for (let i = 0; i < alignedStates.length; ++i) {
            const spec = alignedStates[i];
            obj.alignment = spec[0];

            compare(obj.anchors[spec[1]], 7, obj.state + ": the anchored edge carries the offset");
            compare(obj.anchors[spec[2]], 0, obj.state + ": the free edge takes no margin");
        }
    }

    //! Centered states steer with centerOffset instead, so all four margins stay flush.
    function test_centeredStatesUseNoSideMargins() {
        const obj = make();
        obj.offset = 7;

        const centered = [LatteCore.Types.LeftEdgeCenterAlign,
                          LatteCore.Types.RightEdgeCenterAlign,
                          LatteCore.Types.BottomEdgeCenterAlign,
                          LatteCore.Types.TopEdgeCenterAlign];

        for (let i = 0; i < centered.length; ++i) {
            obj.alignment = centered[i];

            compare(obj.anchors.leftMargin, 0, obj.state + ": left margin");
            compare(obj.anchors.rightMargin, 0, obj.state + ": right margin");
            compare(obj.anchors.topMargin, 0, obj.state + ": top margin");
            compare(obj.anchors.bottomMargin, 0, obj.state + ": bottom margin");
        }
    }
}
