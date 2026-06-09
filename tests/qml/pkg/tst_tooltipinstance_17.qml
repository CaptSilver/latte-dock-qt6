/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Headless coverage driver for previews/ToolTipInstance.qml.
//
// The delegate reads a pile of enclosing-scope ids (isGroup, isWin, root,
// mpris2Source, tasksModel, parentTask, windowsPreviewDlg, backend,
// toolTipDelegate, virtualDesktopInfo, activityInfo, ...) that only exist
// inside a live dock. Rather than swallow the ReferenceErrors those throw, we
// load the staged/instrumented copy through a Loader living in a wrapper that
// *declares* every ambient name as a property/QtObject with the real shape.
// Loader-created items inherit the wrapper's QML context, so the delegate's
// unqualified lookups resolve to our mocks and each function/handler runs
// end-to-end with assertable return values and side effects.
import QtQuick
import QtTest

TestCase {
    id: tc
    name: "ToolTipInstance17"
    when: windowShown

    readonly property url target: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/previews/ToolTipInstance.qml")

    // Fresh wrapper per test: declares every ambient name ToolTipInstance reads.
    // Recording objects (backend/tasksModel/windowsPreviewDlg/mpris2Source) let
    // the onClicked handlers assert observable side effects. The staged delegate
    // loads through vLoader and inherits this context.
    function makeWrapper(props) {
        const wrapperSrc =
            'import QtQuick\n'
          + 'Item {\n'
          + '  id: host\n'
          // --- recording sinks the handlers write to ---
          + '  property var closeRequests: []\n'
          + '  property var activateRequests: []\n'
          + '  property int cancelHighlightCalls: 0\n'
          + '  property var mprisCalls: []\n'
          // --- plain ambient values the delegate reads ---
          + '  property bool isGroup: false\n'
          + '  property bool isWin: false\n'
          + '  property string icon: "application-x-executable"\n'
          + '  property string appName: "Firefox"\n'
          + '  property var genericName: "Web Browser"\n'
          + '  property string displayParent: "Some Page - Firefox"\n'
          + '  property int pidParent: 111\n'
          + '  property bool hideCloseButtons: false\n'
          + '  property real textWidth: 200\n'
          + '  property var windows: []\n'
          + '  property var parentTask: null\n'
          + '  property var virtualDesktopParent: []\n'
          + '  property var isOnAllVirtualDesktopsParent: false\n'
          + '  property var activitiesParent\n'   // intentionally undefined by default
          + '  property var model\n'               // delegate model context; group title reads model.display
          // --- i18nc shim (no live translation engine headless) ---
          + '  function i18nc(ctx, fmt, a1) {\n'
          + '     if (a1 === undefined) return fmt;\n'
          + '     return fmt.replace("%1", a1);\n'
          + '  }\n'
          // --- root: the plasmoid root the delegate dots into ---
          + '  property QtObject root: QtObject {\n'
          + '     property bool showOnlyCurrentDesktop: false\n'
          + '     property bool showOnlyCurrentActivity: false\n'
          + '     property bool plasma520: false\n'
          + '     property bool plasmaAtLeast524: false\n'
          + '     property bool plasmaAtLeast525: false\n'
          + '     property bool plasmaAtLeast526: false\n'
          + '     function createContextMenu(t, i) { return null; }\n'
          + '     function forcePreviewsHiding(x) {}\n'
          + '     function windowsHovered(ids, h) {}\n'
          + '  }\n'
          // --- mainToolTip ---
          + '  property QtObject mainToolTip: QtObject { property bool isMinimizedParent: false }\n'
          // --- toolTipDelegate ---
          + '  property QtObject toolTipDelegate: QtObject { property string launcherUrl: "applications:firefox.desktop" }\n'
          // --- windowsPreviewDlg (handler flips .visible) ---
          + '  property QtObject windowsPreviewDlg: QtObject { property bool visible: true }\n'
          // --- backend (handler calls cancelHighlightWindows) ---
          + '  property QtObject backend: QtObject {\n'
          + '     function cancelHighlightWindows() { host.cancelHighlightCalls += 1; }\n'
          + '  }\n'
          // --- tasksModel (handlers call requestClose/requestActivate) ---
          + '  property QtObject tasksModel: QtObject {\n'
          + '     function requestClose(idx) { host.closeRequests.push(idx); }\n'
          + '     function requestActivate(idx) { host.activateRequests.push(idx); }\n'
          + '  }\n'
          // --- mpris2Source: source-name lookup + data map + control verbs ---
          + '  property string mprisName: ""\n'
          + '  property var mprisData: ({})\n'
          + '  property QtObject mpris2Source: QtObject {\n'
          + '     property var data: host.mprisData\n'
          + '     function sourceNameForLauncherUrl(url, pid) { return host.mprisName; }\n'
          + '     function play(n) { host.mprisCalls.push(["play", n]); }\n'
          + '     function pause(n) { host.mprisCalls.push(["pause", n]); }\n'
          + '     function goNext(n) { host.mprisCalls.push(["goNext", n]); }\n'
          + '     function goPrevious(n) { host.mprisCalls.push(["goPrevious", n]); }\n'
          + '     function raise(n) { host.mprisCalls.push(["raise", n]); }\n'
          + '  }\n'
          // --- appletAbilities.myView.screenGeometry (thumbnail height ratio) ---
          + '  property QtObject appletAbilities: QtObject {\n'
          + '     property QtObject myView: QtObject {\n'
          + '        property rect screenGeometry: Qt.rect(0, 0, 1920, 1080)\n'
          + '     }\n'
          + '  }\n'
          // --- virtualDesktopInfo / activityInfo for generateSubText ---
          + '  property QtObject virtualDesktopInfo: QtObject {\n'
          + '     property var desktopNames: ["Desktop 1", "Desktop 2"]\n'
          + '     property int numberOfDesktops: 2\n'
          + '  }\n'
          + '  property string curActivity: "act-current"\n'
          + '  property QtObject activityInfo: QtObject {\n'
          + '     property int numberOfRunningActivities: 2\n'
          + '     property string currentActivity: host.curActivity\n'
          + '     function activityName(id) { return id === "act-current" ? "Current" : ("Name:" + id); }\n'
          + '  }\n'
          + '  property Loader vLoader: Loader { source: host.vSource }\n'
          + '  property url vSource\n'
          + '}\n';
        const w = Qt.createQmlObject(wrapperSrc, tc, "tooltipWrapper");
        verify(w, "wrapper create failed");
        if (props !== undefined) {
            for (var k in props) {
                w[k] = props[k];
            }
        }
        // Set the source last so the delegate is created with the mocks wired.
        w.vSource = target;
        return w;
    }

    function instanceOf(w) {
        tryVerify(function() { return w.vLoader.status === Loader.Ready
                                   || w.vLoader.status === Loader.Error; }, 4000);
        verify(w.vLoader.status === Loader.Ready,
               "delegate load failed status=" + w.vLoader.status
               + " err=" + w.vLoader.sourceComponent);
        const item = w.vLoader.item;
        verify(item, "no loaded delegate item");
        return item;
    }

    // Walk an object tree, returning the first node whose id-ish objectName or
    // declared `id` matches; we instead match by a probe function the caller gives.
    function findFirst(node, matches) {
        var stack = [node];
        var guard = 0;
        while (stack.length > 0 && guard < 5000) {
            guard++;
            var n = stack.pop();
            if (!n)
                continue;
            if (matches(n))
                return n;
            var kids = n.children;
            if (kids !== undefined)
                for (var i = 0; i < kids.length; ++i)
                    stack.push(kids[i]);
            var res = n.resources;
            if (res !== undefined)
                for (var j = 0; j < res.length; ++j)
                    stack.push(res[j]);
        }
        return null;
    }

    // isTaskActive: isGroup branch returns own isActive; non-group branch reads
    // parentTask.isActive, or false when no parentTask.
    function test_isTaskActive() {
        // group case -> returns this.isActive
        const wg = makeWrapper({ isGroup: true });
        const mg = instanceOf(wg);
        mg.isActive = true;
        verify(mg.isTaskActive(), "group+active should be active");
        mg.isActive = false;
        verify(!mg.isTaskActive(), "group+inactive should be inactive");

        // non-group, no parentTask -> false
        const wn = makeWrapper({ isGroup: false, parentTask: null });
        const mn = instanceOf(wn);
        verify(!mn.isTaskActive(), "non-group without parentTask should be false");

        // non-group, parentTask present -> mirrors parentTask.isActive
        const pt = Qt.createQmlObject('import QtQuick; QtObject { property bool isActive: true }', tc, "parentTaskMock");
        const wp = makeWrapper({ isGroup: false, parentTask: pt });
        const mp = instanceOf(wp);
        verify(mp.isTaskActive(), "non-group should mirror parentTask.isActive=true");
        pt.isActive = false;
        verify(!mp.isTaskActive(), "non-group should mirror parentTask.isActive=false");
    }

    // generateTitle: !isWin returns genericName (or "" when undefined).
    function test_generateTitle_notWindow() {
        const w = makeWrapper({ isWin: false, genericName: "Web Browser" });
        const m = instanceOf(w);
        compare(m.generateTitle(), "Web Browser");

        const w2 = makeWrapper({ isWin: false, genericName: undefined });
        const m2 = instanceOf(w2);
        compare(m2.generateTitle(), "");
    }

    // generateTitle: window, non-group -> uses displayParent and strips a
    // trailing appName plus the KWin "<n>" counter, re-appending the counter.
    function test_generateTitle_window_stripsAppNameAndCounter() {
        // "Some Page - Firefox" with appName "Firefox" -> drops " - Firefox"
        const w = makeWrapper({ isWin: true, isGroup: false,
                                appName: "Firefox",
                                displayParent: "Some Page - Firefox" });
        const m = instanceOf(w);
        compare(m.generateTitle(), "Some Page");

        // Title that is only the appName collapses to the em-dash placeholder.
        const w2 = makeWrapper({ isWin: true, isGroup: false,
                                 appName: "Firefox", displayParent: "Firefox" });
        const m2 = instanceOf(w2);
        compare(m2.generateTitle(), "—");

        // KWin counter "<2>" is saved and re-appended after appName stripping.
        const w3 = makeWrapper({ isWin: true, isGroup: false,
                                 appName: "Firefox",
                                 displayParent: "Some Page - Firefox <2>" });
        const m3 = instanceOf(w3);
        compare(m3.generateTitle(), "Some Page <2>");
    }

    // generateTitle: window + group -> reads ambient model.display; undefined
    // display -> "". The group branch reads `model` from the creation context,
    // so the mock lives on the wrapper.
    function test_generateTitle_group_fromModel() {
        const w = makeWrapper({ isWin: true, isGroup: true });
        w.model = { display: "Window Title - Firefox" };
        const m = instanceOf(w);
        compare(m.generateTitle(), "Window Title");

        const w2 = makeWrapper({ isWin: true, isGroup: true });
        w2.model = { display: undefined };
        const m2 = instanceOf(w2);
        compare(m2.generateTitle(), "");
    }

    // generateSubText: activitiesParent undefined returns "" immediately.
    function test_generateSubText_noActivitiesParent() {
        const w = makeWrapper({ isGroup: false });   // activitiesParent stays undefined
        const m = instanceOf(w);
        compare(m.generateSubText(), "");
    }

    // generateSubText: non-group with a desktop set on >1 desktops and an empty
    // activity list on >1 running activities -> both an "On ..." line and the
    // "Available on all activities" line.
    function test_generateSubText_desktopAndAllActivities() {
        const w = makeWrapper({ isGroup: false,
                                virtualDesktopParent: [1],          // Desktop 1
                                isOnAllVirtualDesktopsParent: false });
        // activitiesParent: empty array -> "all activities" branch (numberOfRunning>1)
        w.activitiesParent = [];
        const m = instanceOf(w);
        const txt = m.generateSubText();
        verify(txt.indexOf("Desktop 1") !== -1, "desktop name missing: " + txt);
        verify(txt.indexOf("all activities") !== -1, "all-activities line missing: " + txt);
    }

    // generateSubText: non-group on a specific non-current activity ->
    // "Available on <name>" listing that activity.
    function test_generateSubText_specificActivity() {
        const w = makeWrapper({ isGroup: false,
                                virtualDesktopParent: [],
                                isOnAllVirtualDesktopsParent: false });
        w.activitiesParent = ["act-other"];   // not the current activity
        const m = instanceOf(w);
        const txt = m.generateSubText();
        verify(txt.indexOf("Name:act-other") !== -1,
               "activity name missing from subtext: " + txt);
    }

    // Close-button onClicked: non-group path hides windowsPreviewDlg, cancels
    // highlight, and requests close of submodelIndex.
    function test_closeButton_onClicked() {
        const w = makeWrapper({ isWin: true, isGroup: false });
        const m = instanceOf(w);
        m.submodelIndex = "idx-7";

        const btn = findFirst(m, function(n) {
            return n.objectName === "" && typeof n.clicked === "function"
                   && n.hasOwnProperty("icon") && String(n.icon ? n.icon.name : "") === "window-close";
        });
        verify(btn, "close button not found in delegate tree");

        btn.clicked();

        compare(w.windowsPreviewDlg.visible, false, "previews dlg not hidden");
        compare(w.cancelHighlightCalls, 1, "cancelHighlightWindows not called");
        compare(w.closeRequests.length, 1, "requestClose not called");
        compare(w.closeRequests[0], "idx-7", "requestClose got wrong index");
    }

    // Close-button onClicked, group path: skips windowsPreviewDlg hiding but
    // still cancels highlight + requests close.
    function test_closeButton_onClicked_group() {
        const w = makeWrapper({ isWin: true, isGroup: true });
        const m = instanceOf(w);
        m.submodelIndex = "grp-idx";

        const btn = findFirst(m, function(n) {
            return typeof n.clicked === "function"
                   && n.hasOwnProperty("icon") && String(n.icon ? n.icon.name : "") === "window-close";
        });
        verify(btn, "close button not found");

        w.windowsPreviewDlg.visible = true;
        btn.clicked();

        // group branch must not touch the previews dialog visibility
        compare(w.windowsPreviewDlg.visible, true, "group path should not hide previews dlg");
        compare(w.cancelHighlightCalls, 1, "cancelHighlightWindows not called");
        compare(w.closeRequests[0], "grp-idx");
    }

    // mpris play-button onClicked: with a live player the playbackLoader builds
    // playerControlsComp; clicking the play/pause button calls mpris2Source.
    function test_mprisPlayButton_onClicked() {
        const w = makeWrapper({ isWin: true, isGroup: true });
        // Make hasPlayer true: non-empty source name + truthy data entry that is
        // NOT "Playing" so the click takes the play() branch.
        w.mprisName = "mpris-firefox";
        var d = {};
        d["mpris-firefox"] = {
            PlaybackStatus: "Paused",
            CanControl: true, CanPlay: true, CanPause: true,
            CanGoPrevious: true, CanGoNext: true, CanRaise: true,
            Metadata: {}
        };
        w.mprisData = d;
        const m = instanceOf(w);
        verify(m.hasPlayer, "hasPlayer should be true with mocked mpris source");
        verify(!m.playing, "should not be playing (status Paused)");

        // The playbackLoader is active when hasPlayer; wait for it to build.
        const playBtn = findFirstWait(m, function(n) {
            return typeof n.clicked === "function"
                   && n.hasOwnProperty("icon")
                   && String(n.icon ? n.icon.name : "").indexOf("media-playback") !== -1;
        });
        verify(playBtn, "play/pause button not found (playbackLoader inactive?)");

        playBtn.clicked();

        compare(w.mprisCalls.length, 1, "mpris control not invoked");
        compare(w.mprisCalls[0][0], "play", "not-playing click should call play()");
        compare(w.mprisCalls[0][1], "mpris-firefox", "play() got wrong source name");
    }

    // mpris play-button onClicked, playing state -> pause() branch.
    function test_mprisPlayButton_pauseBranch() {
        const w = makeWrapper({ isWin: true, isGroup: true });
        w.mprisName = "mpris-firefox";
        var d = {};
        d["mpris-firefox"] = {
            PlaybackStatus: "Playing",
            CanControl: true, CanPlay: true, CanPause: true,
            CanGoPrevious: true, CanGoNext: true, CanRaise: true,
            Metadata: {}
        };
        w.mprisData = d;
        const m = instanceOf(w);
        verify(m.playing, "should be playing (status Playing)");

        const playBtn = findFirstWait(m, function(n) {
            return typeof n.clicked === "function"
                   && n.hasOwnProperty("icon")
                   && String(n.icon ? n.icon.name : "").indexOf("media-playback") !== -1;
        });
        verify(playBtn, "play/pause button not found");

        playBtn.clicked();
        compare(w.mprisCalls[0][0], "pause", "playing click should call pause()");
        compare(w.mprisCalls[0][1], "mpris-firefox");
    }

    // Like findFirst but retries: the playerControls live in a Loader that builds
    // a frame after hasPlayer flips true.
    function findFirstWait(node, matches) {
        var found = null;
        tryVerify(function() {
            found = findFirst(node, matches);
            return found !== null;
        }, 4000, "node never appeared in tree");
        return found;
    }
}
