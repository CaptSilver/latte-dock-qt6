#include "imagecompare.h"

namespace LatteProbe {

QString checkInvariants(const QImage &img0, double minOpaqueFraction) {
    if (img0.isNull()) return QStringLiteral("read-back image is null");
    const QImage img = img0.convertToFormat(QImage::Format_RGBA8888);
    const int w = img.width(), h = img.height();
    qint64 opaque = 0;
    QRgb first = 0; bool firstSet = false, uniform = true;
    for (int y = 0; y < h; ++y) {
        const uchar *p = img.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const int i = x * 4;
            if (p[i + 3] > 0) ++opaque;
            const QRgb v = qRgba(p[i + 0], p[i + 1], p[i + 2], p[i + 3]);
            if (!firstSet) { first = v; firstSet = true; }
            else if (v != first) uniform = false;
        }
    }
    const double frac = (double(w) * h) > 0 ? double(opaque) / (double(w) * h) : 0.0;
    if (frac < minOpaqueFraction)
        return QStringLiteral("near-transparent: opaque fraction %1 < floor %2")
            .arg(frac).arg(minOpaqueFraction);
    if (uniform)
        return QStringLiteral("uniform flat colour — nothing rendered with variation");
    return QString();
}

} // namespace LatteProbe
