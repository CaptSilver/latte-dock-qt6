#!/usr/bin/env bash
# Headless interaction tests for self-contained Latte QML controls. Uses Qt Quick
# Test (qmltestrunner) to instantiate a control offscreen and synthesize real
# mouse/key events, then assert behavior — the "simulate clicking without a live
# session" layer. Pairs with qml_load_compile.sh, which only compiles.
#
# Drop new tst_*.qml cases at the top of tests/qml/ and they run automatically.
# The pkg/ subdir is excluded because those resolve their targets out of a staged
# install tree rather than the source tree; qml_pkg_test.sh runs them, and the
# coverage harness runs them again against an instrumented copy of that stage.
#
# Usage:
#   tests/manual/qml_interaction_test.sh
set -u

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${BUILD:-$REPO}"
STAGE="${STAGE:-/tmp/lattestage}"
QMLTESTRUNNER="${QMLTESTRUNNER:-/usr/lib64/qt6/bin/qmltestrunner}"

export QT_QPA_PLATFORM=offscreen

# Deploy the tree the same way qml_load_compile.sh does. Cases that instantiate
# real indicator QML pull in org.kde.latte.core and org.kde.latte.components, and
# those resolve only from an installed module tree -- without this the tests fail
# on "module is not installed" anywhere the dock is not already installed system
# wide, which is every clean checkout and every CI runner.
echo "staging $BUILD -> $STAGE ..."
if ! ( cd "$BUILD" && DESTDIR="$STAGE" cmake --install . ) >/tmp/qml-interaction-stage.log 2>&1; then
    echo "STAGE FAILED:"; tail -15 /tmp/qml-interaction-stage.log; exit 2
fi

# Run each top-level leaf/interaction test as its own qmltestrunner process.
# A single -input on the tests/qml directory would recurse into pkg/ (coverage)
# and _covself/ (fixture); multiple -input flags only honor the last file. A
# per-file loop sidesteps both while keeping each test running from its real
# repo location so its relative resource URLs still resolve.
#
# Import order matters: qmltestrunner gives the LAST -import the highest
# priority, so the staged tree has to come after the system one or a stale
# installed copy of org.kde.latte.* wins over what was just built.
status=0
for t in "$REPO"/tests/qml/tst_*.qml; do
    echo "== ${t##*/} =="
    "$QMLTESTRUNNER" -input "$t" \
        -import /usr/lib64/qt6/qml \
        -import "$STAGE/usr/lib64/qt6/qml" \
        -import "$REPO/tests/qml" || status=1
done
exit "$status"
