#!/usr/bin/env bash
# Headless interaction tests for self-contained Latte QML controls. Uses Qt Quick
# Test (qmltestrunner) to instantiate a control offscreen and synthesize real
# mouse/key events, then assert behavior — the "simulate clicking without a live
# session" layer. Pairs with qml_load_compile.sh, which only compiles.
#
# Drop new tst_*.qml cases at the top of tests/qml/ and they run automatically.
# The pkg/ subdir is deliberately excluded: those load Cov-instrumented staged
# copies and are run by tests/coverage/qml_coverage.sh, not standalone here —
# running them under a plain qmltestrunner fails on the missing Cov module.
#
# Usage:
#   tests/manual/qml_interaction_test.sh
set -u

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
QMLTESTRUNNER="${QMLTESTRUNNER:-/usr/lib64/qt6/bin/qmltestrunner}"

export QT_QPA_PLATFORM=offscreen

# Run each top-level leaf/interaction test as its own qmltestrunner process.
# A single -input on the tests/qml directory would recurse into pkg/ (coverage)
# and _covself/ (fixture); multiple -input flags only honor the last file. A
# per-file loop sidesteps both while keeping each test running from its real
# repo location so its relative resource URLs still resolve.
status=0
for t in "$REPO"/tests/qml/tst_*.qml; do
    echo "== ${t##*/} =="
    "$QMLTESTRUNNER" -input "$t" -import /usr/lib64/qt6/qml -import "$REPO/tests/qml" || status=1
done
exit "$status"
