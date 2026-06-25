#!/usr/bin/env bash
# Runs latte-dock briefly and fails if the debug log contains QML load errors.
# Latte silences logging unless --debug is passed; this captures the log and
# greps it for the QML-load error signatures. EXIT 124 from `timeout` is the
# normal "survived the run" case (the dock keeps running until killed).
#
# Usage: run after installing the package (cmake --install .):
#   tests/manual/qml_load_gate.sh
set -u
LOG="${1:-/tmp/latte6-gate.log}"
rm -f "$LOG"
QT_FORCE_STDERR_LOGGING=1 timeout 15 latte-dock --replace --debug --log-file "$LOG"
echo "---- log ($LOG) ----"
cat -n "$LOG"
echo "---- gate ----"
if grep -nE 'is not a type|Cannot call method|Cannot assign|is not installed|Unable to assign|is not a member|non-existent property|Component is not ready|error when loading applet' "$LOG"; then
    echo "QML LOAD GATE: FAIL"
    exit 1
fi
echo "QML LOAD GATE: PASS"
exit 0
