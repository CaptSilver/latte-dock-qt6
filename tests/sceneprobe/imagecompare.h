#pragma once
#include <QImage>
#include <QColor>
#include <QRect>
#include <QString>
#include <QVariantList>

namespace LatteProbe {

struct CompareTolerance {
    int perChannelDelta = 0;       // max allowed |delta| per channel (0..255); 0 = exact
    double maxExceedFraction = 0.0; // fraction of pixels allowed to exceed perChannelDelta
};

struct CompareResult {
    bool match = true;
    int diffPixels = 0;
    double diffFraction = 0.0;
    int maxDelta = 0;
    int maxX = -1;
    int maxY = -1;
    QColor expectedAtMax;
    QColor actualAtMax;
    QRect diffBounds;
};

// "" if OK, else a human-readable failure reason.
QString checkInvariants(const QImage &actual, double minOpaqueFraction);
// "" if all probeExpect entries pass, else first failure reason.
QString checkExpectations(const QImage &actual, const QVariantList &expectations);
CompareResult compareImages(const QImage &actual, const QImage &expected, const CompareTolerance &tol);
QImage amplifiedDiff(const QImage &actual, const QImage &expected, int gain = 8);
QString verdictLine(const QString &scene, const QString &device, const CompareResult &r);

} // namespace LatteProbe
