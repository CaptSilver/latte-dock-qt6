// Renders nothing — must FAIL the output invariants floor (the window's flat black
// background is uniform), proving the read-back assertion works. Mirrors selftest-bad's
// role for the shader gate.
import QtQuick
Item { width: 256; height: 256 }
