/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Tests for the pure pixel-scanning math extracted from PanelBackground.
// Each function is tested with a small hand-crafted QImage; expected values for
// non-trivial loops are pinned from an instrument-first run, not computed by hand.

#include "panelbackgroundscan.h"

#include <QtTest>

#include <random>

using namespace Latte::PlasmaExtended;

class PanelBackgroundScanTest : public QObject
{
    Q_OBJECT

private:
    // Creates a transparent premultiplied-ARGB image of the given size.
    static QImage argb(int w, int h)
    {
        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        return img;
    }

    // Sets the alpha (and optional RGB) of a single pixel.
    static void setA(QImage &img, int x, int y, int alpha, QRgb rgb = qRgb(0, 0, 0))
    {
        img.setPixel(x, y, qRgba(qRed(rgb), qGreen(rgb), qBlue(rgb), alpha));
    }

    // Rotates by 180 degrees by copying raw premultiplied words, so no colour
    // conversion can perturb the alphas the scanners key off.
    static QImage rotated180(const QImage &src)
    {
        QImage in = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QImage out(in.width(), in.height(), QImage::Format_ARGB32_Premultiplied);

        for (int y = 0; y < in.height(); ++y) {
            const QRgb *sline = reinterpret_cast<const QRgb *>(in.constScanLine(y));
            QRgb *dline = reinterpret_cast<QRgb *>(out.scanLine(in.height() - 1 - y));

            for (int x = 0; x < in.width(); ++x) {
                dline[in.width() - 1 - x] = sline[x];
            }
        }

        return out;
    }

    // A bottomright (topLeftCorner=false) shadow whose per-row peak sits FURTHER OUT
    // than the base line's peak. Every other shadow fixture here keeps the two in the
    // same column, which is why none of them can see the base line being re-measured.
    //
    // Row 0 (base line): col 1 = 100, col 2 = 200 -> peak at col 2, baseLineLength = 3.
    // Row 1: peak at col 5, well past the base line's reach of 3.
    // Rows 2-3: ordinary rows peaking back at col 2.
    // Row 4: col 0 opaque -> the row loop stops.
    static QImage shadowCornerWithOutlyingRowPeak()
    {
        QImage img = argb(8, 8);

        setA(img, 1, 0, 100);
        setA(img, 2, 0, 200);

        setA(img, 1, 1, 100);
        setA(img, 5, 1, 250);

        for (int r = 2; r <= 3; ++r) {
            setA(img, 1, r, 100);
            setA(img, 2, r, 200);
        }

        setA(img, 0, 4, 255);

        return img;
    }

    // A bottomright shadow that ramps to its peak at col 2 on every row, base line
    // included, so the per-row peak and the base line peak share a column.
    static QImage shadowCornerMonotonicRamp()
    {
        QImage img = argb(8, 8);

        for (int r = 0; r <= 3; ++r) {
            setA(img, 1, r, 100);
            setA(img, 2, r, 200);
        }

        setA(img, 0, 4, 255);

        return img;
    }

    // The bottomright staircase used by maskRoundness_steppedCorner_returnsLineCount.
    static QImage steppedMaskCorner()
    {
        QImage img = argb(8, 8);

        for (int c = 0; c <= 5; ++c) {
            setA(img, c, 0, 255);
        }
        for (int r = 1; r <= 4; ++r) {
            setA(img, 0, r, 255);
            for (int c = 1; c <= 4; ++c) {
                setA(img, c, r, 128);
            }
            setA(img, 5, r, 200);
        }

        return img;
    }

private Q_SLOTS:
    // ---- maxOpacityFromCenter ----

    void maxOpacity_fullyOpaqueCenter_returns1();
    void maxOpacity_fullyTransparent_clampsToFloor();
    void maxOpacity_halfAlpha_only2RowsMatter();
    void maxOpacity_singleRowTall_h1();

    // ---- roundnessFromMaskCorner ----

    void maskRoundness_roundedPointOpaque_returns0();
    void maskRoundness_squareCorner_returns0();
    void maskRoundness_steppedCorner_returnsLineCount();
    void maskRoundness_topLeft_mirrorsBottomRight();
    void maskRoundness_mirrorEquivalence();
    void maskRoundness_singleRow_bottomRight_findsNoRoundness();
    void maskRoundness_singleRow_topLeft_findsNoRoundness();

    // ---- roundnessFromShadowCorner ----

    void shadowRoundness_emptyShadow_returns0();
    void shadowRoundness_zigZagCollapsesToZero();
    void shadowRoundness_monotonicRamp_returnsLineCount();
    void shadowRoundness_mirrorEquivalence();
    void shadowRoundness_perRowMaxBeyondBaseline_doesNotExtendBaseline();
    void shadowRoundness_singleRow_bottomRight_findsNoRoundness();
    void shadowRoundness_singleRow_topLeft_findsNoRoundness();

    // ---- both roundness scanners ----

    void roundness_nullCorner_returns0();
    void roundness_mirrorEquivalence_overRandomCorners();

    // ---- shadowFromBorder ----

    void shadow_horizontalBand_sizeIsSpan();
    void shadow_verticalBand_sizeIsSpan();
    void shadow_color_picksMaxAlphaPixel();
    void shadow_noOpaque_zeroAndInvalid();
};

// ---- maxOpacityFromCenter ----

void PanelBackgroundScanTest::maxOpacity_fullyOpaqueCenter_returns1()
{
    // 100x50, all pixels alpha=255 → average = 1.0
    QImage img = argb(100, 50);
    for (int r = 0; r < 50; ++r) {
        for (int c = 0; c < 100; ++c) {
            setA(img, c, r, 255);
        }
    }
    QVERIFY(qFuzzyCompare(PanelBackgroundScan::maxOpacityFromCenter(img), 1.0f));
}

void PanelBackgroundScanTest::maxOpacity_fullyTransparent_clampsToFloor()
{
    // All pixels transparent → raw average 0.0 → clamped to the 0.01f floor
    QImage img = argb(100, 50);
    QVERIFY(qFuzzyCompare(PanelBackgroundScan::maxOpacityFromCenter(img), 0.01f));
}

void PanelBackgroundScanTest::maxOpacity_halfAlpha_only2RowsMatter()
{
    // Rows 0-1 have alpha=128, rows 2+ have alpha=0.
    // The function only sums rows 0..min(2,h)-1 = rows 0 and 1.
    // Expected: (128/255 * 100 * 2) / (2 * 100) = 128/255 ≈ 0.502.
    QImage img = argb(100, 50);
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 100; ++c) {
            setA(img, c, r, 128);
        }
    }
    float result = PanelBackgroundScan::maxOpacityFromCenter(img);
    QVERIFY2(result > 0.49f && result < 0.52f,
             qPrintable(QStringLiteral("expected ~0.502, got %1").arg(static_cast<double>(result))));
}

void PanelBackgroundScanTest::maxOpacity_singleRowTall_h1()
{
    // 100x1: only row 0 is scanned (min(2,1)=1). Must not scanLine(1).
    QImage img = argb(100, 1);
    for (int c = 0; c < 100; ++c) {
        setA(img, c, 0, 255);
    }
    QVERIFY(qFuzzyCompare(PanelBackgroundScan::maxOpacityFromCenter(img), 1.0f));
}

// ---- roundnessFromMaskCorner (bottomright = topLeftCorner false) ----

void PanelBackgroundScanTest::maskRoundness_roundedPointOpaque_returns0()
{
    // bottomright corner: the "is it rounded?" check is pixel (w-1,h-1).
    // Make that pixel opaque → isRoundedPoint alpha != 0 → returns 0 immediately.
    QImage img = argb(8, 8);
    setA(img, 7, 7, 200); // corner pixel opaque = not rounded
    QCOMPARE(PanelBackgroundScan::roundnessFromMaskCorner(img, false), 0);
}

void PanelBackgroundScanTest::maskRoundness_squareCorner_returns0()
{
    // A perfectly square corner has alpha > 0 in the base row's first pixel,
    // but headLimitR == tailLimitR (no taper) → 0.
    // bottomright: baseRow=0, baseCol=0. Fill the whole image opaque.
    QImage img = argb(8, 8);
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            setA(img, c, r, 255);
        }
    }
    QCOMPARE(PanelBackgroundScan::roundnessFromMaskCorner(img, false), 0);
}

void PanelBackgroundScanTest::maskRoundness_steppedCorner_returnsLineCount()
{
    // Build a bottomright (topLeftCorner=false) staircase corner in an 8x8 image.
    // baseRow=0, baseCol=0. The "is it rounded?" check is img(7,7) → keep transparent.
    //
    // Base row (r=0): cols 0..5 opaque → baseLineLength=6, basePoint alpha>0.
    // Rows r=1..4: col 0 (=baseCol) opaque so the outer loop continues; col 5 (=baseLineLength-1=5) must NOT be 255 to set tailLimitR.
    // Row r=1: col 5 = alpha 200 (not 255) → tailLimitR=1, headLimitR=1 (col 0 opaque) → no change after that.
    // Row r=5: col 0 transparent → outer loop breaks.
    //
    // Instrument-first: the actual result is pinned by running the test.
    int result = PanelBackgroundScan::roundnessFromMaskCorner(steppedMaskCorner(), false);
    // For the staircase above: headLimitR=1, tailLimitR=4 → 4-1+1=4.
    QCOMPARE(result, 4);
}

void PanelBackgroundScanTest::maskRoundness_topLeft_mirrorsBottomRight()
{
    // topleft (topLeftCorner=true): baseRow=h-1=7, baseCol=w-1=7.
    // isRoundedPoint is img(0,0) → keep transparent so roundness check proceeds.
    // Base row (r=7): col 7 opaque (basePoint alpha>0) → baseLineLength scan from col 7 down.
    // Make cols 2..7 opaque in row 7 → baseLineLength=6.
    // Rows r=6..3: col 7 opaque (head scan continues); col 8-6=2 NOT 255 → sets tailLimitR.
    QImage img = argb(8, 8);

    // Row 7 (baseRow for topleft): cols 2..7 opaque
    for (int c = 2; c <= 7; ++c) {
        setA(img, c, 7, 255);
    }
    // Rows 6..3: col 7 opaque, col 2 = 200 (not 255), cols 3..6 = 128
    for (int r = 3; r <= 6; ++r) {
        setA(img, 7, r, 255);
        for (int c = 3; c <= 6; ++c) {
            setA(img, c, r, 128);
        }
        setA(img, 2, r, 200);
    }
    // Row 2: col 7 transparent → head scan breaks

    int result = PanelBackgroundScan::roundnessFromMaskCorner(img, true);
    // For the topleft branch: headLimitR walks r=6,5,4,3 while col 7 opaque → headLimitR=3;
    // tailLimitR walks r=6: col 2 alpha=200 ≠ 255 → tailLimitR=6; roundnessLines = 6-3+1 = 4.
    QCOMPARE(result, 4);
}

void PanelBackgroundScanTest::maskRoundness_mirrorEquivalence()
{
    // The two branches are supposed to be the same walk in opposite directions, so
    // rotating the input 180 degrees and flipping the corner flag must not move the
    // answer. maskRoundness_topLeft_mirrorsBottomRight hand-mirrors one image; this
    // asserts the property instead of a pair of pinned numbers.
    const QImage stepped = steppedMaskCorner();
    QCOMPARE(PanelBackgroundScan::roundnessFromMaskCorner(rotated180(stepped), true),
             PanelBackgroundScan::roundnessFromMaskCorner(stepped, false));

    // A fully transparent corner and a fully opaque one both bail out early; they are
    // here so a merge that broke the early exits could not hide behind the staircase.
    const QImage transparent = argb(8, 8);
    QCOMPARE(PanelBackgroundScan::roundnessFromMaskCorner(rotated180(transparent), true),
             PanelBackgroundScan::roundnessFromMaskCorner(transparent, false));

    QImage square = argb(8, 8);
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            setA(square, c, r, 255);
        }
    }
    QCOMPARE(PanelBackgroundScan::roundnessFromMaskCorner(rotated180(square), true),
             PanelBackgroundScan::roundnessFromMaskCorner(square, false));
}

void PanelBackgroundScanTest::maskRoundness_singleRow_bottomRight_findsNoRoundness()
{
    // A mask one row tall: the base line is found, and then there is no second row for
    // the roundness to be measured across, so the answer is no roundness at all.
    //
    // Note what this does and does not show. The row loops are never entered - the walk
    // rejects the first step off the only row - so nothing here proves the walk stops at
    // the right row in a taller image. What it does pin is the value: the guard against a
    // head and tail limit that never moved is the only thing keeping this at 0 rather than
    // reporting a phantom line of roundness. Under a sanitizer it also covers the bound
    // itself, since a walk that stepped anyway would read past the last scan line.
    //
    // Each direction needs its own image: the walk starts at the opposite corner and bails
    // out early unless the base pixel is opaque and the one across the diagonal is not, so
    // one image cannot get both directions as far as the row loop.
    QImage img = argb(8, 1);
    setA(img, 0, 0, 255);

    QCOMPARE(PanelBackgroundScan::roundnessFromMaskCorner(img, false), 0);
}

void PanelBackgroundScanTest::maskRoundness_singleRow_topLeft_findsNoRoundness()
{
    // The same image mirrored, for the walk that runs back towards (0,0).
    QImage img = argb(8, 1);
    setA(img, 7, 0, 255);

    QCOMPARE(PanelBackgroundScan::roundnessFromMaskCorner(img, true), 0);
}

// ---- roundnessFromShadowCorner ----

void PanelBackgroundScanTest::roundness_nullCorner_returns0()
{
    // A theme can hand the scanners a null image: PanelBackground::hasMask() probes
    // mask-topleft, but a Top/Left edge dock then asks for mask-bottomright, and
    // svg->image() of a missing element is null. Both scanners must answer "no
    // roundness" rather than walking a buffer that is not there.
    const QImage none;
    QCOMPARE(PanelBackgroundScan::roundnessFromMaskCorner(none, false), 0);
    QCOMPARE(PanelBackgroundScan::roundnessFromMaskCorner(none, true), 0);
    QCOMPARE(PanelBackgroundScan::roundnessFromShadowCorner(none, false), 0);
    QCOMPARE(PanelBackgroundScan::roundnessFromShadowCorner(none, true), 0);
}

void PanelBackgroundScanTest::shadowRoundness_emptyShadow_returns0()
{
    // All pixels transparent → basePoint alpha == 0 when at the expected "opaque" corner.
    // bottomright (topLeftCorner=false): baseRow=0, baseCol=0. basePoint alpha=0 but
    // baseShadowMaxOpacity stays 0 → baseLineLength stays 0 → returns 0.
    QImage img = argb(8, 8);
    QCOMPARE(PanelBackgroundScan::roundnessFromShadowCorner(img, false), 0);
}

void PanelBackgroundScanTest::shadowRoundness_zigZagCollapsesToZero()
{
    // The zig-zag reset (transPixels==baseLineLength → roundnessLines=0) is intended to
    // collapse spurious per-row increments when the shadow "wanders" back.
    //
    // topLeftCorner=true: baseRow=h-1=7, baseCol=w-1=7.
    // Baseline (row 7): col 7 transparent (basePoint alpha=0 so baseline scan runs).
    //   Scan c=7..0: place alpha=200 at col 3 → baseShadowMaxOpacity=200,
    //   baseLineLength = baseCol - 3 + 1 = 5.
    //   transPixels inner range: c=7 downto c=7-5+1=3 (inclusive).
    //
    // For zig-zag reset in row r: rowMaxOpacity must come from a pixel OUTSIDE the
    // transPixels scan range (c < 3), so all pixels in [7..3] have alpha < rowMaxOpacity
    // → transPixels increments for every pixel in range (5 times) → transPixels=5=baseLineLength → RESET.
    //
    // Row r=6: col 7 transparent (fpoint alpha=0 → outer loop continues).
    //   rowMaxOpacity scan c=7..0: place max alpha=200 at col 2 (outside range c<3).
    //   cols 3..7 all have lower alpha (e.g. 50).
    //   transPixels loop c=7..3: all alpha=50 ≠ rowMaxOpacity=200 → transPixels=5=baseLineLength → RESET.
    QImage img = argb(8, 8);

    // Row 7 (baseline): col 7 transparent, col 3 has alpha=200, cols 4..6 have alpha=50
    setA(img, 6, 7, 50);
    setA(img, 5, 7, 50);
    setA(img, 4, 7, 50);
    setA(img, 3, 7, 200); // → baseShadowMaxOpacity=200, baseLineLength=5
    // col 7 row 7 stays transparent (basePoint)

    // Row 6: col 7 transparent (outer fpoint continues), col 2 has max alpha, cols 3..6 have alpha=50
    setA(img, 6, 6, 50);
    setA(img, 5, 6, 50);
    setA(img, 4, 6, 50);
    setA(img, 3, 6, 50);
    setA(img, 2, 6, 200); // rowMaxOpacity=200, but col 2 is outside transPixels range [7..3]
    // col 7 row 6 stays transparent

    // Row 5: col 7 opaque → outer loop breaks → only row 6 was processed
    setA(img, 7, 5, 200);

    QCOMPARE(PanelBackgroundScan::roundnessFromShadowCorner(img, true), 0);
}

void PanelBackgroundScanTest::shadowRoundness_monotonicRamp_returnsLineCount()
{
    // bottomright (topLeftCorner=false): baseRow=0, baseCol=0.
    // Build a base row with increasing alpha so peak is at col 2 → baseLineLength=3.
    // Then each subsequent row: col 0 transparent, col 1 has max alpha, col 2 has lower alpha
    // → transPixels < baseLineLength → roundnessLines++.
    // Run 3 such rows before col 0 becomes opaque (breaking the outer loop).
    int result = PanelBackgroundScan::roundnessFromShadowCorner(shadowCornerMonotonicRamp(), false);
    // Instrument-first: run and pin. Expected 3 (one per row 1-3).
    QCOMPARE(result, 3);
}

void PanelBackgroundScanTest::shadowRoundness_mirrorEquivalence()
{
    // Same property as the mask mirror. The shadow scanner used to fail this: only the
    // bottomright walk re-measured baseLineLength from each row's own peak, so a row
    // peaking past the base line pulled the base line out with it.
    const QImage outlying = shadowCornerWithOutlyingRowPeak();
    QCOMPARE(PanelBackgroundScan::roundnessFromShadowCorner(rotated180(outlying), true),
             PanelBackgroundScan::roundnessFromShadowCorner(outlying, false));

    // The monotonic ramp keeps both peaks in one column, so it agreed all along; it is
    // here as the regression net for the shared walk.
    const QImage ramp = shadowCornerMonotonicRamp();
    QCOMPARE(PanelBackgroundScan::roundnessFromShadowCorner(rotated180(ramp), true),
             PanelBackgroundScan::roundnessFromShadowCorner(ramp, false));
}

void PanelBackgroundScanTest::shadowRoundness_perRowMaxBeyondBaseline_doesNotExtendBaseline()
{
    // The same fixture read without the mirror indirection: a row that peaks beyond the
    // base line reaches no further than the base line does, so it fails the roundness
    // test and the zig-zag reset drops what came before it. Rows 2-3 then count.
    //
    // Instrument-first: pinned from a run, not from tracing the loops.
    QCOMPARE(PanelBackgroundScan::roundnessFromShadowCorner(shadowCornerWithOutlyingRowPeak(), false), 2);
}

void PanelBackgroundScanTest::shadowRoundness_singleRow_bottomRight_findsNoRoundness()
{
    // Same shape of test for the shadow scanner. The base pixel has to be transparent and
    // some pixel further along the row solid, or the scan gives up before it reaches the
    // row loop at all and the image proves nothing about it.
    //
    // As above: with one row the loop cannot be entered, so this pins the answer and
    // covers the bound under a sanitizer rather than proving the walk stops correctly.
    QImage img = argb(8, 1);
    setA(img, 2, 0, 200);

    QCOMPARE(PanelBackgroundScan::roundnessFromShadowCorner(img, false), 0);
}

void PanelBackgroundScanTest::shadowRoundness_singleRow_topLeft_findsNoRoundness()
{
    // The same image mirrored, for the walk that runs back towards (0,0).
    QImage img = argb(8, 1);
    setA(img, 5, 0, 200);

    QCOMPARE(PanelBackgroundScan::roundnessFromShadowCorner(img, true), 0);
}

// ---- shadowFromBorder ----

void PanelBackgroundScanTest::roundness_mirrorEquivalence_overRandomCorners()
{
    // The hand-built fixtures above each pin one path. This sweeps the property itself
    // across small random corners, which is what actually catches a direction being
    // folded wrongly: the pre-fix shadow scanner broke this on roughly one image in
    // forty, and no fixture in this file happened to be one of them.
    //
    // Fixed seed so a failure is reproducible; alphas are biased towards 0 and 255 so
    // the transparency guards and the equality tests fire often.
    std::mt19937 rng(12345);
    const int palette[] = {0, 0, 0, 255, 255, 50, 100, 128, 200, 250};

    for (int iteration = 0; iteration < 4000; ++iteration) {
        const int w = 1 + static_cast<int>(rng() % 10);
        const int h = 1 + static_cast<int>(rng() % 10);

        QImage img = argb(w, h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                setA(img, x, y, palette[rng() % 10]);
            }
        }

        const QImage mirror = rotated180(img);
        const QString where = QStringLiteral("iteration %1, %2x%3").arg(iteration).arg(w).arg(h);

        QVERIFY2(PanelBackgroundScan::roundnessFromMaskCorner(img, false) == PanelBackgroundScan::roundnessFromMaskCorner(mirror, true),
                 qPrintable(QStringLiteral("mask corner disagrees with its mirror at %1").arg(where)));

        QVERIFY2(PanelBackgroundScan::roundnessFromShadowCorner(img, false) == PanelBackgroundScan::roundnessFromShadowCorner(mirror, true),
                 qPrintable(QStringLiteral("shadow corner disagrees with its mirror at %1").arg(where)));
    }
}

void PanelBackgroundScanTest::shadow_horizontalBand_sizeIsSpan()
{
    // horizontal=true: scan col 0 down all rows.
    // Opaque rows 3..9 (inclusive) → firstPixel=3, lastPixel=9 → size=7.
    QImage img = argb(4, 15);
    for (int r = 3; r <= 9; ++r) {
        setA(img, 0, r, 128);
    }
    auto s = PanelBackgroundScan::shadowFromBorder(img, true);
    QCOMPARE(s.discoveredSize, 7);
}

void PanelBackgroundScanTest::shadow_verticalBand_sizeIsSpan()
{
    // horizontal=false: scan row 0 across all columns.
    // Opaque cols 2..6 (inclusive) → firstPixel=2, lastPixel=6 → size=5.
    QImage img = argb(10, 4);
    for (int c = 2; c <= 6; ++c) {
        setA(img, c, 0, 128);
    }
    auto s = PanelBackgroundScan::shadowFromBorder(img, false);
    QCOMPARE(s.discoveredSize, 5);
}

void PanelBackgroundScanTest::shadow_color_picksMaxAlphaPixel()
{
    // One pixel with alpha=200 red, rest lower. The color scan should pick the red one.
    QImage img = argb(4, 4);
    setA(img, 1, 1, 100, qRgb(0, 255, 0)); // green, lower alpha
    setA(img, 2, 2, 200, qRgb(255, 0, 0)); // red, highest alpha
    setA(img, 3, 3, 50, qRgb(0, 0, 255));  // blue, lowest alpha

    auto s = PanelBackgroundScan::shadowFromBorder(img, true);
    QVERIFY2(s.color.isValid(), "color must be valid when opaque pixels exist");
    QCOMPARE(s.color.red(), 255);
    QCOMPARE(s.color.green(), 0);
    QCOMPARE(s.color.blue(), 0);
    QCOMPARE(s.color.alpha(), 200);
}

void PanelBackgroundScanTest::shadow_noOpaque_zeroAndInvalid()
{
    // All transparent → discoveredSize=0, color invalid.
    QImage img = argb(8, 8);
    auto s = PanelBackgroundScan::shadowFromBorder(img, true);
    QCOMPARE(s.discoveredSize, 0);
    QVERIFY2(!s.color.isValid(), "color must be invalid when no opaque pixel found");
}

QTEST_GUILESS_MAIN(PanelBackgroundScanTest)
#include "panelbackgroundscantest.moc"
