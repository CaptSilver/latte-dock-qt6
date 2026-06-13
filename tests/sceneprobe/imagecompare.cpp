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

CompareResult compareImages(const QImage &a0, const QImage &e0, const CompareTolerance &tol) {
    CompareResult r;
    if (a0.isNull() || e0.isNull() || a0.size() != e0.size()) {
        const QSize s = a0.isNull() ? e0.size() : a0.size();
        r.match = false; r.diffFraction = 1.0; r.maxDelta = 255;
        r.diffBounds = QRect(QPoint(0, 0), s);
        r.diffPixels = s.width() * s.height();
        return r;
    }
    const QImage a = a0.convertToFormat(QImage::Format_RGBA8888);
    const QImage e = e0.convertToFormat(QImage::Format_RGBA8888);
    const int w = a.width(), h = a.height();
    int minX = w, minY = h, maxXb = -1, maxYb = -1;
    for (int y = 0; y < h; ++y) {
        const uchar *pa = a.constScanLine(y);
        const uchar *pe = e.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const int i = x * 4;
            const int d = qMax(qMax(qAbs(pa[i+0]-pe[i+0]), qAbs(pa[i+1]-pe[i+1])),
                               qMax(qAbs(pa[i+2]-pe[i+2]), qAbs(pa[i+3]-pe[i+3])));
            if (d > r.maxDelta) {
                r.maxDelta = d; r.maxX = x; r.maxY = y;
                r.expectedAtMax = QColor(pe[i+0], pe[i+1], pe[i+2], pe[i+3]);
                r.actualAtMax   = QColor(pa[i+0], pa[i+1], pa[i+2], pa[i+3]);
            }
            if (d > tol.perChannelDelta) {
                ++r.diffPixels;
                minX = qMin(minX, x); minY = qMin(minY, y);
                maxXb = qMax(maxXb, x); maxYb = qMax(maxYb, y);
            }
        }
    }
    const double total = double(w) * double(h);
    r.diffFraction = total > 0 ? r.diffPixels / total : 0.0;
    if (maxXb >= 0) r.diffBounds = QRect(QPoint(minX, minY), QPoint(maxXb, maxYb));
    r.match = r.diffFraction <= tol.maxExceedFraction;
    return r;
}

static QColor parseColor(const QVariant &v) { return QColor(v.toString()); }

QString checkExpectations(const QImage &img0, const QVariantList &exps) {
    if (exps.isEmpty()) return QString();
    const QImage img = img0.convertToFormat(QImage::Format_RGBA8888);
    for (const QVariant &ev : exps) {
        const QVariantMap m = ev.toMap();
        if (m.contains(QStringLiteral("minOpaqueFraction"))) {
            const QString e = checkInvariants(img, m.value(QStringLiteral("minOpaqueFraction")).toDouble());
            if (!e.isEmpty()) return e;
            continue;
        }
        const double tol = m.value(QStringLiteral("tol"), 0.1).toDouble() * 255.0;
        if (m.contains(QStringLiteral("x")) && m.contains(QStringLiteral("y")) && m.contains(QStringLiteral("rgba"))) {
            const int x = m.value(QStringLiteral("x")).toInt(), y = m.value(QStringLiteral("y")).toInt();
            if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                return QStringLiteral("expect pixel (%1,%2) out of bounds").arg(x).arg(y);
            const QColor got = img.pixelColor(x, y), want = parseColor(m.value(QStringLiteral("rgba")));
            // Compare R/G/B only: the probe composites onto opaque black, so readback alpha is constant 255.
            if (qAbs(got.red()-want.red()) > tol || qAbs(got.green()-want.green()) > tol ||
                qAbs(got.blue()-want.blue()) > tol)
                return QStringLiteral("pixel (%1,%2) %3 != expected %4 (tol %5)")
                    .arg(x).arg(y).arg(got.name(), want.name()).arg(int(tol));
            continue;
        }
        if (m.contains(QStringLiteral("region")) && m.contains(QStringLiteral("meanRgba"))) {
            const QVariantList rl = m.value(QStringLiteral("region")).toList();
            if (rl.size() != 4) return QStringLiteral("region must be [x,y,w,h]");
            const QRect clipped = QRect(rl[0].toInt(), rl[1].toInt(), rl[2].toInt(), rl[3].toInt())
                                      .intersected(img.rect());
            if (clipped.isEmpty()) return QStringLiteral("region empty/out of bounds");
            qint64 sr = 0, sg = 0, sb = 0, n = 0;
            for (int y = clipped.top(); y <= clipped.bottom(); ++y)
                for (int x = clipped.left(); x <= clipped.right(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    sr += c.red(); sg += c.green(); sb += c.blue(); ++n;
                }
            const QColor want = parseColor(m.value(QStringLiteral("meanRgba")));
            if (qAbs(int(sr/n)-want.red()) > tol || qAbs(int(sg/n)-want.green()) > tol ||
                qAbs(int(sb/n)-want.blue()) > tol)
                return QStringLiteral("region mean != expected %1 (tol %2)")
                    .arg(want.name()).arg(int(tol));
            continue;
        }
        return QStringLiteral("unrecognized probeExpect entry");
    }
    return QString();
}

QImage amplifiedDiff(const QImage &a0, const QImage &e0, int gain) {
    if (a0.isNull() || e0.isNull() || a0.size() != e0.size()) {
        QImage red(a0.isNull() ? e0.size() : a0.size(), QImage::Format_RGBA8888);
        red.fill(QColor(255, 0, 0, 255));
        return red;
    }
    const QImage a = a0.convertToFormat(QImage::Format_RGBA8888);
    const QImage e = e0.convertToFormat(QImage::Format_RGBA8888);
    QImage out(a.size(), QImage::Format_RGBA8888);
    const int w = a.width(), h = a.height();
    for (int y = 0; y < h; ++y) {
        const uchar *pa = a.constScanLine(y);
        const uchar *pe = e.constScanLine(y);
        uchar *po = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const int i = x * 4;
            po[i+0] = uchar(qMin(255, qAbs(pa[i+0]-pe[i+0]) * gain));
            po[i+1] = uchar(qMin(255, qAbs(pa[i+1]-pe[i+1]) * gain));
            po[i+2] = uchar(qMin(255, qAbs(pa[i+2]-pe[i+2]) * gain));
            po[i+3] = 255;
        }
    }
    return out;
}

QString verdictLine(const QString &scene, const QString &device, const CompareResult &r) {
    if (r.match)
        return QStringLiteral("scene %1 — pixel match (%2)").arg(scene, device);
    return QStringLiteral("scene %1 [%2] — %3 px differ (%4%), max Δ=%5 at (%6,%7) expected %8 got %9")
        .arg(scene, device).arg(r.diffPixels)
        .arg(QString::number(r.diffFraction * 100.0, 'f', 2))
        .arg(r.maxDelta).arg(r.maxX).arg(r.maxY)
        .arg(r.expectedAtMax.name(QColor::HexArgb), r.actualAtMax.name(QColor::HexArgb));
}

} // namespace LatteProbe
