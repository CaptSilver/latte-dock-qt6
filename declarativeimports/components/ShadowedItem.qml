/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Effects
import "code/EffectMath.js" as EffectMath
MultiEffect {
    id: root
    property real shadowSizePx: 0      // == old DropShadow.radius in px
    property int  blurMaxPx: 64         // normalization ceiling (tuned live)
    autoPaddingEnabled: true
    shadowEnabled: true
    shadowOpacity: 1.0
    shadowHorizontalOffset: 0
    shadowVerticalOffset: 2
    blurMax: blurMaxPx
    shadowBlur: EffectMath.shadowBlurFor(shadowSizePx, blurMaxPx)
}
