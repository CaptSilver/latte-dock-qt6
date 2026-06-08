/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Unit test for the shadow-blur normalization helper. MultiEffect.shadowBlur is a
// 0..1 fraction of blurMax, where the old DropShadow.radius was an absolute pixel
// count — shadowBlurFor maps px -> fraction and clamps the ends.

import QtQuick
import QtTest
import "../../declarativeimports/components/code/EffectMath.js" as EffectMath

TestCase {
    id: testCase
    name: "EffectMath"

    function test_zeroSize() { compare(EffectMath.shadowBlurFor(0, 64), 0.0); }
    function test_sizeEqualsCeiling() { compare(EffectMath.shadowBlurFor(64, 64), 1.0); }
    function test_sizeAboveCeilingClamps() { compare(EffectMath.shadowBlurFor(200, 64), 1.0); }
    function test_negativeSize() { compare(EffectMath.shadowBlurFor(-5, 64), 0.0); }
    function test_negativeCeiling() { compare(EffectMath.shadowBlurFor(32, -1), 0.0); }
    function test_midRange() { compare(EffectMath.shadowBlurFor(32, 64), 0.5); }
}
