#!/usr/bin/env bash
# Live-dock C++ coverage capture. Runs the INSTRUMENTED latte-dock under a nested
# kwin_wayland, drives it over DBus (add/query/remove a widget), then exits it
# cleanly with SIGINT so LLVM flushes profraw. Merges that with the headless test
# profraw and reports combined whole-app coverage — the only thing that exercises
# the live runtime core (view/positioner/lattecorona/visibilitymanager).
#
# Run AFTER cxx_coverage.sh has built build-coverage and produced the headless profraw:
#   tests/coverage/cxx_coverage.sh && tests/coverage/live_capture.sh
#
# This is NOT part of the ratcheted gate (run.sh) — the nested compositor is too
# flaky to gate on. It is an on-demand measurement of how far the live run climbs
# the whole-app number. WIDGET= overrides the widget added.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COV="${COV_DIR:-$REPO/build-coverage}"
OUT="$REPO/build/_coverage"
DOCK="$COV/bin/latte-dock"
WIDGET="${WIDGET:-org.kde.plasma.systemmonitor}"
mkdir -p "$OUT"

[ -x "$DOCK" ] || { echo "no instrumented dock at $DOCK — run cxx_coverage.sh first"; exit 2; }

WORK="$(mktemp -d /tmp/latte-live.XXXXXX)"
STAGE="$WORK/stage"; HOMEDIR="$WORK/home"; LIVEDIR="$WORK/profraw"
mkdir -p "$HOMEDIR/.config/latte" "$LIVEDIR"
trap 'rm -rf "$WORK"' EXIT

echo "== stage the instrumented install tree =="
DESTDIR="$STAGE" cmake --install "$COV" >"$WORK/install.log" 2>&1 \
    || { echo "stage install failed"; tail -10 "$WORK/install.log"; exit 2; }

SESS="$WORK/session.sh"
cat > "$SESS" <<EOF
#!/bin/bash
set -u
export HOME="$HOMEDIR" USER=lattelive USERNAME=lattelive
export QT_QPA_PLATFORM=wayland
export XDG_DATA_DIRS="$STAGE/usr/share:\${XDG_DATA_DIRS:-/usr/share}"
export QML_IMPORT_PATH="$STAGE/usr/lib64/qt6/qml" QML2_IMPORT_PATH="$STAGE/usr/lib64/qt6/qml"
# one profraw per process; %p keeps forks distinct.
export LLVM_PROFILE_FILE="$LIVEDIR/dock-%p.profraw"
"$DOCK" --debug --layout Default --log-file "$WORK/dock.log" >"$WORK/dock.out" 2>&1 &
DOCKPID=\$!
dctl(){ busctl --user call org.kde.lattedock /Latte org.kde.LatteDock "\$@" 2>/dev/null; }

up=0; for i in \$(seq 1 30); do
  busctl --user list 2>/dev/null | grep -q org.kde.lattedock && { up=1; break; }
  kill -0 \$DOCKPID 2>/dev/null || break
  sleep 1
done
[ "\$up" = 1 ] || { echo "RESULT: dock-never-came-up"; tail -20 "$WORK/dock.log"; exit 1; }
sleep 5

# Drive a spread of runtime paths: enumerate, add a widget, re-enumerate, remove it.
CID=\$(dctl containmentIds | awk '{print \$3}')
echo "containment=\$CID"
dctl layouts >/dev/null
before=\$(dctl appletIds u "\$CID")
dctl addApplet us "\$CID" "$WIDGET"; sleep 3
after=\$(dctl appletIds u "\$CID")
NEW=""; for t in \$(echo "\$after" | tr ' ' '\n' | grep -E '^[0-9]+\$'); do
  echo "\$before" | grep -qw "\$t" || NEW="\$t"; done
echo "added applet=\$NEW"
if [ -n "\$NEW" ]; then dctl triggerAppletAction uus "\$CID" "\$NEW" "remove"; sleep 3; fi
echo "RESULT: drove add/remove"

# Clean exit so LLVM's atexit writes profraw (SIGTERM/kill would skip it). main.cpp's
# SIGINT handler calls qGuiApp->exit() -> app.exec() returns -> normal shutdown.
kill -INT \$DOCKPID
for i in \$(seq 1 20); do kill -0 \$DOCKPID 2>/dev/null || break; sleep 1; done
kill -0 \$DOCKPID 2>/dev/null && kill -INT \$DOCKPID && sleep 3
echo "RESULT: dock exited"
EOF
chmod +x "$SESS"

echo "== run the instrumented dock under nested kwin =="
RT="$(mktemp -d)"; chmod 700 "$RT"
ICD="$(ls /usr/share/vulkan/icd.d/lvp_icd.*.json 2>/dev/null | head -1)"
XDG_RUNTIME_DIR="$RT" KWIN_WAYLAND_NO_PERMISSION_CHECKS=1 KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1 \
  VK_ICD_FILENAMES="$ICD" LP_NUM_THREADS=0 \
  timeout 180 dbus-run-session -- kwin_wayland --virtual --width 1280 --height 800 \
  --no-lockscreen --exit-with-session "$SESS"
rm -rf "$RT"

shopt -s nullglob
live=("$LIVEDIR"/*.profraw)
[ "${#live[@]}" -gt 0 ] || { echo "FAIL: no profraw written (dock did not exit cleanly)"; exit 1; }
echo "== captured ${#live[@]} live profraw =="

# Combined number = headless test profraw + live dock profraw, exported over the
# test binaries AND the dock binary (the dock carries every app object).
echo "== merge headless + live =="
hl=("$COV"/coverage/*.profraw)
llvm-profdata merge -sparse "${hl[@]}" "${live[@]}" -o "$WORK/combined.profdata"

bins=()
while IFS= read -r _n; do [ -n "$_n" ] && [ -x "$COV/bin/$_n" ] && bins+=("$COV/bin/$_n"); done < "$COV/cov_targets.txt"
objs=("-object=$DOCK"); for b in "${bins[@]:1}"; do objs+=("-object=$b"); done
main="${bins[0]}"

echo "== export combined =="
llvm-cov export -instr-profile="$WORK/combined.profdata" -format=text \
    "$main" "${objs[@]}" \
    -ignore-filename-regex='(^/usr/|/Qt6/|/KF6|/tests/.*test\.cpp$|_autogen/|moc_|/build-coverage/)' \
    > "$OUT/cxx-live-export.json"
python3 "$REPO/tests/coverage/cxx_report.py" \
    --export "$OUT/cxx-live-export.json" --json-out "$OUT/cxx-live-cov.json"

hp=$(python3 -c "import json;print(f\"{json.load(open('$OUT/cxx-cov.json'))['overall_coverage']*100:.2f}\")" 2>/dev/null || echo "n/a")
lp=$(python3 -c "import json;print(f\"{json.load(open('$OUT/cxx-live-cov.json'))['overall_coverage']*100:.2f}\")")
echo "== C++ whole-app: headless ${hp}%  ->  headless+live ${lp}% =="
