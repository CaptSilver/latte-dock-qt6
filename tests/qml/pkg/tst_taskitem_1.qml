// Coverage: drive the plasmoid's TaskItem helpers/handlers against the staged
// (instrumented) package. TaskItem is an AbilityItem.BasicItem subclass that
// reaches for a lot of containment context through unqualified names (root,
// tasksModel, backend, pulseAudio, scrollableList, windowsPreviewDlg,
// toolTipDelegate, tasksExtendedManager, virtualDesktopInfo, activityInfo,
// dragHelper, mouseHandler) plus an `abilities` tree. QML resolves those
// unqualified reads against the component's creation context, so the TestCase
// is named `id: root` and every name the exercised units touch is declared on
// it (or on a child mock) shaped like the real object. No throw-swallowing: we
// only call a unit when its mock context lets the body run, and assert the
// observable effect (return value, property change, or mock side-effect).
import QtQuick
import QtTest

TestCase {
    id: root
    name: "TaskItem"
    when: windowShown
    visible: true
    width: 300
    height: 300

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/task/TaskItem.qml")

    // ---- unqualified globals the component reads through its creation context ----
    // root.* names: the component literally uses `root.foo`; this TestCase is
    // that `root`. Defaults chosen so construction bindings settle without
    // throwing, and so the exercised branches are reachable.
    property bool showAudioBadge: false
    property bool showPreviews: false
    property bool showWindowsOnlyFromLaunchers: false
    property bool disableAllWindowsFunctionality: false
    property bool inActivityChange: false
    property bool inEditMode: false
    property bool inDraggingPhase: false
    property bool vertical: false
    property bool plasma515: true
    property bool plasmaAtLeast525: true
    property int location: 4 // PlasmaCore.Types.BottomEdge
    property var dragSource: null
    property var contextMenu: null
    property int modifierQt: Qt.MetaModifier

    // Delegate-context names a real ListView would inject. modelIndex(),
    // SubWindows and generateSubText read bare `index` / `model`; supplying
    // them on the creation context lets those bodies run instead of throwing.
    property int index: 0
    property var model: ({ AppPid: -1, AppName: "", IsGroupParent: false })

    // root signals the component connects to in Component.onCompleted.
    signal draggingFinished()
    signal publishTasksGeometries()
    signal showPreviewForTasks(var group)

    // root function calls made by the helpers, recorded for assertions.
    property string lastForcePreviewArg: ""
    function forcePreviewsHiding(debugtext) { lastForcePreviewArg = "" + debugtext; }

    property var createdContextMenuArgs: null
    property int createContextMenuCalls: 0
    function createContextMenu(item, modelIndex, args) {
        createContextMenuCalls += 1;
        createdContextMenuArgs = args;
        return menuComp.createObject(root, {});
    }

    // root.getBadger(url) drives updateBadge(); return a value for a known url.
    function getBadger(url) {
        if (url === "applications:badged.desktop") {
            return { value: "7" };
        }
        return null;
    }

    // root.modifierClick is compared against LatteTasks.Types.* by
    // modifierAccepted. We can't name the enum here without the import, so the
    // test sets it to the numeric LeftClick value used by the component.
    property int modifierClick: 0 // LatteTasks.Types.LeftClick

    // ---- libtaskmanager-style context objects ----
    QtObject {
        id: tasksModelObj
        property int newInstanceCalls: 0
        property int activateCalls: 0
        property var lastIndex: null
        function makeModelIndex(i, sub) { return { row: i, sub: (sub === undefined ? -1 : sub) }; }
        function requestNewInstance(idx) { newInstanceCalls += 1; lastIndex = idx; }
        function requestActivate(idx) { activateCalls += 1; lastIndex = idx; }
        function requestToggleMinimized(idx) {}
        function requestPublishDelegateGeometry(idx, geo, item) {}
        function launcherPosition(url) { return url === "applications:exists.desktop" ? 2 : -1; }
        property string activity: ""
    }
    property QtObject tasksModel: tasksModelObj

    QtObject {
        id: backendObj
        property bool windowViewAvailable: false
        property bool canPresentWindows: false
        function generateMimeData(a, b, c) { return null; }
        function globalRect(item) { return Qt.rect(0, 0, 10, 10); }
    }
    property QtObject backend: backendObj

    // pulseAudio.item.* — updateAudioStreams reads pulseAudio.item then queries
    // streamsForPid/streamsForAppName. A null item makes the helper take its
    // early "no PA" branch and clear audioStreams. The item mock is shaped like
    // Plasma-PA's source item: streamsForPid returns whatever the test stages.
    property var stagedStreams: []
    QtObject {
        id: pulseAudioObj
        property var item: null
    }
    Component {
        id: pulseAudioItemComp
        QtObject {
            property int registerPidCalls: 0
            function streamsForPid(pid) { return root.stagedStreams; }
            function streamsForAppName(name) { return root.stagedStreams; }
            function registerPidMatch(name) { registerPidCalls += 1; }
            function hasPidMatch(name) { return false; }
        }
    }
    property QtObject pulseAudio: pulseAudioObj

    // windowsPreviewDlg: previews dialog. hidePreviewWindow checks activeItem.
    QtObject {
        id: windowsPreviewDlgObj
        property var activeItem: null
        property var visualParent: null
        property bool visible: false
        property bool containsMouse: false
        property int hideCalls: 0
        property string lastHideArg: ""
        function hide(arg) { hideCalls += 1; lastHideArg = "" + arg; }
        function show(item) {}
    }
    property QtObject windowsPreviewDlg: windowsPreviewDlgObj

    QtObject {
        id: toolTipDelegateObj
        property var parentTask: null
        property var rootIndex: null
        property bool hideCloseButtons: false
        property string appName: ""
        property int pidParent: 0
        property var windows: null
        property bool isGroup: false
        property var icon: null
        property var launcherUrl: null
        property bool isLauncher: false
        property bool isMinimizedParent: false
        property var displayParent: null
        property var genericName: null
        property var virtualDesktopParent: null
        property bool isOnAllVirtualDesktopsParent: false
        property var activitiesParent: null
    }
    property QtObject toolTipDelegate: toolTipDelegateObj

    QtObject {
        id: tasksExtendedManagerObj
        property var waitingLaunchers: []
        function isLauncherToBeMoved(url) { return false; }
        function moveLauncherToCorrectPos(url, idx) {}
        function addWaitingLauncher(url) { waitingLaunchers.push(url); }
        function waitingLauncherExists(url) { return false; }
        signal waitingLauncherRemoved(string url)
    }
    property QtObject tasksExtendedManager: tasksExtendedManagerObj

    QtObject {
        id: scrollableListObj
        property bool animationsFinished: false
        function autoScrollFor(item, v) {}
        function focusOn(item) {}
    }
    property QtObject scrollableList: scrollableListObj

    QtObject {
        id: virtualDesktopInfoObj
        property int numberOfDesktops: 1
        property var desktopNames: ["one"]
    }
    property QtObject virtualDesktopInfo: virtualDesktopInfoObj

    QtObject {
        id: activityInfoObj
        property int numberOfRunningActivities: 1
        property string currentActivity: "cur"
        function activityName(id) { return "act-" + id; }
    }
    property QtObject activityInfo: activityInfoObj

    QtObject {
        id: dragHelperObj
        property var dragObj: QtObject {
            property var imageSource: ""
            property var mimeData: null
            property bool active: false
        }
    }
    // dragHelper is read as dragHelper.Drag.* via an attached-property idiom in
    // onIsDraggedChanged; that path needs a real grabToImage and is live-only.
    property QtObject dragHelper: dragHelperObj

    QtObject {
        id: mouseHandlerObj
        property var hoveredItem: null
        property var ignoredItem: null
    }
    property QtObject mouseHandler: mouseHandlerObj

    Component {
        id: menuComp
        QtObject {
            property int showCalls: 0
            function show() { showCalls += 1; }
            function destroy() {}
        }
    }

    // ---- mock audio stream ----
    Component {
        id: streamComp
        QtObject {
            property bool corked: false
            property bool muted: false
            property real volume: 50
            function mute() { muted = true; }
            function unmute() { muted = false; }
            function increaseVolume() { volume += 5; }
            function decreaseVolume() { volume -= 5; }
        }
    }
    function makeStream(args) { return streamComp.createObject(root, args || {}); }

    // ---- mock abilities tree ----
    // TaskItem declares `property Item abilities`, so the mock must be an Item
    // (a bare QtObject silently fails the type assignment and stays null).
    Component {
        id: abilitiesComp
        Item {
            id: ab
            property int location: 4 // BottomEdge
            property bool isLayoutGridContainer: false

            property QtObject parabolic: QtObject {
                property bool isEnabled: false
                property QtObject factor: QtObject { property real zoom: 1.5 }
                signal sglClearZoom()
                function setDirectRenderingEnabled(v) {}
                function setCurrentParabolicItem(i) {}
            }
            property QtObject metrics: QtObject {
                property int iconSize: 48
                property QtObject margin: QtObject { property int length: 4; property int screenEdge: 0; property int maxThickness: 24 }
                property QtObject totals: QtObject { property int thickness: 56; property int length: 56 }
                property QtObject mask: QtObject {
                    property QtObject thickness: QtObject { property int zoomedForItems: 80 }
                }
            }
            property QtObject animations: QtObject {
                property bool active: true
                property QtObject duration: QtObject { property int large: 200; property int small: 80; property int proposed: 100 }
                property QtObject speedFactor: QtObject { property real current: 1.0; property real normal: 1.0 }
            }
            property QtObject indicators: QtObject {
                property QtObject info: QtObject {
                    property bool providesTaskLauncherAnimation: false
                    property bool needsMouseEventCoordinates: false
                    property bool providesFrontLayer: false
                    property bool providesHoveredAnimation: false
                    property bool providesClickedAnimation: false
                }
            }
            property QtObject indexer: QtObject {
                property int firstVisibleItemIndex: 0
                property int lastVisibleItemIndex: 5
                property var hidden: []
                property var separators: []
                property bool tailAppletIsSeparator: false
                property bool headAppletIsSeparator: false
            }
            property QtObject containment: QtObject {
                property bool isFirstAppletInContainment: false
                property bool isLastAppletInContainment: false
            }
            property QtObject debug: QtObject {
                property bool timersEnabled: false
                property bool graphicsEnabled: false
                property bool inputMaskEnabled: false
            }
            property QtObject shortcuts: QtObject {
                property bool isEnabled: false
                signal sglActivateEntryAtIndex(int i)
                signal sglNewInstanceForEntryAtIndex(int i)
                function shortcutIndex(i) { return -1; }
            }
            property QtObject thinTooltip: QtObject {
                property bool isEnabled: false
                signal hide()
            }
            property QtObject myView: QtObject {
                property bool isReady: false
                property bool isShownFully: true
                property bool isHidden: false
                property real backgroundOpacity: 1.0
                property QtObject itemShadow: QtObject { property color shadowSolidColor: "black" }
                property var colorPalette: null
                property QtObject screenGeometry: QtObject {
                    property int x: 0; property int y: 0; property int width: 1920; property int height: 1080
                }
                function inCurrentLayout() { return true; }
            }
            property QtObject launchers: QtObject {
                signal launcherChanged(string l)
                signal launcherRemoved(string l)
                function isSeparator(u) { return u === "separator://sep"; }
                function inCurrentActivity(u) { return true; }
            }
        }
    }

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const ab = abilitiesComp.createObject(root);
        verify(ab, "abilities mock failed");
        // Create as a child of root so root IS the component's creation context;
        // that is what makes the unqualified globals above resolve.
        const obj = createTemporaryObject(c, root, { abilities: ab });
        verify(obj, "instantiate failed");
        return obj;
    }

    function test_construct() {
        const obj = make();
        compare(obj.objectName, "TaskItem");
    }

    // Pure animation-flag setters: animationStarted/animationEnded/
    // handlerDraggingFinished/setBlockingAnimation.
    function test_animationFlags() {
        const obj = make();
        obj.animationStarted();
        compare(obj.inAnimation, true);
        obj.animationEnded();
        compare(obj.inAnimation, false);

        obj.isDragged = false;
        obj.handlerDraggingFinished();
        compare(obj.isDragged, false);

        obj.setBlockingAnimation(true);
        compare(obj.inBlockingAnimation, true);
        obj.setBlockingAnimation(false);
        compare(obj.inBlockingAnimation, false);
    }

    // Stage streams through the real updateAudioStreams() path. Assigning
    // audioStreams directly fights onHasAudioStreamChanged -> updateAudioStreams
    // (which resets to [] unless pulseAudio.item hands the same streams back),
    // so the honest way to populate audioStreams is to feed pulseAudio.item.
    function populateStreams(obj, streams) {
        root.stagedStreams = streams;
        pulseAudioObj.item = pulseAudioItemComp.createObject(root);
        obj.updateAudioStreams();
    }

    // Volume mutators iterate audioStreams. The volume/muted GETTERS short-circuit
    // on hasAudioStream; with showAudioBadge false they return 0/false even with
    // streams present. Assert the short-circuit and the per-stream mutations.
    function test_audioHelpers() {
        const obj = make();
        root.showAudioBadge = false;
        const s1 = makeStream({ muted: false, volume: 30, corked: false });
        const s2 = makeStream({ muted: false, volume: 70, corked: true });
        populateStreams(obj, [s1, s2]);
        compare(obj.audioStreams.length, 2);

        // showAudioBadge false -> hasAudioStream false -> volume 0, muted false.
        compare(obj.volume, 0);
        compare(obj.muted, false);

        obj.increaseVolume();
        compare(s1.volume, 35);
        compare(s2.volume, 75);

        obj.decreaseVolume();
        compare(s1.volume, 30);
        compare(s2.volume, 70);

        // muted getter is false -> toggleMuted mutes every stream.
        obj.toggleMuted();
        verify(s1.muted);
        verify(s2.muted);
    }

    // With showAudioBadge true the volume/muted/playingAudio getters take their
    // live branch over the staged streams.
    function test_audioGettersWithBadge() {
        const obj = make();
        root.showAudioBadge = true;
        const s1 = makeStream({ muted: true, volume: 20, corked: true });
        const s2 = makeStream({ muted: true, volume: 65, corked: true });
        populateStreams(obj, [s1, s2]);

        // isLauncher false (IsLauncher undefined) and streams present -> hasAudioStream.
        compare(obj.hasAudioStream, true);
        compare(obj.volume, 65);           // max of the two
        compare(obj.muted, true);          // every() over muted streams
        compare(obj.playingAudio, false);  // both corked -> nothing playing

        // muted true -> toggleMuted unmutes all.
        obj.toggleMuted();
        verify(!s1.muted);
        verify(!s2.muted);

        root.showAudioBadge = false;
        pulseAudioObj.item = null;
        root.stagedStreams = [];
    }

    // playingAudio is true when at least one staged stream is uncorked.
    function test_playingAudio() {
        const obj = make();
        root.showAudioBadge = true;
        const s1 = makeStream({ muted: false, volume: 40, corked: false });
        populateStreams(obj, [s1]);
        compare(obj.playingAudio, true);
        root.showAudioBadge = false;
        pulseAudioObj.item = null;
        root.stagedStreams = [];
    }

    function test_emptyStreamMutators() {
        const obj = make();
        obj.audioStreams = [];
        obj.increaseVolume();
        obj.decreaseVolume();
        obj.toggleMuted();
        compare(obj.audioStreams.length, 0);
        compare(obj.volume, 0);
    }

    // modifierAccepted compares mouse.modifiers against root.modifierQt and the
    // button against root.modifierClick. With the modifier bit set it returns
    // true for the matching button, false otherwise.
    function test_modifierAccepted() {
        const obj = make();
        // modifierClick defaults to LeftClick(0): a left click with the meta
        // modifier is accepted.
        compare(obj.modifierAccepted({ modifiers: Qt.MetaModifier, button: Qt.LeftButton }), true);
        // Wrong button -> not accepted.
        compare(obj.modifierAccepted({ modifiers: Qt.MetaModifier, button: Qt.RightButton }), false);
        // Modifier bit missing -> not accepted.
        compare(obj.modifierAccepted({ modifiers: Qt.NoModifier, button: Qt.LeftButton }), false);
    }

    // modelIndex() forwards to tasksModel.makeModelIndex(index). index is
    // undefined in the headless context, so the mock receives undefined and
    // returns a row object we can assert on.
    function test_modelIndex() {
        const obj = make();
        const idx = obj.modelIndex();
        verify(idx !== null && idx !== undefined);
        compare(idx.sub, -1);
    }

    // updateBadge() reads root.getBadger(launcherUrl) and sets badgeIndicator.
    function test_updateBadge() {
        const obj = make();
        // No badger for the empty/unknown url -> 0.
        obj.launcherUrl = "applications:none.desktop";
        obj.updateBadge();
        compare(obj.badgeIndicator, 0);

        // Known url returns value "7" -> parseInt -> 7.
        obj.launcherUrl = "applications:badged.desktop";
        obj.updateBadge();
        compare(obj.badgeIndicator, 7);
    }

    // onLauncherUrlChanged calls updateBadge(); writing launcherUrl to the
    // badged url drives the badge to 7 via the change handler.
    function test_launcherUrlChangeHandler() {
        const obj = make();
        obj.launcherUrl = "applications:badged.desktop";
        compare(obj.badgeIndicator, 7);
    }

    // onModelLauncherUrlChanged parses launcherName out of the url and sets
    // isSeparator from abilities.launchers.isSeparator. Drive it by writing
    // modelLauncherUrl.
    function test_modelLauncherUrlChangeHandler() {
        const obj = make();
        obj.modelLauncherUrl = "applications:firefox.desktop";
        compare(obj.launcherUrl, "applications:firefox.desktop");
        compare(obj.launcherName, "firefox");
        compare(obj.isSeparator, false);

        // A separator url flips isSeparator true (mock isSeparator matches it).
        obj.modelLauncherUrl = "separator://sep";
        compare(obj.isSeparator, true);
    }

    function test_modelLauncherUrlWithIconChangeHandler() {
        const obj = make();
        obj.modelLauncherUrlWithIcon = "applications:withicon.desktop";
        compare(obj.launcherUrlWithIcon, "applications:withicon.desktop");
    }

    // updateAudioStreams: with a null pulseAudio.item it clears audioStreams.
    function test_updateAudioStreams_noPulseAudio() {
        const obj = make();
        pulseAudioObj.item = null;
        obj.audioStreams = [makeStream({})];
        compare(obj.audioStreams.length, 1);
        obj.updateAudioStreams();
        compare(obj.audioStreams.length, 0);
    }

    // updateAudioStreams: with dragSource set it short-circuits and clears.
    function test_updateAudioStreams_dragSource() {
        const obj = make();
        obj.audioStreams = [makeStream({}), makeStream({})];
        root.dragSource = obj;
        obj.updateAudioStreams();
        compare(obj.audioStreams.length, 0);
        root.dragSource = null;
    }

    // forceHidePreview sets the release-event block flag, stops the hovered
    // timer, and calls root.forcePreviewsHiding(arg).
    function test_forceHidePreview() {
        const obj = make();
        obj.showPreviewsIsBlockedFromReleaseEvent = false;
        root.lastForcePreviewArg = "";
        obj.forceHidePreview("8.3");
        compare(obj.showPreviewsIsBlockedFromReleaseEvent, true);
        compare(root.lastForcePreviewArg, "8.3");
    }

    // hidePreviewWindow only hides when this task is the dialog's activeItem.
    function test_hidePreviewWindow() {
        const obj = make();
        // Not the active item -> no hide call.
        windowsPreviewDlgObj.activeItem = null;
        windowsPreviewDlgObj.hideCalls = 0;
        obj.hidePreviewWindow();
        compare(windowsPreviewDlgObj.hideCalls, 0);

        // Becomes the active item -> hide() with the "14.1" debug tag.
        windowsPreviewDlgObj.activeItem = obj;
        obj.hidePreviewWindow();
        compare(windowsPreviewDlgObj.hideCalls, 1);
        compare(windowsPreviewDlgObj.lastHideArg, "14.1");
        windowsPreviewDlgObj.activeItem = null;
    }

    // showContextMenu: when no menu exists it creates one via
    // root.createContextMenu and shows it; called again (menu now "exists" on
    // root) it tears the menu down. isSeparator+!inEditMode early-returns.
    function test_showContextMenu() {
        const obj = make();
        root.createContextMenuCalls = 0;
        root.contextMenu = null;

        const args = { x: 1, y: 2 };
        obj.showContextMenu(args);
        compare(root.createContextMenuCalls, 1);
        compare(root.createdContextMenuArgs, args);
        verify(obj.contextMenu !== null);

        // Separator outside edit mode bails before creating a second menu.
        obj.isSeparator = true;
        root.inEditMode = false;
        obj.showContextMenu({});
        compare(root.createContextMenuCalls, 1);
    }

    // preparePreviewWindow wires toolTipDelegate bindings/properties off the
    // model. With model undefined the Qt.binding bodies error lazily, but the
    // direct assignments (parentTask, hideCloseButtons, rootIndex) run and are
    // observable.
    function test_preparePreviewWindow() {
        const obj = make();
        toolTipDelegateObj.parentTask = null;
        toolTipDelegateObj.hideCloseButtons = false;
        obj.preparePreviewWindow(true);
        compare(toolTipDelegateObj.parentTask, obj);
        compare(toolTipDelegateObj.hideCloseButtons, true);
        verify(toolTipDelegateObj.rootIndex !== null);
    }

    // slotShowPreviewForTasks shows the preview only for this task when the
    // dialog isn't already visible; preparePreviewWindow runs as a side effect.
    function test_slotShowPreviewForTasks() {
        const obj = make();
        windowsPreviewDlgObj.visible = false;
        toolTipDelegateObj.parentTask = null;
        // A different group object -> no-op.
        obj.slotShowPreviewForTasks(root);
        compare(toolTipDelegateObj.parentTask, null);
        // This task -> preparePreviewWindow set parentTask to obj.
        obj.slotShowPreviewForTasks(obj);
        compare(toolTipDelegateObj.parentTask, obj);
    }

    // updateVisibilityBasedOnLaunchers: with showWindowsOnlyFromLaunchers and
    // no launcher present for a window task, the task force-hides itself.
    function test_updateVisibilityBasedOnLaunchers() {
        const obj = make();
        root.showWindowsOnlyFromLaunchers = true;
        obj.isWindow = true;
        obj.isForcedHidden = false;
        obj.launcherUrl = "applications:missing.desktop";   // launcherPosition -> -1
        obj.launcherUrlWithIcon = "applications:missing.desktop";
        obj.updateVisibilityBasedOnLaunchers();
        compare(obj.isForcedHidden, true);
        root.showWindowsOnlyFromLaunchers = false;
    }

    // onLauncherChanged forwards to updateVisibilityBasedOnLaunchers only when
    // showWindowsOnlyFromLaunchers (or disableAll) is set and the url matches.
    function test_onLauncherChanged() {
        const obj = make();
        root.showWindowsOnlyFromLaunchers = true;
        obj.isWindow = true;
        obj.isForcedHidden = false;
        obj.launcherUrl = "applications:missing.desktop";
        obj.launcherUrlWithIcon = "applications:missing.desktop";
        // Non-matching url -> no visibility change.
        obj.onLauncherChanged("applications:other.desktop");
        compare(obj.isForcedHidden, false);
        // Matching url -> force-hide via updateVisibilityBasedOnLaunchers.
        obj.onLauncherChanged("applications:missing.desktop");
        compare(obj.isForcedHidden, true);
        root.showWindowsOnlyFromLaunchers = false;
    }

    // slotWaitingLauncherRemoved makes a hidden window/launcher visible again
    // when the removed launcher matches and resets zoom (no provider animation).
    function test_slotWaitingLauncherRemoved() {
        const obj = make();
        obj.isWindow = true;
        obj.visible = false;
        obj.launcherUrl = "applications:wait.desktop";
        // Non-matching launcher -> stays hidden.
        obj.slotWaitingLauncherRemoved("applications:nomatch.desktop");
        compare(obj.visible, false);
        // Matching launcher -> becomes visible.
        obj.slotWaitingLauncherRemoved("applications:wait.desktop");
        compare(obj.visible, true);
    }

    // generateSubText is live-only: its very first statement dereferences
    // Plasmoid.configuration.showOnlyCurrentDesktop, and the attached Plasmoid
    // object has no configuration without a real applet, so the body throws
    // before any assertable work. Covered live, not here.

    // activateNextTask forwards to subWindows.activateNextTask(); for a
    // non-group task that helper returns early without requesting activation.
    function test_activateNextTask_nonGroup() {
        const obj = make();
        tasksModelObj.activateCalls = 0;
        // isGroupParent false by default -> subWindows.activateNextTask returns
        // before requesting activation.
        obj.activateNextTask();
        compare(tasksModelObj.activateCalls, 0);
    }

    // animationStarted/Ended are also reachable through the inAnimation
    // property; assert the property reflects the setter both ways.
    function test_inAnimationToggle() {
        const obj = make();
        obj.animationStarted();
        verify(obj.inAnimation);
        obj.animationEnded();
        verify(!obj.inAnimation);
    }

    // shortcutRequestedActivate routes to activateTask (non-group) which, for a
    // launcher task with compositing, requests activation through tasksModel.
    // For a plain non-launcher non-group window task it toggles/activate via
    // tasksModel as well. We assert the activate path fires.
    function test_shortcutActivate_routesToActivate() {
        const obj = make();
        obj.isSeparator = false;
        // Make it a launcher so activateTask -> activateLauncher -> requestActivate.
        root.disableAllWindowsFunctionality = false;
        tasksModelObj.activateCalls = 0;
        tasksModelObj.newInstanceCalls = 0;
        // Drive via the helper directly (isLauncher comes from the model and is
        // false here, so activateTask takes the window branch: not group, not
        // minimized, not active -> requestActivate).
        obj.activateTask();
        verify(tasksModelObj.activateCalls + tasksModelObj.newInstanceCalls >= 1);
    }
}
