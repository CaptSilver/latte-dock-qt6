/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// How many applets does the dock actually hold? The containment turns the theme
// background on when the answer is zero, so an emptied dock stays visible enough
// to right-click and put something back.
//
// The two ParabolicEdgeSpacers live permanently inside _mainLayout and the count
// used to include them, so it could never reach zero and that fallback was dead.
// The start layout was never counted at all.
//
// shouldCheckHalfs rides on the same count: in Justify alignment the container
// has to notice when one half has outgrown half the screen, which is exactly the
// case the whole-contents check misses. Its guard was written as
// `_mainLayout.children>1` -- the list, not its length -- which coerces to NaN and
// compares false, so that check has never run.
//
// The creation-context mocks below mirror tst_layoutscontainer_lengthevent.qml --
// they are what LayoutsContainer.qml and its children read unqualified on a live
// dock, and the spacers only construct when all of them are present.

import QtQuick
import QtTest

import org.kde.latte.core 0.2 as LatteCore

import org.kde.plasma.core 2.0 as PlasmaCore

import "../../declarativeimports/abilities/definition/animations" as AnimationsDefinition

TestCase {
    id: root
    name: "LayoutsContainerAppletsCount"
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

    //! A stand-in applet: any plain child of a layout counts as content.
    function addApplet(layout) {
        return Qt.createQmlObject('import QtQuick; Item { width: 40; height: 40 }', layout);
    }

    //! The non-applet children a layout legitimately carries.
    function addDecoration(layout, flag) {
        return Qt.createQmlObject('import QtQuick; Item { width: 40; height: 40; property bool '
                                  + flag + ': true }', layout);
    }

    function make() {
        const component = Qt.createComponent(targetUrl);
        verify(component.status !== Component.Error, "LayoutsContainer failed to load: " + component.errorString());
        const obj = createTemporaryObject(component, hostItem);
        verify(obj, "LayoutsContainer was not created");
        return obj;
    }

    //! Positive control. Without it, a count of 0 below could just as well mean the
    //! spacers never constructed in this harness.
    function test_edgeSpacersAreRealChildrenOfMainLayout() {
        const obj = make();

        verify(obj.mainLayout.startParabolicSpacer, "start edge spacer must exist");
        verify(obj.mainLayout.endParabolicSpacer, "end edge spacer must exist");
        compare(obj.mainLayout.children.length, 2, "the spacers are the only children of an empty main layout");
    }

    function test_emptyContainerHoldsNoApplets() {
        const obj = make();

        compare(obj.appletsCount, 0, "the edge spacers are not applets");
    }

    function test_startLayoutAppletsAreCounted() {
        const obj = make();

        addApplet(obj.startLayout);

        compare(obj.appletsCount, 1, "an applet parked in the start layout still fills the dock");
    }

    function test_appletsAreCountedAcrossAllThreeLayouts() {
        const obj = make();

        addApplet(obj.startLayout);
        addApplet(obj.mainLayout);
        addApplet(obj.endLayout);

        compare(obj.appletsCount, 3, "every layout holds real applets");
    }

    function test_halfCheckStaysOffOutsideJustify() {
        const obj = make();
        root.myView.alignment = LatteCore.Types.Left;

        addApplet(obj.mainLayout);
        addApplet(obj.mainLayout);

        compare(obj.shouldCheckHalfs, false, "only a justified dock has halves to outgrow");
    }

    function test_halfCheckNeedsMoreThanOneAppletInTheMainLayout() {
        const obj = make();
        root.myView.alignment = LatteCore.Types.Justify;

        compare(obj.shouldCheckHalfs, false, "the edge spacers are not applets");

        addApplet(obj.mainLayout);

        compare(obj.shouldCheckHalfs, false, "a single applet cannot straddle the centre");
    }

    function test_halfCheckRunsForAJustifiedMainLayout() {
        const obj = make();
        root.myView.alignment = LatteCore.Types.Justify;

        addApplet(obj.mainLayout);
        addApplet(obj.mainLayout);

        compare(obj.shouldCheckHalfs, true, "a justified dock with a filled centre must watch its halves");
    }

    //! Guards against "subtract two" passing the cases above.
    function test_splittersAndDragPlaceholderAreNotApplets() {
        const obj = make();

        addDecoration(obj.mainLayout, "isInternalViewSplitter");
        addDecoration(obj.mainLayout, "isDndSpacer");

        compare(obj.appletsCount, 0, "a splitter and the drag placeholder are not content");

        addApplet(obj.mainLayout);

        compare(obj.appletsCount, 1, "only the real applet counts");
    }
}
