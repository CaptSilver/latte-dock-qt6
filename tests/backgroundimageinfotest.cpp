/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Direct-link unit tests for the pure image-analysis helpers extracted from
// BackgroundCache. They average pixel brightness over edge strips and flag a
// strip "busy" when its tiles disagree; here we feed fixture QImages with known
// content and assert the numbers, no ScreenPool or live desktop involved.

#include "plasma/extended/backgroundimageinfo.h"
#include "tools/commontools.h"

#include <QColor>
#include <QImage>
#include <QtTest>

using namespace Latte::PlasmaExtended;

class BackgroundImageInfoTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void brightnessFromArea_uniformEqualsColorBrightness();
    void brightnessFromArea_invalidImageIsSentinel();
    void brightnessFromArea_mixedIsAverage();

    void areaIsBusy_data();
    void areaIsBusy();

    void edgeHints_uniformImageNotBusy_data();
    void edgeHints_uniformImageNotBusy();
    void edgeHints_splitTopEdgeIsBusy();
    void edgeHints_invalidImageIsDefault();
};

void BackgroundImageInfoTest::brightnessFromArea_uniformEqualsColorBrightness()
{
    QImage img(20, 20, QImage::Format_ARGB32);
    img.fill(QColor(100, 150, 200));

    const float expected = Latte::colorBrightness(img.pixel(0, 0));
    QCOMPARE(BackgroundImageInfo::brightnessFromArea(img, 0, 0, 10, 10), expected);
}

void BackgroundImageInfoTest::brightnessFromArea_invalidImageIsSentinel()
{
    QImage invalid;
    QCOMPARE(BackgroundImageInfo::brightnessFromArea(invalid, 0, 0, 10, 10), -1000.0f);
}

void BackgroundImageInfoTest::brightnessFromArea_mixedIsAverage()
{
    // Left half black, right half white over a 4x4 area -> the average sits exactly
    // between the two brightness values.
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::black);
    for (int row = 0; row < 4; ++row) {
        img.setPixel(2, row, qRgb(255, 255, 255));
        img.setPixel(3, row, qRgb(255, 255, 255));
    }

    const float black = Latte::colorBrightness(qRgb(0, 0, 0));
    const float white = Latte::colorBrightness(qRgb(255, 255, 255));
    const float expected = (8 * black + 8 * white) / 16.0f;

    QCOMPARE(BackgroundImageInfo::brightnessFromArea(img, 0, 0, 4, 4), expected);
}

void BackgroundImageInfoTest::areaIsBusy_data()
{
    QTest::addColumn<float>("bright1");
    QTest::addColumn<float>("bright2");
    QTest::addColumn<bool>("busy");

    // Both tiles on the same side of the 123 threshold -> calm.
    QTest::newRow("both-light") << 200.0f << 210.0f << false;
    QTest::newRow("both-dark") << 10.0f << 50.0f << false;
    // Straddling the threshold -> busy.
    QTest::newRow("dark-vs-light") << 50.0f << 200.0f << true;
    // Out of the [0,255] range -> busy regardless of side.
    QTest::newRow("negative") << -5.0f << 100.0f << true;
    QTest::newRow("over-255") << 100.0f << 300.0f << true;
}

void BackgroundImageInfoTest::areaIsBusy()
{
    QFETCH(float, bright1);
    QFETCH(float, bright2);
    QFETCH(bool, busy);
    QCOMPARE(BackgroundImageInfo::areaIsBusy(bright1, bright2), busy);
}

void BackgroundImageInfoTest::edgeHints_uniformImageNotBusy_data()
{
    QTest::addColumn<int>("location");
    QTest::newRow("top") << int(Plasma::Types::TopEdge);
    QTest::newRow("bottom") << int(Plasma::Types::BottomEdge);
    QTest::newRow("left") << int(Plasma::Types::LeftEdge);
    QTest::newRow("right") << int(Plasma::Types::RightEdge);
}

void BackgroundImageInfoTest::edgeHints_uniformImageNotBusy()
{
    QFETCH(int, location);

    // A large square uniform image tiles cleanly along every edge; each tile shares
    // the same (int-truncated) brightness, so the strip is calm and the brightness
    // matches the fill colour.
    QImage img(200, 200, QImage::Format_ARGB32);
    img.fill(QColor(100, 150, 200));

    const BackgroundImageInfo::EdgeHints hints = BackgroundImageInfo::edgeHints(img, Plasma::Types::Location(location));

    QVERIFY(!hints.busy);
    // 140.75 truncated to 140 per tile.
    QCOMPARE(hints.brightness, 140.0f);
}

void BackgroundImageInfoTest::edgeHints_splitTopEdgeIsBusy()
{
    // Left half black, right half white: the top-edge tiles split into all-dark and
    // all-light groups, so min and max brightness straddle the threshold -> busy,
    // and the averaged brightness lands mid-range.
    QImage img(200, 60, QImage::Format_ARGB32);
    img.fill(Qt::black);
    for (int col = 100; col < 200; ++col) {
        for (int row = 0; row < 60; ++row) {
            img.setPixel(col, row, qRgb(255, 255, 255));
        }
    }

    const BackgroundImageInfo::EdgeHints hints = BackgroundImageInfo::edgeHints(img, Plasma::Types::TopEdge);

    QVERIFY(hints.busy);
    QVERIFY(hints.brightness > 100.0f);
    QVERIFY(hints.brightness < 155.0f);
}

void BackgroundImageInfoTest::edgeHints_invalidImageIsDefault()
{
    QImage invalid;
    const BackgroundImageInfo::EdgeHints hints = BackgroundImageInfo::edgeHints(invalid, Plasma::Types::TopEdge);
    QCOMPARE(hints.brightness, -1000.0f);
    QVERIFY(!hints.busy);
}

QTEST_MAIN(BackgroundImageInfoTest)
#include "backgroundimageinfotest.moc"
