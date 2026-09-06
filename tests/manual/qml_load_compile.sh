#!/usr/bin/env bash
# Headless compile-check for every QML file in Latte's shell/containment/plasmoid
# packages, its indicators, and the org.kde.latte.* QML modules. Unlike
# qml_load_gate.sh (which runs the dock and only sees QML that loads during
# passive startup), this compiles each file in the real QML engine
# via Qt.createComponent — so it catches removed-type / removed-property errors
# in lazy, interaction-only components (the widget explorer, task context menu,
# config pages) that a click would otherwise be needed to surface.
#
# It compiles, it does not instantiate: type resolution and property-assignment
# existence are checked; runtime binding evaluation is not. That's the right
# scope for catching the Plasma 5->6 "X is not a type" / "non-existent property"
# class without a live Wayland session.
#
# One class of file is skipped (and reported) because a standalone engine can't
# judge it; it is instead covered by qml_load_gate.sh, which runs the real dock:
#   * files importing org.kde.latte.private.app — that module is registered in
#     the latte-dock binary (lattecorona.cpp), so it only exists inside the
#     running app, never in qmltestrunner. These all load at startup anyway.
#
# Usage:
#   ctest -R qmlloadcompile
# or by hand, against an existing staged install:
#   STAGE=<staged install> tests/manual/qml_load_compile.sh
set -u

# Latte's own QML modules (org.kde.latte.*) resolve out of a staged install, through the
# import path the dock actually uses. The shellpackage ctest fixture builds that stage and
# removes it again, so each run installs into an empty directory -- which is the property
# this script used to buy for itself by installing to a scratch dir and swapping it in.
# Without it, a QML file deleted from the tree survives in the stage and goes on being
# compiled here long after it is gone.
STAGE="${STAGE:?set STAGE to a staged install; ctest passes it from the shellpackage fixture}"
QMLTESTRUNNER="${QMLTESTRUNNER:-/usr/lib64/qt6/bin/qmltestrunner}"

if [ ! -d "$STAGE/usr/share/plasma" ]; then
    echo "no staged install at $STAGE (expected the shellpackage fixture to provide it)"; exit 2
fi

PKG="$STAGE/usr/share/plasma"
# Indicators live outside the plasma package tree (share/latte/indicators) and
# feed the running/active dot under each task icon. They escaped this gate once:
# the dot vanished because the C++ side failed to load the package, and a QML
# error here would do the same silently, so compile-check them too.
IND="$STAGE/usr/share/latte/indicators"
# The org.kde.latte.* QML modules (components, abilities) install with
# install(DIRECTORY), so no build file names their individual files and nothing
# else compiles them: a broken one only errors inside whatever imports it, and
# for a module this is also third-party import surface, so it can break an
# indicator nobody here ships.
MOD="$STAGE/usr/lib64/qt6/qml/org/kde/latte"
mapfile -t ALL < <(find \
    "$PKG/shells/org.kde.latte.shell" \
    "$PKG/plasmoids/org.kde.latte.containment" \
    "$PKG/plasmoids/org.kde.latte.plasmoid" \
    "$IND" \
    "$MOD" \
    -name '*.qml' 2>/dev/null | sort)

if [ "${#ALL[@]}" -eq 0 ]; then echo "no staged QML found under $PKG"; exit 2; fi

# Partition into checkable vs skipped (see header for why).
FILES=(); skipped_app=0
for f in "${ALL[@]}"; do
    if grep -q 'org.kde.latte.private.app' "$f"; then skipped_app=$((skipped_app+1)); continue; fi
    FILES+=("$f")
done
echo "skipped $skipped_app app-module-dependent files (covered by qml_load_gate.sh)"

if [ "${#FILES[@]}" -eq 0 ]; then echo "nothing left to compile"; exit 2; fi

# The generated TestCase holds absolute paths from this run only, so it goes in a
# per-process dir: a fixed /tmp name is one file two concurrent runs would fight over.
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
GEN="$WORK/qml_compile_check.qml"
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
