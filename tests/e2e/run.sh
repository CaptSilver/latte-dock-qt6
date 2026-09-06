#!/usr/bin/env bash
# Live widget add/remove e2e: launches the real latte-dock in a nested kwin with a seeded
# HOME and current-source-staged QML, adds then removes a widget through the real remove
# action, and asserts the widget is gone from (1) the DBus applet list and (2) the rendered
# pixels. The on-disk config group is logged informational only.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
. "$REPO/tests/lib/nested_kwin.sh"
HERE="$REPO/tests/e2e"
BUILD="${BUILD:-$REPO}"
DOCK="$BUILD/bin/latte-dock"
IMGDIFF="$BUILD/bin/latte-imgdiff"
# Default to a plasmoid that ships a renderable contents/ui/main.qml on disk so the pixel
# assertion has something to render. The analog clock isn't packaged on every distro (Fedora
# 44 doesn't ship the standalone org.kde.plasma.analogclock plasmoid); override with WIDGET=.
WIDGET="${WIDGET:-org.kde.plasma.systemmonitor}"
[ -x "$DOCK" ] || { echo "no latte-dock at $DOCK"; exit 2; }
[ -x "$IMGDIFF" ] || { echo "no latte-imgdiff at $IMGDIFF (build the sceneprobe target)"; exit 2; }

WORK="$(mktemp -d /tmp/latte-e2e.XXXXXX)"
STAGE="$WORK/stage"; HOMEDIR="$WORK/home"; SHOTS="$WORK/shots"
mkdir -p "$HOMEDIR/.config/latte" "$SHOTS"
trap 'rm -rf "$WORK"' EXIT

DESTDIR="$STAGE" cmake --install "$BUILD" >"$WORK"/install.log 2>&1 \
    || { echo "stage install failed"; tail -10 "$WORK"/install.log; exit 2; }

# Seed the sandbox with the shipped Default template. This used to copy from a stray
# /tmp/latte-e2e-home if one happened to exist, which is a machine-specific crutch -- and it
# did not matter anyway, because the session leaked out to the developer's real config and
# read its layout from there. With the sandbox honest, the layout has to come from the tree.
TEMPLATE="$REPO/shell/package/contents/templates/Default.layout.latte"
[ -r "$TEMPLATE" ] || { echo "no layout template at $TEMPLATE"; exit 2; }
cp "$TEMPLATE" "$HOMEDIR/.config/latte/Default.layout.latte"
printf '[UniversalSettings]\ncurrentLayout=Default\n' > "$HOMEDIR/.config/lattedockrc"

SESS="$WORK/session.sh"
cat > "$SESS" <<EOF
#!/bin/bash
set -u
. "$REPO/tests/lib/nested_kwin.sh"
seed_sandbox_home "$HOMEDIR"
export USER=lattee2e USERNAME=lattee2e
rm -f /tmp/latte-dock.lattee2e.lock
export QT_QPA_PLATFORM=wayland
export XDG_DATA_DIRS="$STAGE/usr/share:\${XDG_DATA_DIRS:-/usr/share}"
export QML_IMPORT_PATH="$STAGE/usr/lib64/qt6/qml"
export QML2_IMPORT_PATH="$STAGE/usr/lib64/qt6/qml"
LOG="$WORK/dock.log"; : > "\$LOG"
"$DOCK" --debug --layout Default --log-file "\$LOG" >"$WORK/dock.out" 2>&1 &
DOCKPID=\$!

. "$REPO/tests/lib/dockctl.sh"
wait_for_dock \$DOCKPID 25 6 || { echo "RESULT: dock-never-came-up"; tail -20 "\$LOG"; exit 1; }

CID=\$(dctl containmentIds 2>/dev/null | awk '{print \$3}')
[ -n "\$CID" ] || { echo "RESULT: no-containment-id"; exit 1; }
echo "containment=\$CID"
before=\$(dctl appletIds u "\$CID" 2>/dev/null)
dctl addApplet us "\$CID" "$WIDGET"
sleep 3
after=\$(dctl appletIds u "\$CID" 2>/dev/null)
echo "applets before:[\$before] after:[\$after]"
NEW=""
for t in \$(echo "\$after" | tr ' ' '\n' | grep -E '^[0-9]+\$'); do
  echo "\$before" | grep -qw "\$t" || NEW="\$t"
done
[ -n "\$NEW" ] || { echo "RESULT: add-failed (no new applet id)"; exit 1; }
echo "new-applet=\$NEW"

LAYOUT="\$(ls "$HOMEDIR"/.config/latte/*.layout.latte 2>/dev/null | head -1)"
grep -q "\[Applets\]\[\$NEW\]" "\$LAYOUT" && echo "add: in config OK" || echo "add: NOT in config"
python3 "$HERE/shot.py" "$SHOTS/before.png" workspace

dctl triggerAppletAction uus "\$CID" "\$NEW" "remove"
pass_dbus=0 pass_cfg=0
for i in \$(seq 1 15); do
  ids=\$(dctl appletIds u "\$CID" 2>/dev/null)
  echo "\$ids" | grep -qw "\$NEW" || pass_dbus=1
  # Config check runs every tick but does not gate the loop — Latte flushes lazily and not
  # to the seeded legacy layout file in-session, so this group may never vanish from disk.
  grep -q "\[Applets\]\[\$NEW\]" "\$LAYOUT" || pass_cfg=1
  [ "\$pass_dbus" = 1 ] && break
  sleep 1
done
python3 "$HERE/shot.py" "$SHOTS/after.png" workspace

"$IMGDIFF" "$SHOTS/before.png" "$SHOTS/after.png" --delta 8 --budget 0.0 >/dev/null 2>&1
pix_rc=\$?
pass_pix=0; [ "\$pix_rc" = 1 ] && pass_pix=1

echo "REMOVE: dbus=\$pass_dbus pixels=\$pass_pix (config[informational]=\$pass_cfg)"
# Config is informational: Latte persists applet changes lazily and not to the seeded legacy
# layout file in-session, so the on-disk [Applets][<id>] group is not a reliable removal
# witness. The applet object leaving appletIds() (DBus) and the dock reflowing (pixels) are
# the conclusive witnesses — they directly guard the c2e17559a "applet not destroyed" regression.
if [ "\$pass_dbus" = 1 ] && [ "\$pass_pix" = 1 ]; then
  echo "RESULT: PASS"; rc=0
else
  echo "RESULT: FAIL (removal incomplete)"; echo "---- dock log tail ----"; tail -40 "\$LOG"; rc=1
fi
kill \$DOCKPID 2>/dev/null
exit \$rc
EOF
chmod +x "$SESS"

ICD="$(vulkan_icd lvp)" || exit 2
# KWIN_SCREENSHOT_NO_PERMISSION_CHECKS goes on kwin's own environment, not the session's: it
# is read by kwin's ScreenShot2 effect, so forwarding it to shot.py instead would just get
# every capture denied and pin pass_pix at 0.
launch_nested_kwin --width 1280 --height 800 --timeout 120 \
  --env "VK_ICD_FILENAMES=$ICD" --env LP_NUM_THREADS=0 \
  --env KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1 \
  -- "$SESS"
rc=$?
echo "harness exit: $rc"
exit "$rc"
