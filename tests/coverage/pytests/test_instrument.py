import json
import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parents[3] / "tools" / "qmlcov" / "instrument.py"


def _write(p: Path, text: str) -> None:
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")


def test_multi_include_repo_relative(tmp_path):
    root = tmp_path / "repo"
    _write(root / "dirA" / "Foo.qml",
           'import QtQuick\nItem {\n  function go() {\n    var x = 1;\n    return x;\n  }\n}\n')
    _write(root / "dirB" / "Bar.qml",
           'import QtQuick\nItem {\n  Component.onCompleted: {\n    console.log("hi");\n  }\n}\n')
    _write(root / "skip" / "Nope.qml", 'import QtQuick\nItem { function z() { return 0; } }\n')

    out = tmp_path / "mirror"
    cat = tmp_path / "catalog.json"
    r = subprocess.run(
        [sys.executable, str(TOOL), "--root", str(root),
         "--include", "dirA", "--include", "dirB",
         "--out", str(out), "--catalog", str(cat)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0, r.stderr

    # Mirror preserves repo-relative layout; only included dirs instrumented.
    assert (out / "dirA" / "Foo.qml").exists()
    assert (out / "dirB" / "Bar.qml").exists()
    assert not (out / "skip" / "Nope.qml").exists()

    foo = (out / "dirA" / "Foo.qml").read_text()
    assert 'Cov.tick("dirA/Foo.qml::go@' in foo      # repo-relative key
    assert "import Cov 1.0" in foo

    bar = (out / "dirB" / "Bar.qml").read_text()
    assert "import Cov 1.0" in bar
    assert 'Cov.tick("dirB/Bar.qml::' in bar

    data = json.loads(cat.read_text())
    files = {u["file"] for u in data["units"]}
    assert "dirA/Foo.qml" in files and "dirB/Bar.qml" in files
    assert "skip/Nope.qml" not in files


def test_overlapping_includes_no_double_count(tmp_path):
    root = tmp_path / "repo"
    _write(root / "a" / "sub" / "Deep.qml",
           'import QtQuick\nItem {\n  function f() {\n    var y = 2;\n    return y;\n  }\n}\n')
    out = tmp_path / "mirror"
    cat = tmp_path / "catalog.json"
    r = subprocess.run(
        [sys.executable, str(TOOL), "--root", str(root),
         "--include", "a", "--include", "a/sub",   # overlapping
         "--out", str(out), "--catalog", str(cat)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0, r.stderr
    data = json.loads(cat.read_text())
    keys = [u["key"] for u in data["units"]]
    assert sum(1 for k in keys if k.startswith("a/sub/Deep.qml::f@")) == 1
    assert (out / "a" / "sub" / "Deep.qml").read_text().count("import Cov 1.0") == 1


def test_arrow_signal_handlers_are_tracked(tmp_path):
    # Qt 6 wants `onFoo: (a, b) => { ... }` over the parameter-injecting block form, and the
    # hover hot paths were ported to it. Without a pattern for the arrow shape those handlers
    # vanish from the catalog instead of reading as uncovered, so a hand-ported file silently
    # improves the percentage while measuring less.
    root = tmp_path / "repo"
    _write(root / "ui" / "Area.qml",
           'import QtQuick\n'
           'Item {\n'
           '  signal moved(real mouseX, real mouseY)\n'
           '  onMoved: (mouseX, mouseY) => {\n'
           '    var x = mouseX;\n'
           '    return x + mouseY;\n'
           '  }\n'
           '  onWheeled: wheel => {\n'
           '    var d = wheel.angleDelta;\n'
           '    return d;\n'
           '  }\n'
           '}\n')

    out = tmp_path / "mirror"
    cat = tmp_path / "catalog.json"
    r = subprocess.run(
        [sys.executable, str(TOOL), "--root", str(root), "--include", "ui",
         "--out", str(out), "--catalog", str(cat)],
        capture_output=True, text=True,
    )
    assert r.returncode == 0, r.stderr

    units = {u["name"]: u for u in json.loads(cat.read_text())["units"]}
    assert "onMoved" in units, "parenthesised arrow handler missing from the catalog"
    assert "onWheeled" in units, "single-parameter arrow handler missing from the catalog"
    assert units["onMoved"]["loc"] == 4

    mirrored = (out / "ui" / "Area.qml").read_text()
    assert 'Cov.tick("ui/Area.qml::onMoved@' in mirrored
    assert 'Cov.tick("ui/Area.qml::onWheeled@' in mirrored
