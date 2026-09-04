import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parents[1] / "lint_pkg_tests.py"

GAMING = '''import QtQuick
import QtTest
TestCase {
    name: "G"
    function safe(fn) { try { fn(); } catch(e) {} }
    function test_banks() {
        safe(function(){ obj.doThing(); });
    }
}
'''

CLEAN = '''import QtQuick
import QtTest
TestCase {
    name: "C"
    function test_real() {
        const obj = createTemporaryObject(c, root, {});
        compare(obj.add(2, 3), 5);
    }
}
'''


def test_flags_gaming_passes_clean(tmp_path):
    (tmp_path / "tst_gaming.qml").write_text(GAMING, encoding="utf-8")
    r = subprocess.run([sys.executable, str(TOOL), "--dir", str(tmp_path)],
                       capture_output=True, text=True)
    assert r.returncode == 1, r.stdout
    assert "tst_gaming.qml" in r.stdout
    assert "safe(" in r.stdout or "swallow" in r.stdout.lower()

    (tmp_path / "tst_gaming.qml").unlink()
    (tmp_path / "tst_clean.qml").write_text(CLEAN, encoding="utf-8")
    r2 = subprocess.run([sys.executable, str(TOOL), "--dir", str(tmp_path)],
                        capture_output=True, text=True)
    assert r2.returncode == 0, r2.stdout

STAGE_LITERAL = """
import QtQuick
import QtTest
TestCase {
    readonly property url targetUrl: Qt.resolvedUrl("../../../build/_qmlcov/stage/usr/share/x.qml")
    function test_thing() { compare(1, 1); }
}
"""

STAGE_SINGLETON = """
import QtQuick
import QtTest
import Stage 1.0
TestCase {
    readonly property url targetUrl: Stage.share("plasma/plasmoids/x/contents/ui/Foo.qml")
    function test_thing() { compare(1, 1); }
}
"""


def test_flags_hardcoded_stage_path(tmp_path):
    (tmp_path / "tst_literal.qml").write_text(STAGE_LITERAL, encoding="utf-8")
    r = subprocess.run([sys.executable, str(TOOL), "--dir", str(tmp_path)],
                       capture_output=True, text=True)
    assert r.returncode == 1, r.stdout
    assert "tst_literal.qml" in r.stdout
    assert "hardcoded stage path" in r.stdout

    (tmp_path / "tst_literal.qml").unlink()
    (tmp_path / "tst_stage.qml").write_text(STAGE_SINGLETON, encoding="utf-8")
    r2 = subprocess.run([sys.executable, str(TOOL), "--dir", str(tmp_path)],
                        capture_output=True, text=True)
    assert r2.returncode == 0, r2.stdout
