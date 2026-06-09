// Coverage spike: drive the plasmoid's launchers-order Validator (a Timer with
// a pile of array-diffing helpers) through its public functions and the
// onTriggered handler, loading the staged/instrumented copy by file URL so
// every executed line fires a Cov tick.
//
// Validator.qml reads one ambient id, `_launchers`, which the real plasmoid
// binds to its Launchers ability. Outside that context the id is undefined and
// any dereference throws a ReferenceError. To make the unqualified `_launchers`
// resolve, we load the staged Validator through a Loader living inside a
// wrapper that *declares* `_launchers` as a property: Loader-created items
// inherit the wrapper's QML context, so the validator's unqualified lookups
// resolve to our mock and onTriggered can run end-to-end.
import QtQuick
import QtTest

TestCase {
    id: tc
    name: "Validator22"
    when: windowShown

    readonly property url target: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/abilities/launchers/Validator.qml")

    // Fresh wrapper per test: declares the ambient `_launchers` the validator
    // reads, with a fake ability that records move/sync calls and answers the
    // two queries (currentShownLauncherList, indexOfLayoutLauncher). The staged
    // Validator loads through the Loader and inherits this context.
    function makeWrapper() {
        const wrapperSrc =
            'import QtQuick\n'
          + 'Item {\n'
          + '  id: host\n'
          + '  property var moves: []\n'
          + '  property int syncs: 0\n'
          + '  property var shown: []\n'
          + '  property var layoutIndex: ({})\n'
          + '  property var _launchers: QtObject {\n'
          + '     property var tasksModel: QtObject {\n'
          + '        function move(from, to) { host.moves.push([from, to]); }\n'
          + '        function syncLaunchers() { host.syncs += 1; }\n'
          + '     }\n'
          + '     function currentShownLauncherList() { return host.shown; }\n'
          + '     function indexOfLayoutLauncher(url) {\n'
          + '        return host.layoutIndex[url] !== undefined ? host.layoutIndex[url] : -1;\n'
          + '     }\n'
          + '  }\n'
          + '  property Loader vLoader: Loader { source: host.vSource }\n'
          + '  property url vSource\n'
          + '}\n';
        const w = Qt.createQmlObject(wrapperSrc, tc, "validatorWrapper");
        verify(w, "wrapper create failed");
        // Set the source last so the validator is created with the mock wired.
        w.vSource = target;
        return w;
    }

    function validatorOf(w) {
        tryVerify(function() { return w.vLoader.status === Loader.Ready
                                   || w.vLoader.status === Loader.Error; }, 4000);
        verify(w.vLoader.status === Loader.Ready,
               "validator load failed status=" + w.vLoader.status);
        const item = w.vLoader.item;
        verify(item, "no loaded validator item");
        return item;
    }

    // launcherValidPos: linear search of the `launchers` array.
    function test_launcherValidPos() {
        const w = makeWrapper();
        const m = validatorOf(w);

        m.launchers = ["a", "b", "c"];
        compare(m.launcherValidPos("a"), 0);
        compare(m.launcherValidPos("b"), 1);
        compare(m.launcherValidPos("c"), 2);
        compare(m.launcherValidPos("zzz"), -1);
        m.launchers = [];
        compare(m.launcherValidPos("a"), -1);
    }

    // arraysAreEqual: size mismatch, element mismatch, full equality.
    function test_arraysAreEqual() {
        const w = makeWrapper();
        const m = validatorOf(w);

        verify(m.arraysAreEqual([], []));
        verify(m.arraysAreEqual(["a", "b"], ["a", "b"]));
        verify(!m.arraysAreEqual(["a"], ["a", "b"]));        // size mismatch branch
        verify(!m.arraysAreEqual(["a", "b"], ["a", "c"]));   // element mismatch branch
        verify(!m.arraysAreEqual([], ["x"]));
    }

    // upwardIsBetter: equal arrays return false early; a single up-move that
    // reconciles returns true; a mismatch that doesn't reconcile returns false.
    function test_upwardIsBetter() {
        const w = makeWrapper();
        const m = validatorOf(w);

        // Already equal -> the outer guard returns false.
        verify(!m.upwardIsBetter(["a", "b", "c"], ["a", "b", "c"]));

        // current=[b,a,c], goal=[a,b,c]: moving b to a's slot reconciles -> true.
        verify(m.upwardIsBetter(["b", "a", "c"], ["a", "b", "c"]));

        // current=[c,b,a], goal=[a,b,c]: one splice won't reconcile -> false.
        verify(!m.upwardIsBetter(["c", "b", "a"], ["a", "b", "c"]));
    }

    // launchersAreInSync: equal current+target -> true; differing -> false. Reads
    // _launchers.currentShownLauncherList() (resolved via the wrapper context).
    function test_launchersAreInSync() {
        const w = makeWrapper();
        const m = validatorOf(w);

        w.shown = ["a", "b"];
        m.launchers = ["a", "b"];
        verify(m.launchersAreInSync());

        m.launchers = ["a", "c"];
        verify(!m.launchersAreInSync());
    }

    // onTriggered, synced branch: launchers match -> stop(), clear, syncLaunchers().
    function test_triggered_synced() {
        const w = makeWrapper();
        const m = validatorOf(w);

        w.shown = ["a", "b"];
        m.launchers = ["a", "b"];
        m.interval = 1;
        m.restart();
        tryVerify(function() { return w.syncs >= 1; }, 2000, "sync never fired");
        verify(!m.running);                 // stop() ran
        compare(m.launchers.length, 0);     // launchers cleared
    }

    // onTriggered, UPWARD branch: a single up-move reconciles, so the handler
    // takes the upwardIsBetter path, finds a valid pos + layout index, calls
    // move() and restart()s.
    function test_triggered_upward_move() {
        const w = makeWrapper();
        const m = validatorOf(w);

        // current=[b,a], goal=[a,b] -> upwardIsBetter true; first mismatch at i=0
        // is "b": launcherValidPos("b")=1 in goal, layout index for b known.
        w.shown = ["b", "a"];
        w.layoutIndex = { "b": 1, "a": 0 };
        m.launchers = ["a", "b"];

        m.interval = 1;
        m.restart();
        tryVerify(function() { return w.moves.length >= 1; }, 2000, "move never fired");
        compare(w.moves[0][1], 1);          // moved to b's goal position
        verify(m.running);                  // restart() left it running
        m.stop();
    }

    // onTriggered, UPWARD branch where the layout lookup misses -> stop(), no move.
    function test_triggered_upward_layout_miss() {
        const w = makeWrapper();
        const m = validatorOf(w);

        w.shown = ["b", "a"];
        w.layoutIndex = {};                 // indexOfLayoutLauncher -> -1
        m.launchers = ["a", "b"];

        m.interval = 1;
        m.restart();
        // The handler stops itself when the launcher isn't in the model.
        tryVerify(function() { return !m.running; }, 2000, "did not stop on layout miss");
        compare(w.moves.length, 0);
    }

    // onTriggered, DOWNWARD branch: a diff that upwardIsBetter rejects sends the
    // handler down the reverse-iteration path; here the layout lookup succeeds
    // so it moves and restarts.
    function test_triggered_downward_move() {
        const w = makeWrapper();
        const m = validatorOf(w);

        // current=[c,b,a], goal=[a,b,c]: upwardIsBetter false -> DOWNWARD. Reverse
        // scan: i=2 "a" mismatches goal[2]="c"; pos of "a" in goal = 0.
        w.shown = ["c", "b", "a"];
        w.layoutIndex = { "a": 2, "b": 1, "c": 0 };
        m.launchers = ["a", "b", "c"];

        m.interval = 1;
        m.restart();
        tryVerify(function() { return w.moves.length >= 1; }, 2000, "downward move never fired");
        compare(w.moves[0][1], 0);          // "a" moved to its goal slot 0
        verify(m.running);
        m.stop();
    }

    // onTriggered, DOWNWARD branch with a layout miss -> stop(), no move.
    function test_triggered_downward_layout_miss() {
        const w = makeWrapper();
        const m = validatorOf(w);

        w.shown = ["c", "b", "a"];
        w.layoutIndex = {};                 // indexOfLayoutLauncher -> -1
        m.launchers = ["a", "b", "c"];

        m.interval = 1;
        m.restart();
        tryVerify(function() { return !m.running; }, 2000, "did not stop on downward layout miss");
        compare(w.moves.length, 0);
    }

    // Sanity: the default interval is what the source declares.
    function test_default_interval() {
        const w = makeWrapper();
        const m = validatorOf(w);
        compare(m.interval, 400);
    }
}
