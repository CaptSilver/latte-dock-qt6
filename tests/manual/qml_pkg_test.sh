#!/usr/bin/env bash
# Runs the tests/qml/pkg suite: the package-level QML tests that instantiate real
# shell/containment/plasmoid components out of a staged install tree.
#
# These do NOT need the Cov module -- that is an artifact of the coverage harness
# instrumenting its own copy of the stage. A plain stage runs them fine.
#
# They resolve their targets through the Stage singleton, which is dropped into the
# stage below, so this runner needs no scratch copy and never writes near the
# coverage harness's own stage.
#
# Usage:
#   tests/manual/qml_pkg_test.sh
set -u

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
STAGE="${STAGE:-/tmp/lattestage}"
QMLTESTRUNNER="${QMLTESTRUNNER:-/usr/lib64/qt6/bin/qmltestrunner}"

if [ ! -d "$STAGE/usr/share/plasma" ]; then
    echo "no staged install at $STAGE (expected the shellpackage fixture to provide it)"; exit 2
fi

QMLREL="$("$REPO/tests/coverage/drop_stage_module.sh" "$STAGE")" || exit 2

count=$(ls "$REPO"/tests/qml/pkg/tst_*.qml | wc -l)
if [ "$count" -eq 0 ]; then
    echo "no package tests found under $REPO/tests/qml/pkg"; exit 2
fi
echo "running $count package test files against $STAGE (qml modules in $QMLREL)"

QT_QPA_PLATFORM=offscreen "$QMLTESTRUNNER" -maxwarnings 0 \
    -input "$REPO/tests/qml/pkg" \
    -import /usr/lib64/qt6/qml \
    -import "$STAGE/$QMLREL" \
    -import "$STAGE"
