#!/usr/bin/env bash
# Full coverage gate: measure C++ + QML, verify the harness counts correctly,
# then ratchet both against committed baselines. LATTE_COVERAGE_REFRESH=1
# rewrites the baselines (after an intentional, coverage-affecting change).
# Needs clang + llvm-tools and the Qt6/KF6 build deps.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$REPO/build/_coverage"
mkdir -p "$OUT"
REFRESH=(); [ "${LATTE_COVERAGE_REFRESH:-0}" = "1" ] && REFRESH=(--refresh)

bash "$REPO/tests/coverage/cxx_coverage.sh"
bash "$REPO/tests/coverage/qml_coverage.sh"

echo "== verify harness counts correctly =="
python3 "$REPO/tests/coverage/verify_harness.py" \
    --cxx-json "$OUT/cxx-cov.json" --qml-json "$OUT/qml-cov.json"

echo "== ratchet =="
python3 "$REPO/tests/coverage/ratchet.py" --label "C++" \
    --current-json "$OUT/cxx-cov.json" \
    --baseline "$REPO/tests/coverage/.cxx-baseline.json" \
    --tolerance 0.5 "${REFRESH[@]}"
python3 "$REPO/tests/coverage/ratchet.py" --label "QML" \
    --current-json "$OUT/qml-cov.json" \
    --baseline "$REPO/tests/coverage/.qml-baseline.json" \
    --tolerance 0.5 "${REFRESH[@]}"

echo "== coverage gate passed =="
