// Drives the plasmoid's PulseAudio helper QtObject through its pure public
// functions. The component is loaded from the staged (instrumented) package
// by file URL so the Cov.tick calls fire, and every assertion pins an
// observable effect: a return value, a property change, or a signal emission.
//
// The only unqualified creation-context name the component reads is `backend`
// (streamsForPid calls backend.parentPid inside its second loop). We declare a
// shaped mock for it on the TestCase root that records the pid it was asked
// about, so the call — if reached — is asserted rather than throwing.
//
// Headless reality (probed): the Instantiator's PulseObjectFilterModel has no
// active sink-inputs without a live PulseAudio session, so instantiator.count
// is 0. That makes the delegate body (the per-stream QtObject with mute/unmute/
// increaseVolume/decreaseVolume) never materialise, and the inner match/parentPid
// branches of findStreams/streamsForPid never execute. Those are live-only; we
// cover the function entries and the empty-set returns that DO run honestly.
import QtQuick
import QtTest

TestCase {
    id: root
    name: "PulseAudio"
    when: windowShown

    // The single unqualified name the target resolves against its creation
    // context. streamsForPid calls backend.parentPid(stream.pid) only when a
    // real stream is present; shaped (not catch-all) and records the argument.
    QtObject {
        id: backend
        property int lastPid: -999
        function parentPid(pid) { lastPid = pid; return pid + 1000; }
    }

    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/PulseAudio.qml")

    function make() {
        const c = Qt.createComponent(targetUrl);
        verify(c.status === Component.Ready, "compile failed: " + c.errorString());
        const obj = createTemporaryObject(c, root, {});
        verify(obj, "instantiate failed");
        return obj;
    }

    // boundVolume clamps into [PulseAudio.MinimalVolume, maxVolumeValue]. The
    // PulseAudio.NormalVolume-derived maxVolumeValue evaluates to a concrete
    // value here (81920 = round(125 * 65536 / 100)); assert all three legs of
    // the Math.max(min, Math.min(v, max)) clamp.
    function test_boundVolume_clamps() {
        const m = make();
        // maxVolumeValue is a live binding off PulseAudio.NormalVolume; pin it.
        compare(m.maxVolumeValue, 81920);
        // above the ceiling -> clamped down to maxVolumeValue
        compare(m.boundVolume(1000000000), m.maxVolumeValue);
        // below the floor -> clamped up to MinimalVolume (0)
        compare(m.boundVolume(-5), 0);
        // in range -> passes through unchanged
        compare(m.boundVolume(40000), 40000);
    }

    // registerPidMatch records a new app and fires streamsChanged exactly once;
    // a duplicate registration is a no-op and must NOT re-emit (the guard exists
    // to avoid infinite recursion). hasPidMatch reflects the recorded state.
    function test_registerAndHasPidMatch() {
        const m = make();
        const spy = createTemporaryObject(signalSpyComponent, root,
                                          {target: m, signalName: "streamsChanged"});
        verify(!m.hasPidMatch("app1"));

        m.registerPidMatch("app1");
        verify(m.hasPidMatch("app1"));
        compare(spy.count, 1);

        // already present -> guarded early-out, no second emission
        m.registerPidMatch("app1");
        verify(m.hasPidMatch("app1"));
        compare(spy.count, 1);

        // an unregistered name still reports false
        verify(!m.hasPidMatch("other"));
    }

    // findStreams walks instantiator (count 0 headless) and returns an empty
    // array; streamsForAppName forwards to it. Asserting the empty result pins
    // the entry + empty-loop return path that runs honestly. The per-stream
    // match branch needs a live PulseAudio stream -> live-only.
    function test_findStreams_emptyModel() {
        const m = make();
        const byKey = m.findStreams("appName", "nothing");
        verify(Array.isArray(byKey));
        compare(byKey.length, 0);

        const byApp = m.streamsForAppName("nothing");
        verify(Array.isArray(byApp));
        compare(byApp.length, 0);
    }

    // streamsForPid: first findStreams("pid", ...) is empty, so it enters the
    // fallback loop. With count 0 that loop never iterates, backend.parentPid is
    // never called, and it returns []. Assert the empty return and that the
    // backend mock stayed untouched (proving the parentPid branch was not hit).
    function test_streamsForPid_emptyModel() {
        const m = make();
        backend.lastPid = -999;
        const streams = m.streamsForPid(42);
        verify(Array.isArray(streams));
        compare(streams.length, 0);
        // no stream present -> the parentPid fallback branch was not reached
        compare(backend.lastPid, -999);
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
