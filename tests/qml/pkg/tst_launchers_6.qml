// Coverage for the plasmoid's Launchers ability. The component is loaded from
// the staged (instrumented) package by file URL so every executed line fires a
// Cov tick.
//
// Launchers.qml reads several unqualified context names that the real plasmoid
// provides. QML resolves those against the component's creation context, so we
// name the TestCase `id: root` and declare each name it touches as a lowercase-
// id'd QtObject / property here: `activityInfo` (current activity id), the
// `launchers` alias (the ability's own signal sink, used by addDroppedLauncher),
// `inDraggingPhase`, and `appletAbilities.myView`. With those wired the
// non-synced (bridge=null) branches run to completion and we assert their real
// effects: tasksModel CRUD calls, the ability's own signals, and the mock
// signal sink.
//
// The two config-fallback units (currentStoredLauncherList, importLauncherListInModel)
// dereference the attached `Plasmoid.configuration` singleton, which only exists
// inside a real applet. We can't shadow an uppercase attached name, so those
// stay live-only rather than swallow the throw to bank a hollow entry tick.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "Launchers6"
    when: windowShown

    readonly property url target: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/abilities/Launchers.qml")

    // ---- creation-context globals the component reads unqualified ----

    // The ambient `launchers` alias: in the live plasmoid it points back at the
    // ability, so addDroppedLauncher calls launchers.launcherChanged(url). Mock
    // it as a sink that records the urls it was handed.
    property var launcherChangedSink: []
    QtObject {
        id: launchers
        function launcherChanged(url) { root.launcherChangedSink.push(url); }
    }

    // activityInfo.currentActivity drives the activity-scoped add/remove and the
    // inCurrentActivity membership check.
    QtObject {
        id: activityInfo
        property string currentActivity: "ACT-CURRENT"
    }

    property bool inDraggingPhase: false

    // appletAbilities.myView.isReady is read by onGroupChanged and the
    // Connections target binding. Supply it so construction is clean.
    QtObject {
        id: appletAbilities
        property QtObject myView: QtObject { property bool isReady: false }
    }

    // A stand-in for org.kde.taskmanager TasksModel: record calls and answer the
    // queries the ability makes (launcherPosition / launcherActivities).
    property var positions: ({})        // url -> position
    property var activitiesByUrl: ({})  // url -> [activityId]

    Component {
        id: tasksModelComp
        QtObject {
            property var launcherList: []
            property var calls: []
            function requestAddLauncher(url) { calls.push(["add", url]); }
            function requestRemoveLauncher(url) { calls.push(["remove", url]); }
            function requestAddLauncherToActivity(url, act) { calls.push(["addToAct", url, act]); }
            function requestRemoveLauncherFromActivity(url, act) { calls.push(["rmFromAct", url, act]); }
            function syncLaunchers() { calls.push(["sync"]); }
            function launcherPosition(url) {
                return root.positions[url] !== undefined ? root.positions[url] : -1;
            }
            function launcherActivities(url) {
                return root.activitiesByUrl[url] !== undefined ? root.activitiesByUrl[url] : [];
            }
        }
    }

    // A fake task delegate living under `layout.children`.
    Component {
        id: taskComp
        Item {
            property int lastValidIndex: -1
            property int itemIndex: 0
            property string launcherUrl: ""
            property bool isSeparator: false
        }
    }

    // A fake layout whose `children` are the task delegates.
    Component {
        id: layoutComp
        Item {}
    }

    // A fake view supplying groupId. Must be an Item: the ability declares
    // `property Item view`, so a bare QtObject is rejected as an initial value.
    Component {
        id: viewComp
        Item {
            property string groupId: "VIEWGRP"
        }
    }

    function makeContext() {
        const tm = tasksModelComp.createObject(root);
        verify(tm, "tasksModel failed");
        const lay = layoutComp.createObject(root);
        verify(lay, "layout failed");
        const vw = viewComp.createObject(root);
        verify(vw, "view failed");
        return { tm: tm, lay: lay, vw: vw };
    }

    // Build the ability with bridge=null (non-synced branches) and the mock
    // context wired in. Unqualified globals (activityInfo, launchers,
    // inDraggingPhase, appletAbilities) resolve against this TestCase root.
    function make(ctx) {
        const c = Qt.createComponent(target);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {
            bridge: null,
            layout: ctx.lay,
            view: ctx.vw,
            tasksModel: ctx.tm
        });
        verify(obj, "instantiate failed");
        return obj;
    }

    // group classification + separator naming/lookup, all pure or tasksModel-only.
    function test_group_and_separators() {
        const ctx = makeContext();
        const m = make(ctx);

        // group defaults to UniqueLaunchers
        verify(m.inUniqueGroup());
        verify(!m.inLayoutGroup());
        verify(!m.inGlobalGroup());

        // isSeparator: url containing latte-separator
        verify(m.isSeparator("file:///latte-separator1.desktop"));
        verify(!m.isSeparator("applications:firefox.desktop"));

        // No separators registered -> free name is the first slot.
        compare(m.freeAvailableSeparatorName(), "file:///latte-separator1.desktop");

        // Register separator #1 so the loop has to advance.
        root.positions["file:///latte-separator1.desktop"] = 0;
        compare(m.separatorExists("file:///latte-separator1.desktop"), true);
        compare(m.freeAvailableSeparatorName(), "file:///latte-separator2.desktop");

        // hasLauncher reflects the position map.
        verify(m.hasLauncher("file:///latte-separator1.desktop"));
        verify(!m.hasLauncher("applications:firefox.desktop"));
    }

    // addLauncher/removeLauncher with bridge=null hit the tasksModel branch and
    // emit launcherChanged/launcherRemoved.
    function test_add_remove_launcher() {
        const ctx = makeContext();
        const m = make(ctx);

        let changed = [];
        m.launcherChanged.connect(function(u){ changed.push(u); });
        let removed = [];
        m.launcherRemoved.connect(function(u){ removed.push(u); });

        m.addLauncher("applications:foo.desktop");
        compare(changed[changed.length - 1], "applications:foo.desktop");
        compare(ctx.tm.calls.some(function(c){ return c[0] === "add"; }), true);

        m.removeLauncher("applications:foo.desktop");
        compare(removed[removed.length - 1], "applications:foo.desktop");
        compare(ctx.tm.calls.some(function(c){ return c[0] === "remove"; }), true);
    }

    // addDroppedLauncher strips the ?iconData= suffix, emits launcherInAdding
    // with the basename, forwards the stripped url to tasksModel, notifies the
    // `launchers` sink and calls syncLaunchers. addDroppedLaunchers iterates the
    // array (bridge=null) and does the same per item.
    function test_dropped_launchers() {
        const ctx = makeContext();
        const m = make(ctx);

        root.launcherChangedSink = [];
        let adding = [];
        m.launcherInAdding.connect(function(f){ adding.push(f); });

        m.addDroppedLauncher("file:///apps/foo.desktop?iconData=BLOB");
        // filename pushed to launcherInAdding is the stripped basename.
        compare(adding[adding.length - 1], "foo.desktop");
        // requestAddLauncher got the stripped url (no ?iconData=).
        const lastAdd = ctx.tm.calls.filter(function(c){ return c[0] === "add"; }).pop();
        verify(lastAdd, "no add call");
        compare(lastAdd[1].indexOf("?iconData=") === -1, true);
        compare(lastAdd[1], "file:///apps/foo.desktop");
        // the `launchers` sink was notified with the stripped url.
        compare(root.launcherChangedSink[root.launcherChangedSink.length - 1], "file:///apps/foo.desktop");
        // syncLaunchers ran after the add.
        compare(ctx.tm.calls.some(function(c){ return c[0] === "sync"; }), true);

        // bridge=null -> iterates and calls addDroppedLauncher per item.
        adding = [];
        root.launcherChangedSink = [];
        m.addDroppedLaunchers(["file:///apps/bar.desktop", "file:///apps/baz.desktop"]);
        compare(adding.length, 2);
        compare(adding[0], "bar.desktop");
        compare(adding[1], "baz.desktop");
        compare(root.launcherChangedSink, ["file:///apps/bar.desktop", "file:///apps/baz.desktop"]);
    }

    // Separator add/remove at position. addInternalSeparatorAtPos emits
    // launcherInMoving then addLauncher.
    function test_internal_separators() {
        const ctx = makeContext();
        const m = make(ctx);

        let moving = [];
        m.launcherInMoving.connect(function(u, p){ moving.push([u, p]); });

        m.addInternalSeparatorAtPos(2);
        compare(moving.length, 1);
        compare(moving[0][1], 2);
        // A free separator slot was chosen (positions is shared state, so don't
        // hardcode #1) and the same name was added through the model.
        const sepName = moving[0][0];
        verify(m.isSeparator(sepName));
        verify(ctx.tm.calls.some(function(c){ return c[0] === "add" && c[1] === sepName; }));

        // removeInternalSeparatorAtPos -> childAtLayoutIndex -> removeLauncher on
        // a separator delegate.
        taskComp.createObject(ctx.lay, { itemIndex: 0, lastValidIndex: 0,
                                         launcherUrl: "file:///latte-separator9.desktop",
                                         isSeparator: true });
        m.removeInternalSeparatorAtPos(0);
        verify(ctx.tm.calls.some(function(c){ return c[0] === "remove" && c[1] === "file:///latte-separator9.desktop"; }));
    }

    // activity-scoped add/remove (bridge=null). With activityInfo wired the
    // currentActivity comparison resolves and the body forwards to tasksModel
    // and emits launcherChanged.
    function test_activity_funcs() {
        const ctx = makeContext();
        const m = make(ctx);

        let changed = [];
        m.launcherChanged.connect(function(u){ changed.push(u); });
        let removing = [];
        m.launcherInRemoving.connect(function(u){ removing.push(u); });

        // launcher on ALL activities being added to a DIFFERENT activity ->
        // isOnAllActivities true + activityId !== currentActivity -> launcherInRemoving.
        const NULL = "00000000-0000-0000-0000-000000000000";
        root.activitiesByUrl["app:x.desktop"] = [NULL];
        m.addLauncherToActivity("app:x.desktop", "ACT-OTHER");
        verify(ctx.tm.calls.some(function(c){ return c[0] === "addToAct" && c[1] === "app:x.desktop" && c[2] === "ACT-OTHER"; }));
        compare(changed[changed.length - 1], "app:x.desktop");
        compare(removing.indexOf("app:x.desktop") !== -1, true);

        // remove from the CURRENT activity -> launcherInRemoving then forward.
        removing = [];
        m.removeLauncherFromActivity("app:y.desktop", "ACT-CURRENT");
        verify(ctx.tm.calls.some(function(c){ return c[0] === "rmFromAct" && c[1] === "app:y.desktop" && c[2] === "ACT-CURRENT"; }));
        compare(changed[changed.length - 1], "app:y.desktop");
        compare(removing.indexOf("app:y.desktop") !== -1, true);
    }

    // layout-walk helpers: childAtLayoutIndex, indexOfLayoutLauncher,
    // currentShownLauncherList, inCurrentActivity, isOnAllActivities.
    function test_layout_walks() {
        const ctx = makeContext();
        const m = make(ctx);

        taskComp.createObject(ctx.lay, { itemIndex: 0, lastValidIndex: 0,
                                         launcherUrl: "app:a.desktop" });
        taskComp.createObject(ctx.lay, { itemIndex: 1, lastValidIndex: 1,
                                         launcherUrl: "app:b.desktop" });

        // childAtLayoutIndex matches by lastValidIndex.
        compare(m.childAtLayoutIndex(0).launcherUrl, "app:a.desktop");
        compare(m.childAtLayoutIndex(1).launcherUrl, "app:b.desktop");
        // position < 0 returns undefined early.
        compare(m.childAtLayoutIndex(-1), undefined);

        // indexOfLayoutLauncher
        compare(m.indexOfLayoutLauncher("app:b.desktop"), 1);
        compare(m.indexOfLayoutLauncher("missing"), -1);

        // currentShownLauncherList: both launchers present + in current activity
        // via the NULL-activity id.
        const NULL = "00000000-0000-0000-0000-000000000000";
        root.positions["app:a.desktop"] = 0;
        root.positions["app:b.desktop"] = 1;
        root.activitiesByUrl["app:a.desktop"] = [NULL];
        root.activitiesByUrl["app:b.desktop"] = [NULL];
        const shown = m.currentShownLauncherList();
        compare(shown.length, 2);
        compare(shown.indexOf("app:a.desktop") !== -1, true);
        compare(shown.indexOf("app:b.desktop") !== -1, true);

        // inCurrentActivity: a launcher we don't have -> false.
        verify(!m.inCurrentActivity("not-present.desktop"));
        // present + null-activity -> true.
        verify(m.inCurrentActivity("app:a.desktop"));
        // present + only the CURRENT activity -> true (activityInfo branch).
        root.positions["app:cur.desktop"] = 2;
        root.activitiesByUrl["app:cur.desktop"] = ["ACT-CURRENT"];
        verify(m.inCurrentActivity("app:cur.desktop"));
        // present + only some OTHER activity -> false.
        root.positions["app:oth.desktop"] = 3;
        root.activitiesByUrl["app:oth.desktop"] = ["ACT-OTHER"];
        verify(!m.inCurrentActivity("app:oth.desktop"));

        // isOnAllActivities reads launcherActivities only.
        verify(m.isOnAllActivities("app:a.desktop"));
        root.activitiesByUrl["app:c.desktop"] = ["ACT-OTHER"];
        verify(!m.isOnAllActivities("app:c.desktop"));
    }

    // validateSyncedLaunchersOrder with bridge=null hits the empty else branch.
    // The observable effect: it executes without touching the (null) bridge, so
    // no tasksModel call results and the validator stays untouched.
    function test_validate_order() {
        const ctx = makeContext();
        const m = make(ctx);
        const before = ctx.tm.calls.length;
        m.validateSyncedLaunchersOrder(); // bridge=null -> no-op else
        compare(ctx.tm.calls.length, before);
    }

    // isActive reflects whether a bridge is wired.
    function test_isactive() {
        const ctx = makeContext();
        const m = make(ctx);
        compare(m.isActive, false); // bridge null
    }
}
