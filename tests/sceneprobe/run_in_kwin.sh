#!/usr/bin/env bash
# Run a command under a throwaway nested kwin_wayland session so it gets a Vulkan-capable
# wayland QPA. Device mode is controlled by SCENEPROBE_DEVICE (default: lavapipe).
#   lavapipe  — Mesa software Vulkan, LP_NUM_THREADS=0 for determinism
#   dgpu      — hardware RADV on the discrete AMD RX 9070 XT (MESA_VK_DEVICE_SELECT=1002:7550)
# Streams the command's combined output and propagates its exit code (kwin's own exit code
# does not reflect the session command's).
set -u

DEV="${SCENEPROBE_DEVICE:-lavapipe}"
case "$DEV" in
  lavapipe)
    ICD="$(ls /usr/share/vulkan/icd.d/lvp_icd.*.json 2>/dev/null | head -1)"
    [ -n "$ICD" ] || { echo "lavapipe ICD not found" >&2; exit 2; }
    DEV_ENV='LP_NUM_THREADS=0'
    ;;
  dgpu)
    ICD="$(ls /usr/share/vulkan/icd.d/radeon_icd.*.json 2>/dev/null | head -1)"
    [ -n "$ICD" ] || { echo "RADV ICD not found" >&2; exit 2; }
    # pin the discrete RX 9070 XT (vendorID:deviceID 1002:7550); the box also exposes
    # the 9950X3D integrated Radeon (1002:13c0) so explicit selection is mandatory
    DEV_ENV='MESA_VK_DEVICE_SELECT=1002:7550'
    ;;
  *) echo "unknown SCENEPROBE_DEVICE: $DEV" >&2; exit 2;;
esac

RT="$(mktemp -d /tmp/sceneprobe-xdg.XXXXXX)"; chmod 700 "$RT"
ECF="$(mktemp)"; OUTF="$(mktemp)"; SESS="$(mktemp)"
echo 124 > "$ECF"

{
  printf '#!/bin/bash\n'
  printf 'export QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=vulkan VK_ICD_FILENAMES=%q %s\n' "$ICD" "$DEV_ENV"
  for v in LATTE_VK_SUPPRESSIONS LATTE_QML_IMPORT_PATH ASAN_OPTIONS SCENEPROBE_DEVICE SCENEPROBE_ARTIFACTS; do
    if [ -n "${!v:-}" ]; then printf 'export %s=%q\n' "$v" "${!v}"; fi
  done
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
