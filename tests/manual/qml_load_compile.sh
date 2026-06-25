#!/usr/bin/env bash
# Headless compile-check for every QML file in Latte's shell/containment/plasmoid
# and indicator packages. Unlike qml_load_gate.sh (which runs the dock and only sees QML that
# loads during passive startup), this compiles each file in the real QML engine
# via Qt.createComponent — so it catches removed-type / removed-property errors
# in lazy, interaction-only components (the widget explorer, task context menu,
# config pages) that a click would otherwise be needed to surface.
#
# It compiles, it does not instantiate: type resolution and property-assignment
# existence are checked; runtime binding evaluation is not. That's the right
# scope for catching the Plasma 5->6 "X is not a type" / "non-existent property"
# class without a live Wayland session.
#
# Two files-classes are skipped (and reported) because a standalone engine can't
# judge them; they are instead covered by qml_load_gate.sh, which runs the real
# dock:
#   * files importing org.kde.latte.private.app — that module is registered in
#     the latte-dock binary (lattecorona.cpp), so it only exists inside the
#     running app, never in qmltestrunner. These all load at startup anyway.
#   * superseded *.5.2[0-5].qml version-ladder variants — on Plasma 6 only the
#     newest variant is ever loaded (see ToolTipInstance.qml's selector); the
#     older ones target removed Plasma 5 APIs and are dead here.
#
# Usage (inside the fedora distrobox):
#   distrobox enter fedora -- bash -lc '~/build/latte-dock/tests/manual/qml_load_compile.sh'
set -u

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${BUILD:-$REPO/build}"
STAGE="${STAGE:-/tmp/lattestage}"
QMLTESTRUNNER="${QMLTESTRUNNER:-/usr/lib64/qt6/bin/qmltestrunner}"

# Deploy the current tree so Latte's own QML modules (org.kde.latte.*) and any
# edits resolve through the import path the dock actually uses.
echo "staging $BUILD -> $STAGE ..."
if ! ( cd "$BUILD" && DESTDIR="$STAGE" cmake --install . ) >/tmp/qml-compile-stage.log 2>&1; then
    echo "STAGE FAILED:"; tail -15 /tmp/qml-compile-stage.log; exit 2
fi

PKG="$STAGE/usr/share/plasma"
# Indicators live outside the plasma package tree (share/latte/indicators) and
# feed the running/active dot under each task icon. They escaped this gate once:
# the dot vanished because the C++ side failed to load the package, and a QML
# error here would do the same silently, so compile-check them too.
IND="$STAGE/usr/share/latte/indicators"
mapfile -t ALL < <(find \
    "$PKG/shells/org.kde.latte.shell" \
    "$PKG/plasmoids/org.kde.latte.containment" \
    "$PKG/plasmoids/org.kde.latte.plasmoid" \
    "$IND" \
    -name '*.qml' 2>/dev/null | sort)

if [ "${#ALL[@]}" -eq 0 ]; then echo "no staged QML found under $PKG"; exit 2; fi

# Partition into checkable vs skipped (see header for why).
FILES=(); skipped_app=0; skipped_ver=0
for f in "${ALL[@]}"; do
    if [[ "$f" =~ \.5\.2[0-5]\.qml$ ]]; then skipped_ver=$((skipped_ver+1)); continue; fi
    if grep -q 'org.kde.latte.private.app' "$f"; then skipped_app=$((skipped_app+1)); continue; fi
    FILES+=("$f")
done
echo "skipped $skipped_app app-module-dependent + $skipped_ver dead-version-ladder files (covered by qml_load_gate.sh)"

if [ "${#FILES[@]}" -eq 0 ]; then echo "nothing left to compile"; exit 2; fi

GEN=/tmp/qml_compile_check.qml
{
    echo 'import QtQuick'
    echo 'import QtTest'
    echo 'TestCase {'
    echo '    name: "QmlCompileGate"'
    echo '    property var files: ['
    for f in "${FILES[@]}"; do echo "        \"file://$f\","; done
    echo '    ]'
    echo '    function test_compileAll() {'
    echo '        var failed = [];'
    echo '        for (var i = 0; i < files.length; i++) {'
    echo '            var c = Qt.createComponent(files[i]);'
    echo '            if (c.status === Component.Error) {'
    echo '                console.warn("FAIL " + files[i] + "\n      " + c.errorString().trim());'
    echo '                failed.push(files[i]);'
    echo '            }'
    echo '            if (c) c.destroy();'
    echo '        }'
    echo '        console.warn("=== " + failed.length + " of " + files.length + " package QML files failed to compile ===");'
    echo '        verify(failed.length === 0, failed.length + " QML files failed to compile");'
    echo '    }'
    echo '}'
} > "$GEN"

echo "compiling ${#FILES[@]} QML files (offscreen)..."
# Import order matters: qmltestrunner gives the LAST -import the highest priority,
# and a module URI resolves entirely from the first import path that provides it
# (no merging across paths). The system path holds the RPM-installed org.kde.latte.*
# modules, so it must come BEFORE the staged tree — otherwise the stale installed
# copies shadow the working tree and any type added to a Latte module this session
# (e.g. a new component) is invisible to the gate.
QT_QPA_PLATFORM=offscreen "$QMLTESTRUNNER" \
    -import /usr/lib64/qt6/qml \
    -import "$STAGE/usr/lib64/qt6/qml" \
    -input "$GEN"
