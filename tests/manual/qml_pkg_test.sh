#!/usr/bin/env bash
# Runs the tests/qml/pkg suite: the package-level QML tests that instantiate real
# shell/containment/plasmoid components out of a staged install tree.
#
# These do NOT need the Cov module. Each one resolves its target through a
# hardcoded relative path, ../../../build/_qmlcov/stage/usr/share/..., so all they
# need is a stage sitting at that spot. The coverage harness happens to point that
# name at an instrumented stage, which is where the "they need Cov" belief came
# from; a plain stage runs them fine.
#
# Rather than rewrite the path in every test, this gives the relative walk a
# scratch root to land in and symlinks the real stage underneath it. That also
# keeps ctest from writing anywhere near the coverage harness's own stage, which
# tests/coverage/qml_coverage.sh destroys and recreates.
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

scratch="$(mktemp -d)"
mkdir -p "$scratch/tests/qml" "$scratch/build/_qmlcov"
cp -r "$REPO/tests/qml/pkg" "$scratch/tests/qml/pkg"
ln -s "$STAGE" "$scratch/build/_qmlcov/stage"
# Left behind on purpose when something fails: the scratch copy is what was run.
echo "pkg scratch: $scratch"

# A partial copy would still exit 0, just with fewer cases.
want=$(ls "$REPO"/tests/qml/pkg/tst_*.qml | wc -l)
got=$(ls "$scratch"/tests/qml/pkg/tst_*.qml | wc -l)
if [ "$want" != "$got" ]; then
    echo "copied $got of $want pkg tests"; exit 2
fi
echo "running $got package tests against $STAGE"

QT_QPA_PLATFORM=offscreen "$QMLTESTRUNNER" -maxwarnings 0 \
    -input "$scratch/tests/qml/pkg" \
    -import /usr/lib64/qt6/qml \
    -import "$STAGE/usr/lib64/qt6/qml" \
    -import "$STAGE"
status=$?

if [ "$status" -eq 0 ]; then
    rm -rf "$scratch"
fi

exit "$status"
