import json
import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parents[3] / "tools" / "qmlcov" / "report.py"


def test_loc_weighted_overall_and_by_file(tmp_path):
    catalog = {
        "units": [
            {"file": "a/Foo.qml", "name": "go", "start_line": 1, "end_line": 4,
             "loc": 4, "key": "a/Foo.qml::go@1"},
            {"file": "a/Foo.qml", "name": "stop", "start_line": 6, "end_line": 6,
             "loc": 1, "key": "a/Foo.qml::stop@6"},
        ]
    }
    cat = tmp_path / "catalog.json"
    cat.write_text(json.dumps(catalog), encoding="utf-8")
    # Only the 4-LOC unit was hit -> 4/5 = 0.8 overall.
    runlog = tmp_path / "run.txt"
    runlog.write_text('warn: __COV_TICK__:a/Foo.qml::go@1\n', encoding="utf-8")
    out = tmp_path / "cov.json"

    r = subprocess.run(
        [sys.executable, str(TOOL), "--catalog", str(cat), "--runlog", str(runlog),
         "--threshold", "0", "--json-out", str(out)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0, r.stderr
    data = json.loads(out.read_text())
    assert abs(data["overall_coverage"] - 0.8) < 1e-9
    assert abs(data["by_file"]["a/Foo.qml"]["coverage"] - 0.8) < 1e-9
