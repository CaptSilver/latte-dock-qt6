#!/usr/bin/env bash
# QML execution coverage for latte-dock. Builds a repo-relative instrumented
# mirror of the production QML (+ the QML self-test fixture), runs the Qt Quick
# Test suite against it, and writes build/_coverage/qml-cov.json.
# Run inside the fedora distrobox.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
MIRROR="${MIRROR:-$REPO/build/_qmlcov/instrumented}"
CAT="${CAT:-$REPO/build/_qmlcov/catalog.json}"
OUT="${OUT:-$REPO/build/_coverage}"
QMLTESTRUNNER="${QMLTESTRUNNER:-/usr/lib64/qt6/bin/qmltestrunner}"
mkdir -p "$OUT" "$(dirname "$CAT")"
rm -rf "$MIRROR"; mkdir -p "$MIRROR/tests"

echo "== copy tests/qml verbatim into mirror =="
cp -r "$REPO/tests/qml" "$MIRROR/tests/qml"

echo "== instrument production QML + QML self-test fixture (overwrites mirror copies) =="
python3 "$REPO/tools/qmlcov/instrument.py" \
    --root "$REPO" \
    --include declarativeimports --include plasmoid --include shell --include containment \
    --include tests/qml/_covself \
    --out "$MIRROR" --catalog "$CAT"

echo "== run Qt Quick Test against the instrumented mirror =="
# qmltestrunner prints test results and the QML console.log __COV_TICK__ markers
# to stdout, while diagnostics may land on stderr; merge both into the runlog so
# report.py sees every tick regardless of which stream Qt routes it to.
RUNLOG="$OUT/qml-runlog.txt"
QT_QPA_PLATFORM=offscreen "$QMLTESTRUNNER" \
    -input "$MIRROR/tests/qml" \
    -import /usr/lib64/qt6/qml \
    -import "$MIRROR/tests/qml" \
    > "$RUNLOG" 2>&1 || {
        echo "qmltestrunner failed:"; tail -30 "$RUNLOG"; exit 1; }
cp "$RUNLOG" "$OUT/qml-stdout.txt"

echo "== report =="
python3 "$REPO/tools/qmlcov/report.py" \
    --catalog "$CAT" --runlog "$RUNLOG" --threshold 0 --json-out "$OUT/qml-cov.json"
