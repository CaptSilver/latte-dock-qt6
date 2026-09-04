/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "panelbackgroundscan.h"

namespace Latte {
namespace PlasmaExtended {
namespace PanelBackgroundScan {

QImage ensurePremultiplied(const QImage &img)
{
    if (img.format() == QImage::Format_ARGB32_Premultiplied) {
        return img;
    }
    return img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

float maxOpacityFromCenter(const QImage &center)
{
    const int w = center.width();
    const int h = center.height();

    if (w == 0 || h == 0) {
        return 0.01f;
    }

    QImage img = ensurePremultiplied(center);

    const int rows = qMin(2, h);
    float alphasum{0};

    for (int row = 0; row < rows; ++row) {
        QRgb *line = (QRgb *)img.scanLine(row);

        for (int col = 0; col < w; ++col) {
            QRgb pixelData = line[col];
            alphasum += ((float)qAlpha(pixelData) / (float)255);
        }
    }

    float result = alphasum / (float)(rows * w);
    return qMax(0.01f, result);
}

namespace {

//! Both roundness scanners read the same corner twice over: for BottomEdge/RightEdge
//! panels the interesting corner is the topleft one, walked from the bottom-right pixel
//! back towards (0,0); for TopEdge/LeftEdge panels it is the bottomright corner, walked
//! from (0,0) forwards. Only the direction differs, so it lives here and each scan is
//! written once.
struct CornerWalk
{
    int baseRow{0};
    int baseCol{0};
    int oppositeRow{0}; //! the corner diagonally across from the base pixel
    int oppositeCol{0};
    int step{1}; //! +1 walks away from (0,0), -1 walks back towards it

    bool holdsRow(int r) const
    {
        return (step > 0 ? r <= oppositeRow : r >= 0);
    }

    bool holdsColumn(int c) const
    {
        return (step > 0 ? c <= oppositeCol : c >= 0);
    }

    //! How far column c sits from the base column, counted inclusively.
    int reach(int c) const
    {
        return (step * (c - baseCol)) + 1;
    }

    //! The column that closes a run of `length` pixels starting at the base column.
    int columnAtReach(int length) const
    {
        return baseCol + (step * (length - 1));
    }
};

CornerWalk cornerWalk(const QImage &img, bool topLeftCorner)
{
    CornerWalk walk;
    walk.baseRow = (topLeftCorner ? img.height() - 1 : 0);
    walk.baseCol = (topLeftCorner ? img.width() - 1 : 0);
    walk.oppositeRow = (topLeftCorner ? 0 : img.height() - 1);
    walk.oppositeCol = (topLeftCorner ? 0 : img.width() - 1);
    walk.step = (topLeftCorner ? -1 : 1);
    return walk;
}

} // namespace

int roundnessFromMaskCorner(const QImage &corner, bool topLeftCorner)
{
    QImage img = ensurePremultiplied(corner);

    //! A theme that is missing the corner element hands us a null image; there is no
    //! roundness to find in one, and scanLine() would walk a buffer that is not there.
    if (img.isNull()) {
        return 0;
    }

    const CornerWalk walk = cornerWalk(img, topLeftCorner);

    QRgb *line = (QRgb *)img.scanLine(walk.baseRow);
    QRgb basePoint = line[walk.baseCol];

    QRgb *isRoundedLine = (QRgb *)img.scanLine(walk.oppositeRow);
    QRgb isRoundedPoint = isRoundedLine[walk.oppositeCol];

    //! If the pixel across the diagonal is not fully transparent then the corner is
    //! square and there is no roundness to measure.
    if (qAlpha(isRoundedPoint) != 0) {
        return 0;
    }

    int baseLineLength = 0;
    int roundnessLines = 0;

    if (qAlpha(basePoint) > 0) {
        //! calculate the mask baseLine length
        for (int c = walk.baseCol; walk.holdsColumn(c); c += walk.step) {
            QRgb point = line[c];

            if (qAlpha(point) > 0) {
                baseLineLength++;
            } else {
                break;
            }
        }
    }

    if (baseLineLength > 0) {
        int headLimitR = walk.baseRow;
        int tailLimitR = walk.baseRow;

        for (int r = walk.baseRow + walk.step; walk.holdsRow(r); r += walk.step) {
            QRgb *rline = (QRgb *)img.scanLine(r);
            QRgb fpoint = rline[walk.baseCol];
            if (qAlpha(fpoint) == 0) {
                //! a line that is not part of the roundness because its first pixel is fully transparent
                break;
            }

            headLimitR = r;
        }

        const int c = walk.columnAtReach(baseLineLength);

        for (int r = walk.baseRow + walk.step; walk.holdsRow(r); r += walk.step) {
            QRgb *rline = (QRgb *)img.scanLine(r);
            QRgb point = rline[c];

            if (qAlpha(point) != 255) {
                tailLimitR = r;
                break;
            }
        }

        if (headLimitR != tailLimitR) {
            roundnessLines = (walk.step * (headLimitR - tailLimitR)) + 1;
        }
    }

    return roundnessLines;
}

//! 1.  The caller picks which corner shadow to read from the panel location.
//! 2.  For that corner discover the maxOpacity (most solid shadow point) and how many
//!     pixels (distance) it takes to reach it, that is called [baseLineLength]
//! 3.  After [2] for each next line calculate the maxOpacity for that line and how many
//!     points are needed to reach there. If the points to reach the line max opacity are
//!     shorter than baseLineLength then that line is considered part of the roundness
//! 3.1 Avoid zig-zag cases such as the Air plasma theme case. When the shadow is not
//!     following a straight line until reaching the rounded part only the last part of
//!     the discovered roundness counts and everything before it is ignored.
//! 4.  The lines that are shorter than the baseline are the discovered roundness
int roundnessFromShadowCorner(const QImage &corner, bool topLeftCorner)
{
    QImage img = ensurePremultiplied(corner);

    //! A theme that is missing the corner element hands us a null image; there is no
    //! roundness to find in one, and scanLine() would walk a buffer that is not there.
    if (img.isNull()) {
        return 0;
    }

    const CornerWalk walk = cornerWalk(img, topLeftCorner);

    QRgb *line = (QRgb *)img.scanLine(walk.baseRow);
    QRgb basePoint = line[walk.baseCol];

    int baseLineLength = 0;
    int roundnessLines = 0;

    if (qAlpha(basePoint) == 0) {
        //! 2. the shadow maxOpacity in the base line and the pixels needed to reach it
        int baseShadowMaxOpacity = 0;

        for (int c = walk.baseCol; walk.holdsColumn(c); c += walk.step) {
            QRgb point = line[c];

            if (qAlpha(point) > baseShadowMaxOpacity) {
                baseShadowMaxOpacity = qAlpha(point);
                baseLineLength = walk.reach(c);
            }
        }
    }

    if (baseLineLength <= 0) {
        return 0;
    }

    for (int r = walk.baseRow + walk.step; walk.holdsRow(r); r += walk.step) {
        QRgb *rline = (QRgb *)img.scanLine(r);
        QRgb fpoint = rline[walk.baseCol];
        if (qAlpha(fpoint) != 0) {
            //! a line that is not part of the roundness because its first pixel is not transparent
            break;
        }

        //! 3. this line's own most solid shadow point
        int rowMaxOpacity = 0;

        for (int c = walk.baseCol; walk.holdsColumn(c); c += walk.step) {
            QRgb point = rline[c];

            if (qAlpha(point) > rowMaxOpacity) {
                rowMaxOpacity = qAlpha(point);
            }
        }

        int transPixels = 0;

        for (int c = walk.baseCol; walk.reach(c) <= baseLineLength; c += walk.step) {
            QRgb point = rline[c];

            if (qAlpha(point) != rowMaxOpacity) {
                transPixels++;
                continue;
            }

            if (transPixels != baseLineLength) {
                roundnessLines++;
                break;
            }
        }

        if (transPixels == baseLineLength) {
            //! 3.1 avoid zig-zag shadows Air plasma theme case
            roundnessLines = 0;
        }
    }

    return roundnessLines;
}

EdgeShadow shadowFromBorder(const QImage &border, bool horizontal)
{
    QImage img = ensurePremultiplied(border);

    int firstPixel{-1};
    int lastPixel{-1};

    if (horizontal) {
        for (int y = 0; y < img.height(); ++y) {
            QRgb *line = (QRgb *)img.scanLine(y);
            QRgb pixel = line[0];

            if (qAlpha(pixel) > 0) {
                if (firstPixel < 0) {
                    firstPixel = y;
                    lastPixel = y;
                } else {
                    lastPixel = y;
                }
            }
        }
    } else {
        QRgb *line = (QRgb *)img.scanLine(0);
        for (int x = 0; x < img.width(); ++x) {
            QRgb pixel = line[x];

            if (qAlpha(pixel) > 0) {
                if (firstPixel < 0) {
                    firstPixel = x;
                    lastPixel = x;
                } else {
                    lastPixel = x;
                }
            }
        }
    }

    EdgeShadow result;
    result.discoveredSize = (firstPixel >= 0 ? qMax(0, lastPixel - firstPixel + 1) : 0);

    // Scan all pixels to find the one with the highest alpha; that is the shadow colour.
    // Leave result.color as a default-constructed (invalid) QColor when nothing is found.
    int maxopacity{0};

    for (int r = 0; r < img.height(); ++r) {
        QRgb *line = (QRgb *)img.scanLine(r);

        for (int c = 0; c < img.width(); ++c) {
            QRgb pixel = line[c];

            if (qAlpha(pixel) > maxopacity) {
                maxopacity = qAlpha(pixel);
                result.color = QColor(pixel);
                result.color.setAlpha(qMin(255, maxopacity));
            }
        }
    }

    return result;
}

} // namespace PanelBackgroundScan
} // namespace PlasmaExtended
} // namespace Latte
