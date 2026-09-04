#!/usr/bin/env bash
# Drops the Stage singleton into a staged install so the package tests can resolve
# their targets through it. Both the coverage harness and the plain qmlpkg runner
# call this, so the two cannot drift.
#
# Usage: drop_stage_module.sh <stage-dir>
set -u

STAGE="${1:?usage: drop_stage_module.sh <stage-dir>}"
HERE="$(cd "$(dirname "$0")" && pwd)"

# Take the QML module dir from the stage itself rather than assuming lib64 or asking
# qtpaths: ECM decides it at configure time and only the stage knows the answer.
QMLREL="$(cd "$STAGE" && find usr -type d -path '*/org/kde/latte' -print -quit)"
QMLREL="${QMLREL%/org/kde/latte}"
if [ -z "$QMLREL" ]; then
    echo "drop_stage_module: no org/kde/latte QML module under $STAGE" >&2
    exit 2
fi

mkdir -p "$STAGE/Stage"
cp "$HERE/stage-module/qmldir" "$STAGE/Stage/qmldir"
sed "s|@QMLREL@|$QMLREL|g" "$HERE/stage-module/Stage.qml.in" > "$STAGE/Stage/Stage.qml"
echo "$QMLREL"
