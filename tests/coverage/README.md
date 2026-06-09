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

## Target

Both baselines climb toward 90%+. The ratchet only prevents backsliding; raising the
number means writing tests (QML instantiation tests behind stubs; C++ real-link tests
as Corona-coupled logic is decoupled). Not measured here: standalone `.js` files
(the tracer instruments `.qml` only) and Corona/View C++ not linked into any test
(tracked separately by the live-dock capture — see the follow-on plans).
