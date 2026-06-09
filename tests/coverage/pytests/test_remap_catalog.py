import json
import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "remap_catalog.py"


def test_remaps_staged_keys_to_repo_relative(tmp_path):
    cat = {
        "units": [
            {"file": "usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/TasksExtendedManager.qml",
             "name": "addToBuffer", "start_line": 10, "end_line": 14, "loc": 5,
             "key": "usr/share/plasma/plasmoids/org.kde.latte.plasmoid/contents/ui/TasksExtendedManager.qml::addToBuffer@10"},
            {"file": "usr/lib64/qt6/qml/org/kde/latte/components/CheckBox.qml",
             "name": "onClicked", "start_line": 3, "end_line": 5, "loc": 3,
             "key": "usr/lib64/qt6/qml/org/kde/latte/components/CheckBox.qml::onClicked@3"},
            {"file": "tests/qml/_covself/CovSelf.qml",
             "name": "covSelfCovered", "start_line": 6, "end_line": 9, "loc": 4,
             "key": "tests/qml/_covself/CovSelf.qml::covSelfCovered@6"},
        ]
    }
    src = tmp_path / "catalog.json"
    src.write_text(json.dumps(cat), encoding="utf-8")
    out = tmp_path / "remapped.json"
    r = subprocess.run([sys.executable, str(TOOL), "--catalog", str(src), "--out", str(out)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    data = json.loads(out.read_text())
    files = {u["file"] for u in data["units"]}
    keys = {u["key"] for u in data["units"]}
    assert "plasmoid/package/contents/ui/TasksExtendedManager.qml" in files
    assert "plasmoid/package/contents/ui/TasksExtendedManager.qml::addToBuffer@10" in keys
    assert "declarativeimports/components/CheckBox.qml" in files
    # Already-repo-relative entries (e.g. the _covself fixture) pass through unchanged.
    assert "tests/qml/_covself/CovSelf.qml" in files
