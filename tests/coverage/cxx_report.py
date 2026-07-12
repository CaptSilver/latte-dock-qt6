#!/usr/bin/env python3
"""Convert `llvm-cov export` JSON into the shared coverage JSON shape
(`overall_coverage`, `by_file[path].coverage`) the ratchet consumes."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _canon(p: str) -> str:
    # /home/<user> and /var/home/<user> are the same inode on an ostree host, and a
    # recreated distrobox can flip which spelling it mounts. Canonicalize so the keys
    # don't drift between runs on different box incarnations.
    if p.startswith("/var/home/"):
        return "/home/" + p[len("/var/home/"):]
    return p


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--export", required=True, help="llvm-cov export -format=text output")
    ap.add_argument("--json-out", required=True)
    ap.add_argument("--repo-root", default=None,
                    help="strip this prefix so by_file keys are repo-relative and path-stable")
    args = ap.parse_args()

    root = _canon(args.repo_root.rstrip("/")) + "/" if args.repo_root else None

    def key(filename: str) -> str:
        canon = _canon(filename)
        if root and canon.startswith(root):
            return canon[len(root):]
        return filename

    data = json.loads(Path(args.export).read_text(encoding="utf-8"))
    d = data["data"][0]
    totals = d["totals"]["lines"]
    overall = (totals["covered"] / totals["count"]) if totals["count"] else 1.0

    by_file = {}
    for f in d["files"]:
        lines = f["summary"]["lines"]
        cov = (lines["covered"] / lines["count"]) if lines["count"] else 1.0
        by_file[key(f["filename"])] = {
            "lines_hit": lines["covered"],
            "lines_total": lines["count"],
            "coverage": cov,
        }

    out = {
        "overall_coverage": overall,
        "lines_hit": totals["covered"],
        "lines_total": totals["count"],
        "by_file": by_file,
    }
    Path(args.json_out).write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(f"C++ overall: {overall * 100:.2f}% ({totals['covered']}/{totals['count']} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
