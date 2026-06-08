/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

.pragma library
function shadowBlurFor(sizePx, blurMaxPx) {
    if (sizePx <= 0 || blurMaxPx <= 0) return 0.0;
    return Math.min(1.0, sizePx / blurMaxPx);
}
