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
