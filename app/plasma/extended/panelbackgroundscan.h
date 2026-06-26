/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PANELBACKGROUNDSCAN_H
#define PANELBACKGROUNDSCAN_H

// Qt
#include <QColor>
#include <QImage>
#include <QtGlobal>

namespace Latte {
namespace PlasmaExtended {
namespace PanelBackgroundScan {

// Ensure the image is in premultiplied-alpha format before scanning, so qAlpha()
// reads consistently. Returns a converted copy only when needed.
QImage ensurePremultiplied(const QImage &img);

// Core of updateMaxOpacity: sum alpha over the first min(2,h) rows × full width,
// divide by (rows × width), then clamp to the 0.01f floor so a fully-transparent
// panel background never produces a zero-opacity result.
// Returns 0.01f when the image is empty (w==0 or h==0).
float maxOpacityFromCenter(const QImage &center);

// Core of updateRoundnessFromMask: pixel-walk the supplied corner image.
// topLeftCorner=true → topleft corner (BottomEdge/RightEdge panels);
// topLeftCorner=false → bottomright corner (TopEdge/LeftEdge panels).
// Returns >= 0.
int roundnessFromMaskCorner(const QImage &corner, bool topLeftCorner);

// Core of updateRoundnessFromShadows: same corner choice / return convention.
int roundnessFromShadowCorner(const QImage &corner, bool topLeftCorner);

// Pixel scan result for a single panel border image.
struct EdgeShadow
{
    int discoveredSize{0};
    QColor color; // invalid (QColor()) when no opaque pixel was found
};

// Core of the heuristic size + dominant-color scan from updateShadow.
// Does NOT touch themeshadowsize or hasShadow() — those stay in the adapter.
// horizontal=true → scan column 0 down all rows (BottomEdge/TopEdge panels);
// horizontal=false → scan row 0 across all columns (LeftEdge/RightEdge panels).
EdgeShadow shadowFromBorder(const QImage &border, bool horizontal);

} // namespace PanelBackgroundScan
} // namespace PlasmaExtended
} // namespace Latte

#endif // PANELBACKGROUNDSCAN_H
