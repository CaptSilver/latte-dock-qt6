#!/usr/bin/env bash
# Live widget add/remove e2e: launches the real latte-dock in a nested kwin with a seeded
# HOME and current-source-staged QML, adds then removes a widget through the real remove
# action, and asserts the widget is gone from (1) the layout config, (2) the DBus applet
# list, and (3) the rendered pixels. Run inside the fedora distrobox.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$REPO/tests/e2e"
BUILD="$REPO/build"
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

if [ -d /tmp/latte-e2e-home/.config/latte ]; then
    cp /tmp/latte-e2e-home/.config/latte/*.layout.latte "$HOMEDIR/.config/latte/" 2>/dev/null || true
    cp /tmp/latte-e2e-home/.config/lattedockrc "$HOMEDIR/.config/" 2>/dev/null || true
fi

SESS="$WORK/session.sh"
cat > "$SESS" <<EOF
#!/bin/bash
set -u
export HOME="$HOMEDIR"
export USER=lattee2e USERNAME=lattee2e
rm -f /tmp/latte-dock.lattee2e.lock
export QT_QPA_PLATFORM=wayland
export XDG_DATA_DIRS="$STAGE/usr/share:\${XDG_DATA_DIRS:-/usr/share}"
export QML_IMPORT_PATH="$STAGE/usr/lib64/qt6/qml"
export QML2_IMPORT_PATH="$STAGE/usr/lib64/qt6/qml"
LOG="$WORK/dock.log"; : > "\$LOG"
"$DOCK" --debug --layout Default --log-file "\$LOG" >"$WORK/dock.out" 2>&1 &
DOCKPID=\$!

dctl(){ busctl --user call org.kde.lattedock /Latte org.kde.LatteDock "\$@"; }
up=0; for i in \$(seq 1 25); do
  busctl --user list 2>/dev/null | grep -q org.kde.lattedock && { up=1; break; }
  kill -0 \$DOCKPID 2>/dev/null || break
  sleep 1
done
[ "\$up" = 1 ] || { echo "RESULT: dock-never-came-up"; tail -20 "\$LOG"; exit 1; }
sleep 6

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
  grep -q "\[Applets\]\[\$NEW\]" "\$LAYOUT" || pass_cfg=1
  [ "\$pass_dbus" = 1 ] && [ "\$pass_cfg" = 1 ] && break
  sleep 1
done
python3 "$HERE/shot.py" "$SHOTS/after.png" workspace

"$IMGDIFF" "$SHOTS/before.png" "$SHOTS/after.png" --delta 8 --budget 0.0 >/dev/null 2>&1
pix_rc=\$?
pass_pix=0; [ "\$pix_rc" = 1 ] && pass_pix=1

echo "REMOVE assertions: dbus=\$pass_dbus config=\$pass_cfg pixels=\$pass_pix"
if [ "\$pass_dbus" = 1 ] && [ "\$pass_cfg" = 1 ] && [ "\$pass_pix" = 1 ]; then
  echo "RESULT: PASS"; rc=0
else
  echo "RESULT: FAIL (removal incomplete)"; echo "---- dock log tail ----"; tail -40 "\$LOG"; rc=1
fi
kill \$DOCKPID 2>/dev/null
exit \$rc
EOF
chmod +x "$SESS"

RT="$(mktemp -d)"; chmod 700 "$RT"
ICD="$(ls /usr/share/vulkan/icd.d/lvp_icd.*.json 2>/dev/null | head -1)"
XDG_RUNTIME_DIR="$RT" KWIN_WAYLAND_NO_PERMISSION_CHECKS=1 KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1 \
  VK_ICD_FILENAMES="$ICD" LP_NUM_THREADS=0 \
  timeout 120 dbus-run-session -- kwin_wayland --virtual --width 1280 --height 800 \
  --no-lockscreen --exit-with-session "$SESS"
rc=$?
rm -rf "$RT"
echo "harness exit: $rc"
exit "$rc"
