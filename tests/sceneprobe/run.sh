#!/usr/bin/env bash
# Deterministic render gate for Latte QML: runs latte-sceneprobe over every scene in
# scenes/ through a throwaway nested kwin (lavapipe + LP_NUM_THREADS=0), and fails if any
# real scene fails. It first runs a self-test — the known-good scene must pass and the
# known-bad scene must fail — so a broken gate is caught before trusting its verdicts.
#
# Usage (inside the fedora distrobox):
#   tests/sceneprobe/run.sh [build-dir]   (default: build-asan if built, else build)
set -u
SCENEPROBE_DEVICE=lavapipe
BLESS=0
while [ $# -gt 0 ]; do
  case "$1" in
    --device) SCENEPROBE_DEVICE="${2:-lavapipe}"; shift 2;;
    --bless) BLESS=1; shift;;
    *) break;;
  esac
done
export SCENEPROBE_DEVICE

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$REPO/tests/sceneprobe"
WRAP="$HERE/run_in_kwin.sh"
BUILD="${1:-$REPO/build-asan}"
[ -x "$BUILD/bin/latte-sceneprobe" ] || BUILD="$REPO/build"
PROBE="$BUILD/bin/latte-sceneprobe"
[ -x "$PROBE" ] || { echo "no latte-sceneprobe at $PROBE (build it first)"; exit 2; }
echo "device mode: $SCENEPROBE_DEVICE"

OUT="$(mktemp)"; trap 'rm -f "$OUT"' EXIT

# Stage the current source's Latte QML modules into a fresh dir so scenes can import the
# real org.kde.latte.components types — the system copy in this environment can be stale
# (qmldir lists files that aren't installed). The modules are source .qml copied by install,
# so they reflect current source regardless of build state. Installs from the normal build.
STAGE_BUILD="$REPO/build"
[ -d "$STAGE_BUILD" ] || { echo "no build dir at $STAGE_BUILD to stage QML from"; exit 2; }
STAGE="$(mktemp -d)"
trap 'rm -f "$OUT"; rm -rf "$STAGE"' EXIT
if ! DESTDIR="$STAGE" cmake --install "$STAGE_BUILD" >/tmp/sceneprobe-stage.log 2>&1; then
    echo "QML staging failed (cmake --install):"; tail -8 /tmp/sceneprobe-stage.log; exit 2
fi

export LATTE_VK_SUPPRESSIONS="$HERE/vk-suppressions.txt"
export LATTE_QML_IMPORT_PATH="$STAGE/usr/lib64/qt6/qml"
export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:exitcode=99"
export SCENEPROBE_ARTIFACTS="${SCENEPROBE_ARTIFACTS:-/tmp/latte-sceneprobe-artifacts}"
mkdir -p "$SCENEPROBE_ARTIFACTS"
rm -f "$SCENEPROBE_ARTIFACTS"/*.actual.png "$SCENEPROBE_ARTIFACTS"/*.diff.png "$SCENEPROBE_ARTIFACTS"/*.expected.png 2>/dev/null
echo "artifacts: $SCENEPROBE_ARTIFACTS"
[ "$BLESS" -eq 1 ] && export SCENEPROBE_BLESS=1

run_scene(){ "$WRAP" "$PROBE" "$1" >"$OUT" 2>&1; return $?; }

# Self-test: good must pass, bad must fail, or the gate itself is broken.
run_scene "$HERE/scenes/selftest-good.qml" || { echo "GATE BROKEN: selftest-good failed"; cat "$OUT"; exit 3; }
run_scene "$HERE/scenes/selftest-bad.qml"; rc=$?
[ "$rc" -eq 1 ] || { echo "GATE BROKEN: selftest-bad exited $rc, expected 1"; cat "$OUT"; exit 3; }
echo "self-test ok (good passes, bad fails)"
run_scene "$HERE/scenes/selftest-blank.qml"; rc=$?
[ "$rc" -eq 1 ] || { echo "GATE BROKEN: selftest-blank exited $rc, expected 1 (output floor)"; cat "$OUT"; exit 3; }
echo "self-test ok (output floor catches blank)"

fails=0
for s in "$HERE"/scenes/*.qml; do
    case "$s" in *selftest-*) continue;; esac
    if run_scene "$s"; then
        echo "PASS  $(basename "$s")"
        if [ "$BLESS" -eq 1 ]; then
            base="${s%.qml}"; cand="$SCENEPROBE_ARTIFACTS/$(basename "$base").actual.png"
            if [ -f "$cand" ]; then cp "$cand" "${base}.expected.${SCENEPROBE_DEVICE}.png"; echo "  blessed $(basename "${base}").expected.${SCENEPROBE_DEVICE}.png"; fi
        fi
    else
        echo "FAIL  $(basename "$s")"; cat "$OUT"; fails=$((fails+1))
    fi
done

echo "---- $fails scene(s) failed ----"
[ "$fails" -eq 0 ] && { echo "RENDER GATE: PASS"; exit 0; } || { echo "RENDER GATE: FAIL"; exit 1; }
