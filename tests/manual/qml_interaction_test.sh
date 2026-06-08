#!/usr/bin/env bash
# Headless interaction tests for self-contained Latte QML controls. Uses Qt Quick
# Test (qmltestrunner) to instantiate a control offscreen and synthesize real
# mouse/key events, then assert behavior — the "simulate clicking without a live
# session" layer. Pairs with qml_load_compile.sh, which only compiles.
#
# Drop new tst_*.qml cases into tests/qml/ and they run automatically.
#
# Usage (inside the fedora distrobox):
#   distrobox enter fedora -- bash -lc '~/build/latte-dock/tests/manual/qml_interaction_test.sh'
set -u

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
QMLTESTRUNNER="${QMLTESTRUNNER:-/usr/lib64/qt6/bin/qmltestrunner}"

export QT_QPA_PLATFORM=offscreen
exec "$QMLTESTRUNNER" -input "$REPO/tests/qml" -import /usr/lib64/qt6/qml
