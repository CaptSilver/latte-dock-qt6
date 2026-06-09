import json
import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "verify_harness.py"


def _report(tmp_path, name, needle, cov):
    p = tmp_path / name
    p.write_text(json.dumps({"overall_coverage": cov,
                             "by_file": {needle: {"coverage": cov}}}), encoding="utf-8")
    return p


def test_partial_fixture_passes(tmp_path):
    cxx = _report(tmp_path, "cxx.json", "/x/tests/coverage/selftest/covself.cpp", 0.5)
    qml = _report(tmp_path, "qml.json", "tests/qml/_covself/CovSelf.qml", 0.5)
    r = subprocess.run([sys.executable, str(TOOL), "--cxx-json", str(cxx),
                        "--qml-json", str(qml)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


def test_all_or_nothing_fixture_fails(tmp_path):
    # Coverage of exactly 1.0 means the harness failed to see the uncovered fn.
    cxx = _report(tmp_path, "cxx.json", "/x/tests/coverage/selftest/covself.cpp", 1.0)
    r = subprocess.run([sys.executable, str(TOOL), "--cxx-json", str(cxx)],
                       capture_output=True, text=True)
    assert r.returncode == 1, r.stdout + r.stderr
