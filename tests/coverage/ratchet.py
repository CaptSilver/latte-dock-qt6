#!/usr/bin/env python3
"""Ratchet coverage against a committed baseline.

FAILs (exit 1) only when current coverage drops more than --tolerance percentage
points below the baseline. --refresh (or a missing baseline) rewrites the baseline.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--current-json", required=True)
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--tolerance", type=float, default=0.5, help="percentage points")
    ap.add_argument("--label", default="coverage")
    ap.add_argument("--refresh", action="store_true")
    args = ap.parse_args()

    cur = json.loads(Path(args.current_json).read_text(encoding="utf-8"))
    cur_pct = float(cur["overall_coverage"])
    bpath = Path(args.baseline)

    if args.refresh or not bpath.exists():
        bpath.parent.mkdir(parents=True, exist_ok=True)
        bpath.write_text(
            json.dumps(
                {"overall_coverage": cur_pct, "by_file": cur.get("by_file", {})},
                indent=2,
            ),
            encoding="utf-8",
        )
        print(f"{args.label}: baseline written at {cur_pct * 100:.2f}%")
        return 0

    base_pct = float(json.loads(bpath.read_text(encoding="utf-8"))["overall_coverage"])
    floor = base_pct - args.tolerance / 100.0
    print(
        f"{args.label}: current {cur_pct * 100:.2f}%  "
        f"baseline {base_pct * 100:.2f}%  floor {floor * 100:.2f}%"
    )
    if cur_pct < floor:
        print(
            f"{args.label} FAIL: regressed more than {args.tolerance}pp below baseline. "
            f"If intentional, re-run with LATTE_COVERAGE_REFRESH=1.",
            file=sys.stderr,
        )
        return 1
    print(f"{args.label} PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
