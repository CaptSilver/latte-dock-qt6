# Coverage gate

Measures line-level coverage for C++ (Clang source-based) and QML (execution
tracer), verifies the harness counts correctly, and ratchets against committed
baselines. Everything runs in the `fedora` distrobox (clang + llvm-tools live there,
not on the host).

## Run

    distrobox enter fedora -- bash -lc '~/build/latte-dock/tests/coverage/run.sh'

- `cxx_coverage.sh` — clang coverage build → ctest with profraw → `llvm-cov export` → `cxx-cov.json`
- `qml_coverage.sh` — instrument a repo-relative mirror of the QML → run Qt Quick Test → `qml-cov.json`
- `verify_harness.py` — fails unless the self-test fixtures (`selftest/covself.cpp`,
  `tests/qml/_covself/CovSelf.qml`) report partial coverage, proving the harness can
  tell executed from unexecuted code
- `ratchet.py` — fails if coverage drops more than 0.5pp below the committed baseline

## Rebaseline (after intentionally changing coverage)

    distrobox enter fedora -- bash -lc 'cd ~/build/latte-dock && LATTE_COVERAGE_REFRESH=1 tests/coverage/run.sh'

Commit the updated `.cxx-baseline.json` / `.qml-baseline.json`.

## Scope

`LATTE_COVERAGE` instruments the **whole `latte-dock` app target**, not just the sources
compiled into a test. So the C++ number is real whole-app line coverage: every app file
counts, and the Corona-coupled core (`view.cpp`, `lattecorona.cpp`, `positioner`, the
settings dialogs, …) sits at or near 0% until the live-dock capture exercises it — that
is honest, not a gap in the harness. The behavioral tests that link the app objects
(`universalsettingstest`, `viewsmodeltest`, …) contribute their coverage because those
objects are now instrumented; they are listed in `_latte_cov_targets` so their binaries
emit profile data and join the export object set.

## Target

Both numbers climb toward 90%+, from very different starting points: QML is already in the
60s (whole production-QML coverage), C++ starts low because the live-only runtime now counts
in its denominator. The ratchet only prevents backsliding; raising C++ means more headless
tests for the decoupleable logic plus the **live-dock capture** for the runtime core (the
only thing that exercises `view`/`positioner`/`visibilitymanager`). Not measured: standalone
`.js` files (the tracer instruments `.qml` only).
