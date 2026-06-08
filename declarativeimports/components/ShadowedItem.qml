/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Effects
import "code/EffectMath.js" as EffectMath

// A MultiEffect preconfigured as a drop shadow. The root IS a MultiEffect, so
// callers set shadowColor/source/shadowHorizontalOffset/shadowVerticalOffset
// directly; this only adds shadowSizePx (the old px radius) and the blur ceiling.
MultiEffect {
    id: root
    property real shadowSizePx: 0      // == old DropShadow.radius in px
    // Pixel radius at which shadowBlur saturates to 1.0 (shadowSizePx == blurMaxPx
    // -> full blur). Sits above the largest itemShadow.size (0.5 * the 512px
    // icon-size cap = 256), so big-icon shadows scale instead of clamping early.
    property int  blurMaxPx: 256
    autoPaddingEnabled: true
    shadowEnabled: true
    shadowOpacity: 1.0
    shadowHorizontalOffset: 0
    // Default matches the common old DropShadow.verticalOffset: 2; sites whose
    // old shadow used a different offset override this.
    shadowVerticalOffset: 2
    blurMax: blurMaxPx
    shadowBlur: EffectMath.shadowBlurFor(shadowSizePx, blurMaxPx)
}
