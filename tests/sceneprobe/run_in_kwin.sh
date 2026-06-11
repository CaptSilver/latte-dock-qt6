#!/usr/bin/env bash
# Run a command under a throwaway nested kwin_wayland session so it gets a Vulkan-capable
# wayland QPA, pinned to Mesa lavapipe + LP_NUM_THREADS=0 for determinism. Streams the
# command's combined output and propagates its exit code (kwin's own exit code does not
# reflect the session command's).
set -u
ICD="$(ls /usr/share/vulkan/icd.d/lvp_icd.*.json 2>/dev/null | head -1)"
[ -n "$ICD" ] || { echo "lavapipe ICD not found" >&2; exit 2; }

RT="$(mktemp -d /tmp/sceneprobe-xdg.XXXXXX)"; chmod 700 "$RT"
ECF="$(mktemp)"; OUTF="$(mktemp)"; SESS="$(mktemp)"
echo 124 > "$ECF"

{
  printf '#!/bin/bash\n'
  printf 'export QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=vulkan VK_ICD_FILENAMES=%q LP_NUM_THREADS=0\n' "$ICD"
  printf '%q ' "$@"
  printf '>%q 2>&1; echo $? >%q\n' "$OUTF" "$ECF"
} > "$SESS"
chmod +x "$SESS"

XDG_RUNTIME_DIR="$RT" KWIN_WAYLAND_NO_PERMISSION_CHECKS=1 \
  timeout 90 dbus-run-session -- kwin_wayland --virtual --width 256 --height 256 \
  --no-lockscreen --exit-with-session "$SESS" >/dev/null 2>&1

cat "$OUTF"
ec="$(cat "$ECF" 2>/dev/null || echo 124)"
rm -rf "$RT" "$ECF" "$OUTF" "$SESS"
exit "$ec"
