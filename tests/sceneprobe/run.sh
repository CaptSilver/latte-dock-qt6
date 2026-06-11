#!/usr/bin/env bash
# Deterministic render gate for Latte QML: runs latte-sceneprobe over every scene in
# scenes/ through a throwaway nested kwin (lavapipe + LP_NUM_THREADS=0), and fails if any
# real scene fails. It first runs a self-test — the known-good scene must pass and the
# known-bad scene must fail — so a broken gate is caught before trusting its verdicts.
#
# Usage (inside the fedora distrobox):
#   tests/sceneprobe/run.sh [build-dir]   (default: build-asan if built, else build)
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$REPO/tests/sceneprobe"
WRAP="$HERE/run_in_kwin.sh"
BUILD="${1:-$REPO/build-asan}"
[ -x "$BUILD/bin/latte-sceneprobe" ] || BUILD="$REPO/build"
PROBE="$BUILD/bin/latte-sceneprobe"
[ -x "$PROBE" ] || { echo "no latte-sceneprobe at $PROBE (build it first)"; exit 2; }

export LATTE_VK_SUPPRESSIONS="$HERE/vk-suppressions.txt"
export LATTE_QML_IMPORT_PATH="${LATTE_QML_IMPORT_PATH:-/usr/lib64/qt6/qml}"
export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:exitcode=99"

run_scene(){ "$WRAP" "$PROBE" "$1" >/tmp/sceneprobe.out 2>&1; return $?; }

# Self-test: good must pass, bad must fail, or the gate itself is broken.
run_scene "$HERE/scenes/selftest-good.qml" || { echo "GATE BROKEN: selftest-good failed"; cat /tmp/sceneprobe.out; exit 3; }
if run_scene "$HERE/scenes/selftest-bad.qml"; then echo "GATE BROKEN: selftest-bad passed"; exit 3; fi
echo "self-test ok (good passes, bad fails)"

fails=0
for s in "$HERE"/scenes/*.qml; do
    case "$s" in *selftest-*) continue;; esac
    if run_scene "$s"; then echo "PASS  $(basename "$s")"; else echo "FAIL  $(basename "$s")"; cat /tmp/sceneprobe.out; fails=$((fails+1)); fi
done

echo "---- $fails scene(s) failed ----"
[ "$fails" -eq 0 ] && { echo "RENDER GATE: PASS"; exit 0; } || { echo "RENDER GATE: FAIL"; exit 1; }
