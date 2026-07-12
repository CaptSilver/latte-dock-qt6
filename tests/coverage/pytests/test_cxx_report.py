import json
import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "cxx_report.py"


def test_parses_llvm_cov_export(tmp_path):
    export = {
        "data": [{
            "files": [
                {"filename": "/repo/app/foo.cpp",
                 "summary": {"lines": {"count": 10, "covered": 8, "percent": 80.0}}},
                {"filename": "/repo/app/bar.cpp",
                 "summary": {"lines": {"count": 10, "covered": 2, "percent": 20.0}}},
            ],
            "totals": {"lines": {"count": 20, "covered": 10, "percent": 50.0}},
        }],
        "type": "llvm.coverage.json.export",
    }
    exp = tmp_path / "export.json"
    exp.write_text(json.dumps(export), encoding="utf-8")
    out = tmp_path / "cov.json"
    r = subprocess.run(
        [sys.executable, str(TOOL), "--export", str(exp), "--json-out", str(out)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0, r.stderr
    data = json.loads(out.read_text())
    assert abs(data["overall_coverage"] - 0.5) < 1e-9
    assert abs(data["by_file"]["/repo/app/foo.cpp"]["coverage"] - 0.8) < 1e-9


def _run(tool, exp, out, extra):
    return subprocess.run(
        [sys.executable, str(tool), "--export", str(exp), "--json-out", str(out), *extra],
        capture_output=True, text=True,
    )


def test_repo_root_makes_keys_relative(tmp_path):
    export = {
        "data": [{
            "files": [
                {"filename": "/home/u/repo/app/foo.cpp",
                 "summary": {"lines": {"count": 10, "covered": 8, "percent": 80.0}}},
                {"filename": "/elsewhere/qt/qglobal.h",
                 "summary": {"lines": {"count": 4, "covered": 1, "percent": 25.0}}},
            ],
            "totals": {"lines": {"count": 14, "covered": 9, "percent": 64.3}},
        }],
        "type": "llvm.coverage.json.export",
    }
    exp = tmp_path / "export.json"
    exp.write_text(json.dumps(export), encoding="utf-8")
    out = tmp_path / "cov.json"
    r = _run(TOOL, exp, out, ["--repo-root", "/home/u/repo"])
    assert r.returncode == 0, r.stderr
    by_file = json.loads(out.read_text())["by_file"]
    # under the repo root -> relative key
    assert "app/foo.cpp" in by_file
    # outside the repo root -> left absolute
    assert "/elsewhere/qt/qglobal.h" in by_file


def test_repo_root_canonicalizes_ostree_home_prefix(tmp_path):
    # The distrobox may mount the source under /home while the driver resolves the
    # repo root to /var/home (or vice versa) — same inode, different spelling. The
    # key must still come out repo-relative.
    export = {
        "data": [{
            "files": [
                {"filename": "/home/u/repo/app/foo.cpp",
                 "summary": {"lines": {"count": 10, "covered": 5, "percent": 50.0}}},
            ],
            "totals": {"lines": {"count": 10, "covered": 5, "percent": 50.0}},
        }],
        "type": "llvm.coverage.json.export",
    }
    exp = tmp_path / "export.json"
    exp.write_text(json.dumps(export), encoding="utf-8")
    out = tmp_path / "cov.json"
    r = _run(TOOL, exp, out, ["--repo-root", "/var/home/u/repo"])
    assert r.returncode == 0, r.stderr
    by_file = json.loads(out.read_text())["by_file"]
    assert "app/foo.cpp" in by_file
