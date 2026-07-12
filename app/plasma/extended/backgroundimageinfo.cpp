/*
    SPDX-FileCopyrightText: 2018 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "backgroundimageinfo.h"

// local
#include "../../tools/commontools.h"

// Qt
#include <QImage>
#include <QList>
#include <QRgb>

namespace Latte {
namespace PlasmaExtended {
namespace BackgroundImageInfo {

float brightnessFromArea(const QImage &image, int firstRow, int firstColumn, int endRow, int endColumn)
{
    float areaBrightness = -1000;

    if (image.format() != QImage::Format_Invalid) {
        for (int row = firstRow; row < endRow; ++row) {
            const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(row));

            for (int col = firstColumn; col < endColumn; ++col) {
                QRgb pixelData = line[col];
                float pixelBrightness = Latte::colorBrightness(pixelData);

                areaBrightness = (areaBrightness == -1000) ? pixelBrightness : (areaBrightness + pixelBrightness);
            }
        }

        float areaSize = (endRow - firstRow) * (endColumn - firstColumn);
        areaBrightness = areaBrightness / areaSize;
    }

    return areaBrightness;
}

bool areaIsBusy(float bright1, float bright2)
{
    bool bright1IsLight = bright1 >= 123;
    bool bright2IsLight = bright2 >= 123;

    bool inBounds = bright1 >= 0 && bright2 <= 255 && bright2 >= 0 && bright2 <= 255;

    return !inBounds || bright1IsLight != bright2IsLight;
}

//! In order to calculate the brightness and busy hints for a specific image the code
//! does the following. It is not needed to calculate these values for the entire
//! image, which would be cpu costly. The area along the interesting edge is split
//! into ten tiles and each tile's brightness is computed. Their average is the strip
//! brightness; comparing the minimum and maximum tile brightness tells us whether the
//! strip is busy (a big difference means busy).
EdgeHints edgeHints(const QImage &image, Plasma::Types::Location location)
{
    EdgeHints hints;

    if (image.format() == QImage::Format_Invalid) {
        return hints;
    }

    float maxBrightness{0};
    float minBrightness{255};

    bool vertical = (location == Plasma::Types::LeftEdge || location == Plasma::Types::RightEdge) ? true : false;
    int imageLength = !vertical ? image.width() : image.height();
    int tiles{qMin(10, imageLength)};

    //! 24px. should be enough because the views are always snapped to edges
    int tileThickness = !vertical ? qMin(24, image.height()) : qMin(24, image.width());

    float factor = ((float)100 / tiles) / 100;

    QList<float> subBrightness;

    //! Iterating algorithm
    int firstRow = 0;
    int firstColumn = 0;
    int endRow = 0;
    int endColumn = 0;

    //! horizontal tiles calculations
    if (location == Plasma::Types::TopEdge) {
        firstRow = 0;
        endRow = tileThickness;
    } else if (location == Plasma::Types::BottomEdge) {
        firstRow = image.height() - tileThickness - 1;
        endRow = image.height() - 1;
    }

    if (!vertical) {
        for (int i = 1; i <= tiles; ++i) {
            float subFactor = ((float)i) * factor;
            firstColumn = endColumn + 1;
            endColumn = (subFactor * imageLength) - 1;
            endColumn = qMin(endColumn, imageLength - 1);

            int tempBrightness = brightnessFromArea(image, firstRow, firstColumn, endRow, endColumn);

            subBrightness.append(tempBrightness);

            if (tempBrightness > maxBrightness) {
                maxBrightness = tempBrightness;
            }
            if (tempBrightness < minBrightness) {
                minBrightness = tempBrightness;
            }
        }
    }

    //! vertical tiles calculations
    if (location == Plasma::Types::LeftEdge) {
        firstColumn = 0;
        endColumn = tileThickness;
    } else if (location == Plasma::Types::RightEdge) {
        firstColumn = image.width() - 1 - tileThickness;
        endColumn = image.width() - 1;
    }

    if (vertical) {
        for (int i = 1; i <= tiles; ++i) {
            float subFactor = ((float)i) * factor;
            firstRow = endRow + 1;
            endRow = (subFactor * imageLength) - 1;
            endRow = qMin(endRow, imageLength - 1);

            int tempBrightness = brightnessFromArea(image, firstRow, firstColumn, endRow, endColumn);

            subBrightness.append(tempBrightness);

            if (tempBrightness > maxBrightness) {
                maxBrightness = tempBrightness;
            }
            if (tempBrightness < minBrightness) {
                minBrightness = tempBrightness;
            }
        }
    }

    //! compute total brightness for this area
    float subBrightnessSum = 0;

    for (int i = 0; i < subBrightness.count(); ++i) {
        subBrightnessSum = subBrightnessSum + subBrightness[i];
    }

    hints.brightness = subBrightnessSum / subBrightness.count();
    hints.busy = areaIsBusy(minBrightness, maxBrightness);

    return hints;
}

}
}
}
