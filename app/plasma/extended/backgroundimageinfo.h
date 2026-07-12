/*
    SPDX-FileCopyrightText: 2018 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BACKGROUNDIMAGEINFO_H
#define BACKGROUNDIMAGEINFO_H

// Plasma
#include <Plasma/Plasma>

class QImage;

namespace Latte {
namespace PlasmaExtended {

//! Pure image-analysis helpers lifted out of BackgroundCache so the tiling and
//! brightness/busy maths are directly unit-testable without ScreenPool, KDirWatch
//! or a live desktop. BackgroundCache::updateImageCalculations delegates here.
namespace BackgroundImageInfo {

struct EdgeHints
{
    float brightness{-1000};
    bool busy{false};
};

//! Average pixel brightness (Latte::colorBrightness) over the half-open area
//! [firstRow, endRow) x [firstColumn, endColumn). Returns -1000 for an invalid image.
float brightnessFromArea(const QImage &image, int firstRow, int firstColumn, int endRow, int endColumn);

//! An edge strip is "busy" when its darkest and brightest tiles land on opposite
//! sides of the light/dark threshold, or a value falls out of the [0,255] range.
bool areaIsBusy(float bright1, float bright2);

//! Splits the strip along the given edge into up to ten tiles, averages their
//! brightness, and flags the strip busy when the tiles disagree. This is the core
//! of BackgroundCache::updateImageCalculations minus the cache bookkeeping.
EdgeHints edgeHints(const QImage &image, Plasma::Types::Location location);

}
}
}

#endif
