#!/usr/bin/env bash
# QML execution coverage for latte-dock. Two runs feed one report:
#   RUN 1 instruments a repo-relative MIRROR of the production QML (+ the QML
#         self-test fixture) and runs the leaf-component Qt Quick Test suite.
#   RUN 2 instruments the STAGED install tree (so org.kde.latte.* module imports
#         resolve) and runs the package tests under tests/qml/pkg against it.
# The two catalogs are remapped to repo-relative keys, merged, and reported as
# build/_coverage/qml-cov.json. Needs qmltestrunner + the Qt6/KF6 build deps.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
MIRROR="${MIRROR:-$REPO/build/_qmlcov/instrumented}"
STAGE="${STAGE:-$REPO/build/_qmlcov/stage}"
OUT="${OUT:-$REPO/build/_coverage}"
QMLTESTRUNNER="${QMLTESTRUNNER:-/usr/lib64/qt6/bin/qmltestrunner}"
mkdir -p "$OUT" "$REPO/build/_qmlcov"

# Ticks ride on console.warn; if Qt Test ever hits its warning cap and silences
# output, every tick past that point vanishes and coverage reads falsely low
# (this silently capped the suite at the first ~10 test files once). Fail loudly
# rather than report a truncated number.
_guard_no_warning_cap() {
    if grep -q "Maximum amount of warnings exceeded" "$1"; then
        echo "FATAL: qmltestrunner ($2) hit its warning cap and silenced ticks." >&2
        echo "       Coverage would be undercounted. Raise -maxwarnings." >&2
        exit 1
    fi
}

# ------------------------------------------------------------------ RUN 1 ----
# Repo-relative instrumented mirror of tests/qml + production dirs, exercised by
# the leaf-component suite. Covers the leaf components and the _covself fixture.
CAT_MIRROR="$OUT/cat-mirror.json"
RUN_MIRROR="$OUT/run-mirror.txt"
rm -rf "$MIRROR"; mkdir -p "$MIRROR/tests"

echo "== copy tests/qml verbatim into mirror (minus the staged pkg tests) =="
cp -r "$REPO/tests/qml" "$MIRROR/tests/qml"
# The pkg tests load their target from the staged tree by a build-relative URL;
# they belong to RUN 2 only, so keep them out of the mirror's -input set.
rm -rf "$MIRROR/tests/qml/pkg"

echo "== instrument production QML + QML self-test fixture (overwrites mirror copies) =="
python3 "$REPO/tools/qmlcov/instrument.py" \
    --root "$REPO" \
    --include declarativeimports --include plasmoid --include shell --include containment \
    --include indicators --include tests/qml/_covself \
    --out "$MIRROR" --catalog "$CAT_MIRROR"

echo "== run Qt Quick Test against the instrumented mirror =="
# qmltestrunner prints test results and the QML console.log __COV_TICK__ markers
# to stdout, while diagnostics may land on stderr; merge both into the runlog so
# report.py sees every tick regardless of which stream Qt routes it to. The pkg
# tests live outside this mirror, so they are not picked up here.
# Each Cov tick is a console.warn (QWARN); Qt Test silences output after 2000
# warnings by default, which would drop ticks from every test past the cap.
# -maxwarnings 0 lifts the cap; _guard_no_warning_cap re-checks per run.
QT_QPA_PLATFORM=offscreen "$QMLTESTRUNNER" \
    -maxwarnings 0 \
    -input "$MIRROR/tests/qml" \
    -import /usr/lib64/qt6/qml \
    -import "$MIRROR/tests/qml" \
    > "$RUN_MIRROR" 2>&1 || {
        echo "qmltestrunner (mirror) failed:"; tail -30 "$RUN_MIRROR"; exit 1; }
_guard_no_warning_cap "$RUN_MIRROR" mirror

# ------------------------------------------------------------------ RUN 2 ----
# Instrument the staged install tree so package QML that imports org.kde.latte.*
# modules can be loaded and driven. The pkg test loads its target component from
# $STAGE by a build-relative file URL.
CAT_STAGED="$OUT/cat-staged.json"
RUN_STAGED="$OUT/run-staged.txt"

echo "== stage the install tree =="
rm -rf "$STAGE"
( cd "$REPO/build-coverage" && DESTDIR="$STAGE" cmake --install . ) >/dev/null

echo "== instrument the staged install tree =="
python3 "$REPO/tools/qmlcov/instrument.py" --root "$STAGE" \
    --include usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents \
    --include usr/share/plasma/plasmoids/org.kde.latte.containment/contents \
    --include usr/share/plasma/shells/org.kde.latte.shell/contents \
    --include usr/lib64/qt6/qml/org/kde/latte/core \
    --include usr/lib64/qt6/qml/org/kde/latte/components \
    --include usr/lib64/qt6/qml/org/kde/latte/abilities \
    --out "$STAGE" --catalog "$CAT_STAGED"

# The instrumenter injects `import Cov 1.0`; drop the Cov module where the staged
# imports can find it.
cp -r "$REPO/tests/qml/Cov" "$STAGE/Cov"

echo "== run Qt Quick Test against the staged tree =="
QT_QPA_PLATFORM=offscreen "$QMLTESTRUNNER" \
    -maxwarnings 0 \
    -input "$REPO/tests/qml/pkg" \
    -import /usr/lib64/qt6/qml \
    -import "$STAGE/usr/lib64/qt6/qml" \
    -import "$STAGE" \
    > "$RUN_STAGED" 2>&1 || {
        echo "qmltestrunner (staged) failed:"; tail -40 "$RUN_STAGED"; exit 1; }
_guard_no_warning_cap "$RUN_STAGED" staged

# ----------------------------------------------------------- MERGE + REPORT --
CAT_MERGED="$OUT/cat-merged.json"
RUNLOG="$OUT/qml-runlog.txt"

echo "== merge catalogs (remap staged prefixes -> repo-relative, dedup) =="
python3 "$REPO/tests/coverage/remap_catalog.py" \
    --catalog "$CAT_MIRROR" --catalog "$CAT_STAGED" \
    --out "$CAT_MERGED"

# The staged ticks carry staged-prefix keys; remap them so they match the
# now-repo-relative merged catalog before report.py unions everything.
RUN_STAGED_REMAPPED="$OUT/run-staged-remapped.txt"
python3 "$REPO/tests/coverage/remap_catalog.py" \
    --runlog "$RUN_STAGED" --runlog-out "$RUN_STAGED_REMAPPED"

cat "$RUN_MIRROR" "$RUN_STAGED_REMAPPED" > "$RUNLOG"
cp "$RUNLOG" "$OUT/qml-stdout.txt"

echo "== report =="
python3 "$REPO/tools/qmlcov/report.py" \
    --catalog "$CAT_MERGED" --runlog "$RUNLOG" --threshold 0 --json-out "$OUT/qml-cov.json"
