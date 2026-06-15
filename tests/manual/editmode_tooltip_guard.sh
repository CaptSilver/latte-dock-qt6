#!/usr/bin/env bash
# Static guard for the edit-mode handle flicker.
#
# ConfigOverlay's applet tooltip is a PlasmaCore.Dialog (its own Wayland surface) whose visibility is
# gated by a hideTimer + the wrapping MouseArea's containsMouse. If a handle button carries an attached
# QQC2.ToolTip, hovering it pops a *second* surface at the cursor; on Wayland the compositor then sends
# a leave to the button AND the wrapping MouseArea, the ToolTip hides, the cursor re-enters, and it
# loops ~20Hz — the flicker that made lock/colorize/delete uncliccable. There is no headless repro
# (offscreen QPA doesn't surface popups), so this guards the fix statically: the handle buttons must
# carry no attached QQC2.ToolTip. If hints are wanted, drive the in-Dialog label instead of a popup.
set -u

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
f="$REPO/containment/package/contents/ui/editmode/ConfigOverlay.qml"

# Match real attached-tooltip usage only; the leading [^/]* rejects the explanatory comment line.
if grep -nE '^[^/]*QQC2\.ToolTip\.(visible|text)' "$f"; then
    echo "FAIL: ConfigOverlay edit-handle buttons must not use attached QQC2.ToolTip"
    echo "      (Wayland popup-surface steals hover from the tooltip Dialog -> flicker; see the"
    echo "       comment above the handle Row in ConfigOverlay.qml)."
    exit 1
fi

echo "PASS: no attached QQC2.ToolTip on ConfigOverlay edit-handle buttons."
exit 0
