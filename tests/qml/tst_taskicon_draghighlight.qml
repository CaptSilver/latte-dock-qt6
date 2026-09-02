/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The rectangle TaskIcon fades in while a task is being dragged paints a 35%
// alpha fill inside a 1px opaque border, both taken from the theme's highlight
// colour. Deriving the fill by writing back into a sub-property of a bound
// color (tempColor.a = 0.35) destroys that binding on the first evaluation, so
// the fill freezes at whatever the accent was when the delegate was built while
// the border keeps tracking it: change the Plasma accent with the dock running
// and the same rectangle shows two different hues.
//
// This drives the real TaskIcon.qml under a mock task context and flips
// Kirigami.Theme.highlightColor underneath it. The border assertion is the
// control -- it proves the theme override reached the component at all.

import QtQuick
import QtTest

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.kirigami 2.0 as Kirigami

TestCase {
    id: root
    name: "TaskIconDragHighlight"
    when: windowShown
    visible: true
    width: 200
    height: 200

    readonly property url targetUrl: Qt.resolvedUrl("../../plasmoid/package/contents/ui/task/TaskIcon.qml")

    //! Names below are the creation-context names TaskIcon.qml and its animation
    //! children read unqualified, mirroring what tasks main.qml and TaskItem.qml
    //! provide on a live dock.

    //! main.qml (the tasks plasmoid root)
    property bool vertical: false
    property int location: PlasmaCore.Types.BottomEdge
    property bool showInfoBadge: false
    property bool showProgressBadge: false
    property bool showAudioBadge: false
    property Item dragSource: null
    property int noTasksInAnimation: 0
    property bool launcherBouncingEnabled: false
    property bool windowInAttentionEnabled: false
    property bool windowAddedInGroupEnabled: false
    property bool windowRemovedFromGroupEnabled: false

    //! TaskItem.qml's icon delegate refers to itself through this name; make()
    //! hands it the instance so Component.onDestruction has a live target.
    property Item taskIcon: null

    //! TaskDelegate's icon source
    property string decoration: "folder"

    Item {
        id: abilitiesMock

        property Item animations: Item {
            property bool active: true
            property QtObject speedFactor: QtObject {
                property real current: 1.0
                property real normal: 1.0
            }
            property QtObject duration: QtObject {
                property int small: 200
                property int large: 500
            }
            property QtObject needThickness: QtObject {
                property var events: []
                function addEvent(event) { events.push(event); }
                function removeEvent(event) { events = events.filter(function (e) { return e !== event; }); }
            }
        }

        property Item environment: Item {
            property bool isGraphicsSystemAccelerated: true
        }

        property Item indicators: Item {
            property QtObject info: QtObject {
                property bool needsIconColors: false
                property bool providesClickedAnimation: false
                property bool providesHoveredAnimation: false
                property bool providesTaskLauncherAnimation: false
                property bool providesInAttentionAnimation: false
                property bool providesGroupedWindowAddedAnimation: false
                property bool providesGroupedWindowRemovedAnimation: false
            }
        }

        property Item metrics: Item {
            property int iconSize: 48
            property QtObject totals: QtObject {
                property int length: 56
                property int thickness: 56
            }
            property QtObject margin: QtObject {
                property int length: 4
                property int thickness: 4
                property int tailThickness: 4
                property int screenEdge: 0
            }
        }

        property Item parabolic: Item {
            property QtObject factor: QtObject {
                property real zoom: 1.0
            }
            property bool directRenderingEnabled: false
            function setDirectRenderingEnabled(value) { directRenderingEnabled = value; }
            function invkClearZoom() {}
        }

        property Item myView: Item {
            property bool isHidden: false
            property QtObject itemShadow: QtObject {
                property bool isEnabled: false
                property int size: 0
            }
        }
    }

    //! TaskItem.qml (an AbilityItem.BasicItem)
    Item {
        id: taskItem

        property Item abilities: abilitiesMock

        signal taskLauncherActivated()
        signal taskGroupedWindowAdded()
        signal taskGroupedWindowRemoved()

        property bool isStartup: true
        property bool isSeparator: false
        property bool isDragged: false
        property bool isVertical: false
        property bool containsMouse: false
        property bool pressed: false
        property int lastButtonClicked: Qt.NoButton
        property int lastValidIndex: 0
        property string launcherUrl: ""
        property string launcherUrlWithIcon: ""
        property int badgeIndicator: 0
        property bool hasAudioStream: false
        property bool playingAudio: false
        property bool inAttention: false
        property bool inAddRemoveAnimation: false
        property bool inBlockingAnimation: false
        property bool inBouncingAnimation: false
        property bool inRemoveStage: false
        property bool inAttentionBuiltinAnimation: false
        property bool inNewWindowBuiltinAnimation: false
        property bool isLauncherBuiltinAnimationRunning: false
        property real iconAnimatedOffsetX: 0
        property real iconAnimatedOffsetY: 0

        property Item parabolicItem: Item {
            property real zoom: 1.0
        }

        property bool blockingAnimation: false
        function setBlockingAnimation(value) { blockingAnimation = value; }
        function animationStarted() {}
        function animationEnded() {}
    }

    //! MouseHandler.qml, the drop target that greys the icon during a file drag
    Item {
        id: mouseHandler
        property bool isDroppingSeparator: false
        property bool isDroppingFiles: false
        property Item hoveredItem: null
    }

    //! The containment bridge; only its palette is read from here
    QtObject {
        id: latteBridge
        property QtObject colorPalette: QtObject {
            property color textColor: "#ffffff"
        }
    }

    //! The tasks ListView
    Item {
        id: icList
        property int orientation: Qt.Horizontal
        function childAtIndex(index) { return undefined; }
    }

    QtObject {
        id: tasksExtendedManager
        function removeWaitingLauncher(url) {}
    }

    //! Kirigami.Theme propagates down the visual parent chain, so overriding the
    //! highlight here is what a live accent-colour change looks like to the icon.
    Item {
        id: hostItem
        anchors.fill: parent
        Kirigami.Theme.inherit: false
        Kirigami.Theme.highlightColor: "#ff0000"
    }

    function make() {
        const component = Qt.createComponent(targetUrl);
        verify(component.status === Component.Ready, "compile failed: " + component.errorString());
        const obj = createTemporaryObject(component, hostItem);
        verify(obj, "instantiate failed");
        root.taskIcon = obj;
        return obj;
    }

    //! The drag highlight is the rounded, thin-bordered rectangle child; assert
    //! it was found rather than letting a restyle silently skip the test.
    function dragHighlightOf(taskIconContainer) {
        for (let i = 0; i < taskIconContainer.children.length; ++i) {
            const child = taskIconContainer.children[i];
            if (child.radius === 3 && child.border !== undefined && child.border.width === 1) {
                return child;
            }
        }
        return null;
    }

    function init() {
        hostItem.Kirigami.Theme.highlightColor = "#ff0000";
    }

    function test_dragHighlightFollowsTheme() {
        const dragHighlight = dragHighlightOf(make());
        verify(dragHighlight, "drag highlight rectangle not found in TaskIcon");

        compare(dragHighlight.color.r, dragHighlight.border.color.r, "fill and border must start on the same hue");

        hostItem.Kirigami.Theme.highlightColor = "#00ff00";

        compare(dragHighlight.border.color.g, 1, "the theme override never reached the component");
        compare(dragHighlight.color.g, dragHighlight.border.color.g, "fill hue did not follow the theme");
        compare(dragHighlight.color.r, dragHighlight.border.color.r, "fill kept the stale hue");
    }

    //! The fill is deliberately translucent so the icon shows through; 8-bit
    //! colour quantization means it lands a hair under 0.35.
    function test_dragHighlightStaysTranslucent() {
        const dragHighlight = dragHighlightOf(make());
        verify(dragHighlight, "drag highlight rectangle not found in TaskIcon");

        fuzzyCompare(dragHighlight.color.a, 0.35, 0.01);
        compare(dragHighlight.border.color.a, 1, "the border must stay opaque");

        hostItem.Kirigami.Theme.highlightColor = "#00ff00";
        fuzzyCompare(dragHighlight.color.a, 0.35, 0.01);
    }
}
