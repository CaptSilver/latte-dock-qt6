#!/usr/bin/env bash
# Clang source-based C++ coverage for the latte-dock test suite. Configures a
# dedicated clang coverage build, runs ctest with per-process profraw, merges,
# exports per-file line coverage, and writes build/_coverage/cxx-cov.json.
# Run inside the fedora distrobox.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COV_DIR="${COV_DIR:-$REPO/build-coverage}"
OUT="${OUT:-$REPO/build/_coverage}"
mkdir -p "$OUT"

echo "== configure (clang, LATTE_COVERAGE=ON) =="
cmake -B "$COV_DIR" -S "$REPO" -G Ninja \
    -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
    -DLATTE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON

echo "== build =="
cmake --build "$COV_DIR" -j"$(nproc)"

echo "== run instrumented tests =="
# Absolute LLVM_PROFILE_FILE: configuring -S . puts the tests in build-coverage/tests/,
# so a relative path would scatter profraw under each test's working dir and the
# merge glob below would miss them. Anchor every process to one dir.
# Exclude the QML shell-script gates: they produce no C++ coverage and are measured
# by qml_coverage.sh separately.
rm -rf "$COV_DIR/coverage"; mkdir -p "$COV_DIR/coverage"
( cd "$COV_DIR" && QT_QPA_PLATFORM=offscreen \
    LLVM_PROFILE_FILE="$COV_DIR/coverage/%p.profraw" \
    ctest -E 'qmlloadcompile|qmlinteraction' --output-on-failure )

echo "== merge =="
llvm-profdata merge -sparse "$COV_DIR"/coverage/*.profraw \
    -o "$COV_DIR/coverage/merged.profdata"

echo "== export =="
# All test binaries end in 'test' and land in bin/; production binaries/plugins do not.
mapfile -t bins < <(find "$COV_DIR/bin" -maxdepth 1 -type f -executable -name '*test' | sort)
if [ "${#bins[@]}" -eq 0 ]; then echo "no instrumented test binaries found"; exit 2; fi
main="${bins[0]}"; rest=(); for b in "${bins[@]:1}"; do rest+=("-object=$b"); done
# Drop *test.cpp driver files (incl. covselftest.cpp) but KEEP production sources
# and the covself.cpp fixture, so verify_harness.py can see the self-test file.
llvm-cov export -instr-profile="$COV_DIR/coverage/merged.profdata" -format=text \
    "$main" "${rest[@]}" \
    -ignore-filename-regex='(^/usr/|/Qt6/|/KF6|/tests/.*test\.cpp$|_autogen/|moc_|/build-coverage/)' \
    > "$OUT/cxx-export.json"

echo "== report =="
python3 "$REPO/tests/coverage/cxx_report.py" \
    --export "$OUT/cxx-export.json" --json-out "$OUT/cxx-cov.json"
