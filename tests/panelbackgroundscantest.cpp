/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Tests for the pure pixel-scanning math extracted from PanelBackground.
// Each function is tested with a small hand-crafted QImage; expected values for
// non-trivial loops are pinned from an instrument-first run, not computed by hand.

#include "panelbackgroundscan.h"

#include <QtTest>

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
    void maskRoundness_singleRowTall_noCrash();

    // ---- roundnessFromShadowCorner ----

    void shadowRoundness_emptyShadow_returns0();
    void shadowRoundness_zigZagCollapsesToZero();
    void shadowRoundness_monotonicRamp_returnsLineCount();
    void shadowRoundness_singleRowTall_noCrash();

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
    QImage img = argb(8, 8);

    // Row 0 (baseRow): cols 0..5 opaque
    for (int c = 0; c <= 5; ++c) {
        setA(img, c, 0, 255);
    }
    // Rows 1..4: col 0 opaque (keeps the head-scan going), col 5 = 200 (not 255 → triggers tailLimitR), col 1..4 = 128
    for (int r = 1; r <= 4; ++r) {
        setA(img, 0, r, 255);
        for (int c = 1; c <= 4; ++c) {
            setA(img, c, r, 128);
        }
        setA(img, 5, r, 200);
    }
    // Row 5: col 0 transparent → outer loop breaks immediately (head scan stops here)
    // Rows 5..7: transparent

    int result = PanelBackgroundScan::roundnessFromMaskCorner(img, false);
    // For the staircase above: headLimitR=1, tailLimitR=4 → 4-1+1=4.
    QCOMPARE(result, 4);
}

void PanelBackgroundScanTest::maskRoundness_topLeft_mirrorsBottomRight()
{
    // topleft (topLeftCorner=true): baseRow=h-1=7, baseCol=w-1=7.
    // isRoundedPoint is img(0,0) → keep transparent so roundness check proceeds.
    // Base row (r=7): col 7 opaque (basePoint alpha>0) → baseLineLength scan from col 7 down.
    // Make cols 2..7 opaque in row 7 → baseLineLength=6.
    // Rows r=6..3: col 7 opaque (head scan continues); col qMax(0,8-6)=2 NOT 255 → sets tailLimitR.
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

void PanelBackgroundScanTest::maskRoundness_singleRowTall_noCrash()
{
    // 8x1 image — both topLeftCorner=false and topLeftCorner=true must not over-read.
    QImage img = argb(8, 1);
    setA(img, 0, 0, 255);
    // Should complete without crash and return >= 0.
    int r1 = PanelBackgroundScan::roundnessFromMaskCorner(img, false);
    int r2 = PanelBackgroundScan::roundnessFromMaskCorner(img, true);
    QVERIFY(r1 >= 0);
    QVERIFY(r2 >= 0);
}

// ---- roundnessFromShadowCorner ----

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
    // Use the TOPLEFT branch where baseLineLength is fixed (not mutated per row).
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
    QImage img = argb(8, 8);
    // Row 0 (baseline): col 0 transparent, col 1=100, col 2=200 → peak at col 2, baseLineLength=3
    setA(img, 1, 0, 100);
    setA(img, 2, 0, 200);

    // Rows 1..3: col 0 transparent, col 2=200 (rowMaxOpacity), col 1=100 (not max) →
    // inner scan: transPixels counts non-max pixels before hitting max < baseLineLength → roundnessLines++
    for (int r = 1; r <= 3; ++r) {
        setA(img, 1, r, 100);
        setA(img, 2, r, 200);
        // col 0 stays transparent
    }
    // Row 4: col 0 opaque → outer loop breaks
    setA(img, 0, 4, 255);

    int result = PanelBackgroundScan::roundnessFromShadowCorner(img, false);
    // Instrument-first: run and pin. Expected 3 (one per row 1-3).
    QCOMPARE(result, 3);
}

void PanelBackgroundScanTest::shadowRoundness_singleRowTall_noCrash()
{
    // 8x1 image — both branches must not read scanLine(1).
    QImage img = argb(8, 1);
    int r1 = PanelBackgroundScan::roundnessFromShadowCorner(img, false);
    int r2 = PanelBackgroundScan::roundnessFromShadowCorner(img, true);
    QVERIFY(r1 >= 0);
    QVERIFY(r2 >= 0);
}

// ---- shadowFromBorder ----

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
