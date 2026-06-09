// Drives the plasmoid launcher Syncer ability through its public sync
// functions, the isActive transition handler, and the host isReady
// Connections handler. The component is loaded from the staged (instrumented)
// package by file URL so the Cov.tick calls fire, and every assertion pins an
// observable effect: a mock side-effect (a forwarded call landed), a signal
// emission, or a backend property write.
//
// Unqualified creation-context names the target reads, declared shaped on the
// TestCase root (the component's creation context):
//   - bridge          : isActive + every host call; .launchers.host carries
//                        addAbilityClient/removeAbilityClient/isReady.
//   - _launchers      : group/groupId + the launcherChanged/launcherInRemoving/
//                        isOnAllActivities/addDroppedLauncher members.
//   - tasksModel      : requestAdd/Remove launcher + syncLaunchers sink.
//   - activityInfo    : currentActivity, read by the *Activity variants.
//   - validator       : the order-validation Timer-like object.
//
// Live-only (not claimed here): clientId reads Plasmoid.id (a qualified
// attached property, unmockable); Component.onCompleted/onDestruction only run
// during incubation/teardown where no assertion observes their host call.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "Syncer"
    when: windowShown

    // bridge starts null so isActive is false at construction; tests that need
    // the active path assign hostBridge to flip it and drive onIsActiveChanged.
    property var bridge: null

    // The host the syncer registers itself with. Carries isReady (+ its change
    // signal) so the Connections onIsReadyChanged handler can be driven, and
    // records the client passed to add/removeAbilityClient.
    QtObject {
        id: hostObj
        property bool isReady: false
        property var lastClient: null
        property int addCalls: 0
        property int removeCalls: 0
        function addAbilityClient(client) { addCalls++; lastClient = client; }
        function removeAbilityClient(client) { removeCalls++; lastClient = client; }
    }
    QtObject {
        id: launchersBridge
        property QtObject host: hostObj
    }
    QtObject {
        id: hostBridge
        property QtObject launchers: launchersBridge
    }

    // The launchers ability the syncer guards every action against. group is the
    // gate (matched against the first argument); the signals/methods record that
    // the forward landed. groupId backs the syncedGroupId binding.
    QtObject {
        id: _launchers
        property int group: 5
        property string groupId: "synced-group-7"
        property var lastChanged: ""
        property var lastRemoving: ""
        property var lastDropped: ""
        property int changedCalls: 0
        property int removingCalls: 0
        property int droppedCalls: 0
        property bool onAllActivities: false
        function launcherChanged(url) { changedCalls++; lastChanged = url; }
        function launcherInRemoving(url) { removingCalls++; lastRemoving = url; }
        function isOnAllActivities(url) { return onAllActivities; }
        function addDroppedLauncher(url) { droppedCalls++; lastDropped = url; }
    }

    // The LibTaskManager model sink. Records the per-request urls and the
    // syncLaunchers tail call so each forward is asserted, not merely executed.
    QtObject {
        id: tasksModel
        property var lastAdd: ""
        property var lastRemove: ""
        property var lastAddToActivity: []
        property var lastRemoveFromActivity: []
        property int syncCalls: 0
        function requestAddLauncher(url) { lastAdd = url; }
        function requestRemoveLauncher(url) { lastRemove = url; }
        function requestAddLauncherToActivity(url, activity) { lastAddToActivity = [url, activity]; }
        function requestRemoveLauncherFromActivity(url, activity) { lastRemoveFromActivity = [url, activity]; }
        function syncLaunchers() { syncCalls++; }
    }

    QtObject {
        id: activityInfo
        property string currentActivity: "act-current"
    }

    // The order validator the syncer stops/seeds/starts. Records the launcher
    // list write plus the stop/start lifecycle so validateSyncedLaunchersOrder
    // is asserted on its observable effect.
    QtObject {
        id: validator
        property var launchers: []
        property int stopCalls: 0
        property int startCalls: 0
        function stop() { stopCalls++; }
        function start() { startCalls++; }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/abilities/launchers/Syncer.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    function init() {
        // Reset shared mock state so tests are order-independent.
        bridge = null;
        hostObj.isReady = false;
        hostObj.lastClient = null;
        hostObj.addCalls = 0;
        hostObj.removeCalls = 0;
        _launchers.group = 5;
        _launchers.onAllActivities = false;
        _launchers.changedCalls = 0;
        _launchers.removingCalls = 0;
        _launchers.droppedCalls = 0;
        tasksModel.syncCalls = 0;
        validator.stopCalls = 0;
        validator.startCalls = 0;
        validator.launchers = [];
    }

    // syncedGroupId mirrors _launchers.groupId; pin the binding so the read of
    // the unqualified _launchers name is asserted.
    function test_syncedGroupId_binding() {
        const m = make();
        compare(m.syncedGroupId, "synced-group-7");
    }

    // Component.onCompleted@22 active branch: with bridge set BEFORE construction
    // isActive is true at completion, so the handler registers the client. The
    // object is retained, so the addAbilityClient side-effect is observable after
    // make() returns. isActive also transitions false->true during settling, so
    // onIsActiveChanged contributes a second add; both active-registration paths
    // ran (hence two calls), proving onCompleted took its addAbilityClient branch.
    function test_onCompleted_registersWhenActiveAtConstruction() {
        bridge = hostBridge;
        const m = make();
        verify(m.isActive);
        compare(hostObj.addCalls, 2);
        compare(hostObj.lastClient, m);
    }

    // onIsActiveChanged@34 add branch: isActive is false at construction (bridge
    // null). Assigning a real bridge flips bridge.launchers.host !== null true, so
    // isActive transitions to true and the handler runs its addAbilityClient
    // branch. Assert the transition hit the host with this client.
    function test_isActiveChanged_addsClientOnActivation() {
        const m = make();
        verify(!m.isActive);
        compare(hostObj.addCalls, 0);

        bridge = hostBridge;
        verify(m.isActive);
        compare(hostObj.addCalls, 1);
        compare(hostObj.lastClient, m);
    }

    // onIsReadyChanged@44: with an active bridge, the Connections target is the
    // real host. Flipping host.isReady true (while active) re-registers the
    // client. Assert the handler ran by the add count climbing on the ready flip.
    function test_isReadyChanged_reRegistersWhenReady() {
        const m = make();
        bridge = hostBridge;
        verify(m.isActive);
        const baseline = hostObj.addCalls; // already 1 from the active transition
        hostObj.isReady = true;
        compare(hostObj.addCalls, baseline + 1);
        compare(hostObj.lastClient, m);
    }

    // addSyncedLauncher@52: matching group forwards requestAddLauncher +
    // launcherChanged + syncLaunchers; a mismatching group is a no-op.
    function test_addSyncedLauncher() {
        const m = make();
        m.addSyncedLauncher(5, "applications:firefox.desktop");
        compare(tasksModel.lastAdd, "applications:firefox.desktop");
        compare(_launchers.lastChanged, "applications:firefox.desktop");
        compare(tasksModel.syncCalls, 1);

        // wrong group -> guarded out, nothing more happens
        m.addSyncedLauncher(99, "applications:other.desktop");
        compare(tasksModel.lastAdd, "applications:firefox.desktop");
        compare(tasksModel.syncCalls, 1);
    }

    // removeSyncedLauncher@60: matching group fires launcherInRemoving +
    // requestRemoveLauncher + launcherChanged + syncLaunchers.
    function test_removeSyncedLauncher() {
        const m = make();
        m.removeSyncedLauncher(5, "a:b.desktop");
        compare(_launchers.lastRemoving, "a:b.desktop");
        compare(tasksModel.lastRemove, "a:b.desktop");
        compare(_launchers.lastChanged, "a:b.desktop");
        compare(tasksModel.syncCalls, 1);
    }

    // addSyncedLauncherToActivity@69: the inner pre-remove branch fires only when
    // the target activity differs from currentActivity AND the launcher is on all
    // activities. Drive that branch and assert launcherInRemoving ran plus the
    // activity request/sync tail.
    function test_addSyncedLauncherToActivity_allActivitiesBranch() {
        const m = make();
        _launchers.onAllActivities = true;
        m.addSyncedLauncherToActivity(5, "a:c.desktop", "act-other");
        compare(_launchers.removingCalls, 1);
        compare(_launchers.lastRemoving, "a:c.desktop");
        compare(tasksModel.lastAddToActivity[0], "a:c.desktop");
        compare(tasksModel.lastAddToActivity[1], "act-other");
        compare(_launchers.lastChanged, "a:c.desktop");
        compare(tasksModel.syncCalls, 1);
    }

    // Same function, the skip-the-pre-remove path: launcher already on the
    // current activity (activity === currentActivity short-circuits the &&), so
    // launcherInRemoving must NOT fire but the add/sync tail still runs.
    function test_addSyncedLauncherToActivity_currentActivitySkipsRemoving() {
        const m = make();
        _launchers.onAllActivities = true;
        m.addSyncedLauncherToActivity(5, "a:d.desktop", "act-current");
        compare(_launchers.removingCalls, 0);
        compare(tasksModel.lastAddToActivity[0], "a:d.desktop");
        compare(tasksModel.lastAddToActivity[1], "act-current");
        compare(tasksModel.syncCalls, 1);
    }

    // removeSyncedLauncherFromActivity@81: launcherInRemoving fires only when the
    // removed activity IS the current one; assert that branch + the request/sync.
    function test_removeSyncedLauncherFromActivity_currentActivity() {
        const m = make();
        m.removeSyncedLauncherFromActivity(5, "a:e.desktop", "act-current");
        compare(_launchers.removingCalls, 1);
        compare(_launchers.lastRemoving, "a:e.desktop");
        compare(tasksModel.lastRemoveFromActivity[0], "a:e.desktop");
        compare(tasksModel.lastRemoveFromActivity[1], "act-current");
        compare(tasksModel.syncCalls, 1);
    }

    // Same function, non-current activity skips launcherInRemoving.
    function test_removeSyncedLauncherFromActivity_otherActivity() {
        const m = make();
        m.removeSyncedLauncherFromActivity(5, "a:f.desktop", "act-other");
        compare(_launchers.removingCalls, 0);
        compare(tasksModel.lastRemoveFromActivity[0], "a:f.desktop");
        compare(tasksModel.lastRemoveFromActivity[1], "act-other");
        compare(tasksModel.syncCalls, 1);
    }

    // dropSyncedUrls@93: matching group iterates the url array and forwards each
    // to addDroppedLauncher. Assert every item landed.
    function test_dropSyncedUrls() {
        const m = make();
        m.dropSyncedUrls(5, ["a:g.desktop", "a:h.desktop"]);
        compare(_launchers.droppedCalls, 2);
        compare(_launchers.lastDropped, "a:h.desktop");

        // wrong group -> no iteration
        m.dropSyncedUrls(99, ["a:i.desktop"]);
        compare(_launchers.droppedCalls, 2);
    }

    // validateSyncedLaunchersOrder@101: matching group AND not blocked stops the
    // validator, writes the ordered list, and restarts it. Assert the full
    // stop/write/start sequence, then the isBlocked guard suppresses it.
    function test_validateSyncedLaunchersOrder() {
        const m = make();
        const ordered = ["a:j.desktop", "a:k.desktop"];
        m.validateSyncedLaunchersOrder(5, ordered);
        compare(validator.stopCalls, 1);
        compare(validator.launchers.length, 2);
        compare(validator.launchers[1], "a:k.desktop");
        compare(validator.startCalls, 1);

        // isBlocked true -> guarded out
        m.isBlocked = true;
        m.validateSyncedLaunchersOrder(5, ["a:l.desktop"]);
        compare(validator.stopCalls, 1);
        compare(validator.startCalls, 1);
    }
}
