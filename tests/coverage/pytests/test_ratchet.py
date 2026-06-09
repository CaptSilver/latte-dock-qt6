import json
import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "ratchet.py"


def _cur(tmp_path, pct):
    p = tmp_path / "cur.json"
    p.write_text(json.dumps({"overall_coverage": pct, "by_file": {}}), encoding="utf-8")
    return p


def _run(cur, base, extra=()):
    return subprocess.run(
        [sys.executable, str(TOOL), "--current-json", str(cur), "--baseline", str(base),
         "--tolerance", "0.5", "--label", "T", *extra],
        capture_output=True, text=True,
    )


def test_refresh_writes_baseline(tmp_path):
    base = tmp_path / "base.json"
    r = _run(_cur(tmp_path, 0.42), base, ("--refresh",))
    assert r.returncode == 0, r.stderr
    assert abs(json.loads(base.read_text())["overall_coverage"] - 0.42) < 1e-9


def test_regression_beyond_tolerance_fails(tmp_path):
    base = tmp_path / "base.json"
    base.write_text(json.dumps({"overall_coverage": 0.80, "by_file": {}}), encoding="utf-8")
    r = _run(_cur(tmp_path, 0.79), base)        # 1.0pp drop > 0.5pp tolerance
    assert r.returncode == 1, r.stdout + r.stderr


def test_within_tolerance_passes(tmp_path):
    base = tmp_path / "base.json"
    base.write_text(json.dumps({"overall_coverage": 0.80, "by_file": {}}), encoding="utf-8")
    r = _run(_cur(tmp_path, 0.797), base)       # 0.3pp drop <= 0.5pp tolerance
    assert r.returncode == 0, r.stdout + r.stderr


def test_improvement_passes(tmp_path):
    base = tmp_path / "base.json"
    base.write_text(json.dumps({"overall_coverage": 0.80, "by_file": {}}), encoding="utf-8")
    r = _run(_cur(tmp_path, 0.91), base)
    assert r.returncode == 0, r.stdout + r.stderr
