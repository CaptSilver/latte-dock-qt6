/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// LayoutsContainer announces "my length is changing" by pushing itself onto the
// animations.needLength tracker, and a 300ms timer pops it back off. Everything
// that has to hold still while the dock resizes -- the mask/localGeometry writes,
// the effects rect binding, the fill-applet recalcs -- keys off that tracker
// being non-empty.
//
// The horizontal handler pushes the event; the vertical one called removeEvent
// on a key it had never added, which Tracker treats as a no-op, so a left/right
// edge dock latched animationSent without ever registering anything.
//
// The tracker here is the REAL Tracker.qml on purpose. A hand-written
// {property int count} mock would happily "count" a removeEvent and hide the
// entire defect.

import QtQuick
import QtTest

import org.kde.plasma.core 2.0 as PlasmaCore

import "../../declarativeimports/abilities/definition/animations" as AnimationsDefinition

TestCase {
    id: root
    name: "LayoutsContainerLengthEvent"
    when: windowShown
    visible: true
    width: 400
    height: 400

    readonly property url targetUrl: Qt.resolvedUrl("../../containment/package/contents/ui/layouts/LayoutsContainer.qml")

    //! Names below are the creation-context names LayoutsContainer.qml and its
    //! children read unqualified, mirroring what the containment main.qml
    //! provides on a live dock.

    property bool isVertical: false
    property bool isHorizontal: true
    property bool behaveAsPlasmaPanel: false
    property bool inStartup: false
    property bool inConfigureAppletsMode: false
    property int maxLength: 100000
    property int minLength: 0

    //! keeps the EnvironmentActions loader inactive
    property int scrollAction: 0
    property bool dragActiveWindowEnabled: false
    property bool closeActiveWindowEnabled: false

    function updateEffectsArea() {}

    property Item myView: Item {
        property int alignment: 0
        property bool inSlidingIn: false
        property bool inSlidingOut: false
    }

    property Item animations: Item {
        property QtObject duration: QtObject {
            property int proposed: 0
            property int small: 0
        }
        property QtObject speedFactor: QtObject {
            property real normal: 1.0
        }

        //! the real thing, not a counter mock -- see the header
        property AnimationsDefinition.Tracker needLength: AnimationsDefinition.Tracker {}
    }

    property Item parabolic: Item {
        property bool isEnabled: false
        property bool directRenderingEnabled: false
        property int spread: 0
        property QtObject factor: QtObject {
            property real zoom: 1.0
        }

        signal sglClearZoom()
        signal sglUpdateLowerItemScale(int delegateIndex, real newScale, real step)
        signal sglUpdateHigherItemScale(int delegateIndex, real newScale, real step)
    }

    property Item metrics: Item {
        property QtObject totals: QtObject {
            property int length: 48
            property int thickness: 48
        }
        property QtObject margin: QtObject {
            property int length: 0
            property int screenEdge: 0
        }
        property QtObject mask: QtObject {
            property int screenEdge: 0
        }

        signal iconSizeAnimationEnded()
    }

    property Item background: Item {
        property int offset: 0
        property color color: "black"
        property QtObject paddings: QtObject {
            property int left: 0
            property int right: 0
            property int top: 0
            property int bottom: 0
        }
        property QtObject shadows: QtObject {
            property int left: 0
            property int right: 0
            property int top: 0
            property int bottom: 0
        }
        property QtObject totals: QtObject {
            property int visualLength: 0
        }
    }

    property Item debug: Item {
        property bool layouterEnabled: false
        property bool timersEnabled: false
        property bool spacersEnabled: false
    }

    property Item autosize: Item {
        function updateIconSize() {}
    }

    //! the Debugger Loaders bind `readonly property Item debugLayout` to these
    //! even while inactive, so they have to be Items rather than QtObjects
    property Item layouter: Item {
        property bool appletsInParentChange: false
        property bool inNormalFillCalculationsState: true

        property Item startLayout: Item { property Item grid: Item {} }
        property Item mainLayout: Item { property Item grid: Item {} }
        property Item endLayout: Item { property Item grid: Item {} }
    }

    property Item visibilityManager: Item {
        property bool inRelocationAnimation: false
        property int slidingOutToPos: 0
        function updateMaskArea() {}
    }

    property Item latteView: Item {
        width: 400
        height: 400
        property Item visibility: Item {
            property bool isHidden: false
        }
    }

    property Item indexer: Item {}
    property Item layoutsManager: Item {}

    Item {
        id: hostItem
        anchors.fill: parent
    }

    //! Grows the given layout so the container's contents length changes.
    function growLayout(layout) {
        return Qt.createQmlObject('import QtQuick; Item { width: 40; height: 40 }', layout);
    }

    function make() {
        const component = Qt.createComponent(targetUrl);
        verify(component.status !== Component.Error, "LayoutsContainer failed to load: " + component.errorString());
        const obj = createTemporaryObject(component, hostItem);
        verify(obj, "LayoutsContainer was not created");
        return obj;
    }

    //! Positive control. Without it a red vertical case could equally mean the
    //! harness never delivered a contents change at all.
    function test_horizontalContentsChangeRegistersLengthEvent() {
        root.isHorizontal = true;
        root.isVertical = false;

        const obj = make();
        const tracker = root.animations.needLength;
        compare(tracker.count, 0, "tracker must start empty");

        growLayout(obj.mainLayout);
        wait(80);

        compare(tracker.count, 1, "horizontal contents change must register the length event");
        compare(obj.animationSent, true, "the sent flag must latch");

        //! the 300ms delayUpdateMaskArea timer owns the other half of the pair
        tryCompare(tracker, "count", 0, 1500);
    }

    function test_verticalContentsChangeRegistersLengthEvent() {
        root.isHorizontal = false;
        root.isVertical = true;

        const obj = make();
        const tracker = root.animations.needLength;
        compare(tracker.count, 0, "tracker must start empty");

        growLayout(obj.mainLayout);
        wait(80);

        compare(tracker.count, 1, "vertical contents change must register the length event");
        compare(obj.animationSent, true, "the sent flag must latch");

        tryCompare(tracker, "count", 0, 1500);
    }
}
