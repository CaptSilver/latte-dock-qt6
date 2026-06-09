#!/usr/bin/env python3
"""Reviewer-aid lint for package-QML coverage tests. Flags the gaming patterns the
honest-coverage standard bans: `safe(`-style entry-tick banking, throw-swallowing
try/catch, and `test_*` functions with no assertion. Exit 1 if anything is flagged."""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

_ASSERT = re.compile(r"\b(compare|verify|tryCompare|tryVerify|fuzzyCompare|fail)\s*\(")


def _matching_brace(text: str, open_pos: int) -> int:
    depth = 0
    for i in range(open_pos, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    return -1


def flag_file(text: str) -> list[str]:
    flags = []
    if "safe(" in text:
        flags.append("uses safe(...) — entry-tick banking is banned")
    for m in re.finditer(r"catch\s*\([^)]*\)\s*\{", text):
        end = _matching_brace(text, m.end() - 1)
        body = text[m.end():end] if end > 0 else ""
        if "throw" not in body:
            flags.append("catch block swallows errors (no rethrow) — may bank entry ticks")
            break
    for m in re.finditer(r"function\s+(test_[A-Za-z0-9_]*)\s*\([^)]*\)\s*\{", text):
        end = _matching_brace(text, m.end() - 1)
        body = text[m.end():end] if end > 0 else ""
        if not _ASSERT.search(body):
            flags.append(f"{m.group(1)} has no assertion (compare/verify/...)")
    return flags


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True)
    args = ap.parse_args()

    any_flag = False
    for qml in sorted(Path(args.dir).glob("tst_*.qml")):
        flags = flag_file(qml.read_text(encoding="utf-8"))
        if flags:
            any_flag = True
            print(f"{qml.name}:")
            for f in flags:
                print(f"    - {f}")
    if not any_flag:
        print("lint: no gaming patterns flagged")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
