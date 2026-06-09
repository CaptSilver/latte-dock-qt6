# Package-QML coverage tests

Each `tst_*.qml` here drives a real Latte package QML component to produce honest
execution coverage. A test earns a unit's coverage **only** if all of these hold:

1. **Load the instrumented staged copy** — not the repo copy:
   `readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/<staged-path>")`
   (the staged tree carries the `Cov.tick` instrumentation; the repo copy does not).
2. **Honest mock context.** Latte components read unqualified context names. QML resolves
   those against the component's *creation context*, so name the TestCase `id: root` and
   declare every name the component reads as a property / lowercase-`id`'d QtObject on it:

       TestCase {
           id: root
           property bool isVertical: false
           QtObject { id: animations; property QtObject speedFactor: QtObject { property real current: 1 } }
           // ...declare every unqualified name the target reads...
           function test_x() {
               const c = Qt.createComponent(targetUrl);
               verify(c.status === Component.Ready, c.errorString());
               const obj = createTemporaryObject(c, root, {});
               // ...
           }
       }

   Mocks must be shaped like the real object — never a catch-all that swallows every call.
3. **Genuine execution — BANNED:**
   - No `safe()` / `try{}catch{}` that swallows a throw so the entry tick banks while the
     body fails. If a unit throws on a missing global, mock the global or don't claim the unit.
   - No "construction-only" credit: if the object can't be retained (C++ attached-property
     failure, etc.) and only `Component.onCompleted` fires during a torn-down incubation,
     that does not count.
4. **Real assertions.** Every unit a test claims must assert an observable effect — a return
   value, property change, signal emission, or mock side-effect. Execute-but-assert-nothing
   does not count.
5. **Deterministic + non-disruptive** — passes offscreen, stable across runs, doesn't break
   the merged `qml_coverage.sh` run.

A unit that can't meet 1–4 headlessly is **live-only** — add it to `tests/coverage/live-only.md`,
don't game it.

Run the lint before review: `python3 tests/coverage/lint_pkg_tests.py --dir tests/qml/pkg`.
