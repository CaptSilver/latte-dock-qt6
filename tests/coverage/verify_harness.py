#!/usr/bin/env python3
"""Prove the coverage harness counts correctly before any number is trusted.

The self-test fixtures each have one exercised and one unexercised function, so
their file coverage must be strictly between 0 and 1. A reading of exactly 0 or 1
means the harness can't tell executed from unexecuted code -> FAIL.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _file_cov(report: dict, needle: str):
    for path, entry in report.get("by_file", {}).items():
        if needle in path:
            return entry["coverage"]
    return None


def _check(json_path: str, needle: str, label: str) -> bool:
    report = json.loads(Path(json_path).read_text(encoding="utf-8"))
    cov = _file_cov(report, needle)
    if cov is None or not (0.0 < cov < 1.0):
        print(f"HARNESS FAIL ({label}): {needle} coverage={cov}, "
              f"expected strictly between 0 and 1", file=sys.stderr)
        return False
    print(f"harness OK ({label}): {needle} partial coverage {cov * 100:.1f}% "
          f"(executed and unexecuted both observed)")
    return True


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cxx-json")
    ap.add_argument("--qml-json")
    args = ap.parse_args()

    ok = True
    if args.cxx_json:
        ok &= _check(args.cxx_json, "tests/coverage/selftest/covself.cpp", "C++")
    if args.qml_json:
        ok &= _check(args.qml_json, "tests/qml/_covself/CovSelf.qml", "QML")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
