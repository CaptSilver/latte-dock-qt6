#!/usr/bin/env bash
# Run a command under a throwaway nested kwin_wayland session so it gets a Vulkan-capable
# wayland QPA. Device mode is controlled by SCENEPROBE_DEVICE (default: lavapipe).
#   lavapipe  — Mesa software Vulkan, LP_NUM_THREADS=0 for determinism
#   dgpu      — hardware RADV on the discrete AMD RX 9070 XT (MESA_VK_DEVICE_SELECT=1002:7550)
# Emits ONLY the probe's own transcript and exits with its exit code. Unlike the e2e and
# capture harnesses, this one's caller redirects to a file and prints it only on failure, so
# streaming buys nothing here and costs a readable verdict: the "N px differ" line ends up
# buried in portal warnings, dbus activation lines and a PipeWire connect failure.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../lib/nested_kwin.sh"

DEV="${SCENEPROBE_DEVICE:-lavapipe}"
case "$DEV" in
  lavapipe)
    ICD="$(vulkan_icd lvp)" || exit 2
    DEV_ENV='LP_NUM_THREADS=0'
    ;;
  dgpu)
    ICD="$(vulkan_icd radeon)" || exit 2
    # pin the discrete RX 9070 XT (vendorID:deviceID 1002:7550); the box also exposes
    # the 9950X3D integrated Radeon (1002:13c0) so explicit selection is mandatory
    DEV_ENV='MESA_VK_DEVICE_SELECT=1002:7550'
    ;;
  *) echo "unknown SCENEPROBE_DEVICE: $DEV" >&2; exit 2;;
esac

SESS="$(mktemp)"; OUTF="$(mktemp)"
trap 'rm -f "$SESS" "$OUTF"' EXIT

# The probe picks its own RHI backend (main.cpp qputenv's QSG_RHI_BACKEND), so this only has
# to hand it a Vulkan driver and the wayland platform.
{
  printf '#!/bin/bash\n'
  printf 'export QT_QPA_PLATFORM=wayland VK_ICD_FILENAMES=%q %s\n' "$ICD" "$DEV_ENV"
  for v in LATTE_VK_SUPPRESSIONS LATTE_QML_IMPORT_PATH ASAN_OPTIONS SCENEPROBE_DEVICE SCENEPROBE_ARTIFACTS SCENEPROBE_BLESS; do
    if [ -n "${!v:-}" ]; then printf 'export %s=%q\n' "$v" "${!v}"; fi
  done
  printf 'exec '
  printf '%q ' "$@"
  printf '> %q 2>&1\n' "$OUTF"

} > "$SESS"
chmod +x "$SESS"

launch_nested_kwin --width 256 --height 256 --timeout 90 -- "$SESS" >/dev/null 2>&1
rc=$?
cat "$OUTF"
exit "$rc"
