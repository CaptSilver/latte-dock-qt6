from __future__ import annotations

"""Rewrite staged install paths in a coverage catalog to repo-relative paths.

The QML coverage harness instruments the staged install tree so that
org.kde.latte.* module imports resolve at test time. The resulting catalog
keys carry staged paths like:

    usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/Foo.qml

This tool rewrites those to the stable, readable repo-relative form:

    plasmoid/package/contents/ui/Foo.qml

Entries that don't match any staged prefix pass through unchanged.
"""

import argparse
import json
import sys
from pathlib import Path

# Ordered longest-first so a more-specific prefix can't be shadowed by a
# shorter one (all current prefixes are distinct, but the ordering is cheap
# insurance).
PREFIX_MAP: list[tuple[str, str]] = [
    (
        "usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents",
        "plasmoid/package/contents",
    ),
    (
        "usr/share/plasma/plasmoids/org.kde.latte.containment/contents",
        "containment/package/contents",
    ),
    (
        "usr/share/plasma/shells/org.kde.latte.shell/contents",
        "shell/package/contents",
    ),
    (
        "usr/lib64/qt6/qml/org/kde/latte/components",
        "declarativeimports/components",
    ),
    (
        "usr/lib64/qt6/qml/org/kde/latte/core",
        "declarativeimports/core",
    ),
    (
        "usr/lib64/qt6/qml/org/kde/latte/abilities",
        "declarativeimports/abilities",
    ),
]

# Sort by descending length so the longest match wins if prefixes ever overlap.
PREFIX_MAP.sort(key=lambda pair: len(pair[0]), reverse=True)


def remap(s: str) -> str:
    for staged, repo in PREFIX_MAP:
        if s.startswith(staged):
            return repo + s[len(staged):]
    return s


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Rewrite staged install paths in a QML coverage catalog to repo-relative paths."
    )
    parser.add_argument(
        "--catalog", required=True, action="append",
        help="Input catalog JSON file. May be repeated to merge multiple catalogs.",
    )
    parser.add_argument("--out", required=True, help="Output path for the remapped catalog.")
    args = parser.parse_args(argv)

    seen: dict[str, bool] = {}
    merged_units: list[dict] = []
    for path in args.catalog:
        catalog = json.loads(Path(path).read_text(encoding="utf-8"))
        for unit in catalog.get("units", []):
            unit["file"] = remap(unit["file"])
            unit["key"] = remap(unit["key"])
            if unit["key"] not in seen:
                seen[unit["key"]] = True
                merged_units.append(unit)

    Path(args.out).write_text(json.dumps({"units": merged_units}, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
