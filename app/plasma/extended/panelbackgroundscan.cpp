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

int roundnessFromMaskCorner(const QImage &corner, bool topLeftCorner)
{
    QImage img = ensurePremultiplied(corner);

    int baseRow = (topLeftCorner ? img.height() - 1 : 0);
    int baseCol = (topLeftCorner ? img.width() - 1 : 0);

    int baseLineLength = 0;
    int roundnessLines = 0;

    if (topLeftCorner) {
        //! TOPLEFT corner
        QRgb *line = (QRgb *)img.scanLine(baseRow);
        QRgb basePoint = line[baseCol];

        QRgb *isRoundedLine = (QRgb *)img.scanLine(0);
        QRgb isRoundedPoint = isRoundedLine[0];

        //! If there is roundness, if that point is not fully transparent then
        //! there is no roundness
        if (qAlpha(isRoundedPoint) == 0) {
            if (qAlpha(basePoint) > 0) {
                //! calculate the mask baseLine length
                for (int c = baseCol; c >= 0; --c) {
                    QRgb point = line[c];

                    if (qAlpha(point) > 0) {
                        baseLineLength++;
                    } else {
                        break;
                    }
                }
            }

            if (baseLineLength > 0) {
                int headLimitR = baseRow;
                int tailLimitR = baseRow;

                for (int r = baseRow - 1; r >= 0; --r) {
                    QRgb *rline = (QRgb *)img.scanLine(r);
                    QRgb fpoint = rline[baseCol];
                    if (qAlpha(fpoint) == 0) {
                        //! a line that is not part of the roundness because its first pixel is fully transparent
                        break;
                    }

                    headLimitR = r;
                }

                int c = qMax(0, img.width() - baseLineLength);

                for (int r = baseRow - 1; r >= 0; --r) {
                    QRgb *rline = (QRgb *)img.scanLine(r);
                    QRgb point = rline[c];

                    if (qAlpha(point) != 255) {
                        tailLimitR = r;
                        break;
                    }
                }

                if (headLimitR != tailLimitR) {
                    roundnessLines = tailLimitR - headLimitR + 1;
                }
            }
        }
    } else {
        //! BOTTOMRIGHT CORNER
        //! it should be TOPRIGHT corner in that case
        QRgb *line = (QRgb *)img.scanLine(baseRow);
        QRgb basePoint = line[baseCol];

        QRgb *isRoundedLine = (QRgb *)img.scanLine(img.height() - 1);
        QRgb isRoundedPoint = isRoundedLine[img.width() - 1];

        //! If there is roundness, if that point is not fully transparent then
        //! there is no roundness
        if (qAlpha(isRoundedPoint) == 0) {
            if (qAlpha(basePoint) > 0) {
                //! calculate the mask baseLine length
                for (int c = baseCol; c < img.width(); ++c) {
                    QRgb point = line[c];

                    if (qAlpha(point) > 0) {
                        baseLineLength++;
                    } else {
                        break;
                    }
                }
            }

            if (baseLineLength > 0) {
                int headLimitR = 0;
                int tailLimitR = 0;

                for (int r = baseRow + 1; r < img.height(); ++r) {
                    QRgb *rline = (QRgb *)img.scanLine(r);
                    QRgb fpoint = rline[baseCol];
                    if (qAlpha(fpoint) == 0) {
                        //! a line that is not part of the roundness because its first pixel is not transparent
                        break;
                    }

                    headLimitR = r;
                }

                int c = baseLineLength - 1;

                for (int r = baseRow + 1; r < img.height(); ++r) {
                    QRgb *rline = (QRgb *)img.scanLine(r);
                    QRgb point = rline[c];

                    if (qAlpha(point) != 255) {
                        tailLimitR = r;
                        break;
                    }
                }

                if (headLimitR != tailLimitR) {
                    roundnessLines = headLimitR - tailLimitR + 1;
                }
            }
        }
    }

    return roundnessLines;
}

int roundnessFromShadowCorner(const QImage &corner, bool topLeftCorner)
{
    QImage img = ensurePremultiplied(corner);

    int baseRow = (topLeftCorner ? img.height() - 1 : 0);
    int baseCol = (topLeftCorner ? img.width() - 1 : 0);

    int baseLineLength = 0;
    int roundnessLines = 0;

    if (topLeftCorner) {
        //! TOPLEFT corner
        QRgb *line = (QRgb *)img.scanLine(baseRow);
        QRgb basePoint = line[baseCol];

        int baseShadowMaxOpacity = 0;

        if (qAlpha(basePoint) == 0) {
            //! calculate the shadow maxOpacity in the base line
            //! and number of pixels to reach there
            for (int c = baseCol; c >= 0; --c) {
                QRgb point = line[c];

                if (qAlpha(point) > baseShadowMaxOpacity) {
                    baseShadowMaxOpacity = qAlpha(point);
                    baseLineLength = (baseCol - c + 1);
                }
            }
        }

        if (baseLineLength > 0) {
            for (int r = baseRow - 1; r >= 0; --r) {
                QRgb *rline = (QRgb *)img.scanLine(r);
                QRgb fpoint = rline[baseCol];
                if (qAlpha(fpoint) != 0) {
                    //! a line that is not part of the roundness because its first pixel is not transparent
                    break;
                }

                int transPixels = 0;
                int rowMaxOpacity = 0;

                for (int c = baseCol; c >= 0; --c) {
                    QRgb point = rline[c];

                    if (qAlpha(point) > rowMaxOpacity) {
                        rowMaxOpacity = qAlpha(point);
                        continue;
                    }
                }

                for (int c = baseCol; c >= (baseCol - baseLineLength + 1); --c) {
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
        }
    } else {
        //! BOTTOMRIGHT CORNER
        //! it should be TOPRIGHT corner in that case
        QRgb *line = (QRgb *)img.scanLine(baseRow);
        QRgb basePoint = line[baseCol];

        int baseShadowMaxOpacity = 0;

        if (qAlpha(basePoint) == 0) {
            //! calculate the base line transparent pixels
            for (int c = baseCol; c < img.width(); ++c) {
                QRgb point = line[c];

                if (qAlpha(point) > baseShadowMaxOpacity) {
                    baseShadowMaxOpacity = qAlpha(point);
                    baseLineLength = c + 1;
                }
            }
        }

        if (baseLineLength > 0) {
            for (int r = baseRow + 1; r < img.height(); ++r) {
                QRgb *rline = (QRgb *)img.scanLine(r);
                QRgb fpoint = rline[baseCol];
                if (qAlpha(fpoint) != 0) {
                    //! a line that is not part of the roundness because its first pixel is not transparent
                    break;
                }

                int transPixels = 0;
                int rowMaxOpacity = 0;

                for (int c = baseCol; c < img.width(); ++c) {
                    QRgb point = rline[c];

                    if (qAlpha(point) > rowMaxOpacity) {
                        rowMaxOpacity = qAlpha(point);
                        baseLineLength = c + 1;
                    }
                }

                for (int c = baseCol; c < baseLineLength; ++c) {
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
