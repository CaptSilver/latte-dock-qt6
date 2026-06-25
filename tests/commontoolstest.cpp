/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../app/tools/commontools.h"

#include <QColor>
#include <QDir>
#include <QRect>
#include <QStandardPaths>
#include <QString>
#include <QtTest>

class CommonToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void brightnessBlackIsZero();
    void brightnessWhiteIsFull();
    void brightnessWeightedChannels();
    void brightnessOverloadsAgree();

    void luminaBlackIsZero();
    void luminaWhiteIsOne();
    void luminaOverloadsAgree();
    void luminaMonotonic();

    void rectToStringKnownValue();
    void rectRoundTrip_data();
    void rectRoundTrip();

    void configPathNonEmpty();
};

void CommonToolsTest::brightnessBlackIsZero()
{
    QCOMPARE(Latte::colorBrightness(QColor(Qt::black)), 0.0f);
}

void CommonToolsTest::brightnessWhiteIsFull()
{
    // (255*299 + 255*587 + 255*114) / 1000 == 255
    QCOMPARE(Latte::colorBrightness(QColor(Qt::white)), 255.0f);
}

void CommonToolsTest::brightnessWeightedChannels()
{
    // Per-channel weights: green dominates, blue is lightest. Hand-computed:
    // red   255 -> 255*299/1000 = 76.245
    // green 255 -> 255*587/1000 = 149.685
    // blue  255 -> 255*114/1000 = 29.07
    QCOMPARE(Latte::colorBrightness(QColor(255, 0, 0)), 76.245f);
    QCOMPARE(Latte::colorBrightness(QColor(0, 255, 0)), 149.685f);
    QCOMPARE(Latte::colorBrightness(QColor(0, 0, 255)), 29.07f);
}

void CommonToolsTest::brightnessOverloadsAgree()
{
    QColor c(123, 45, 200);
    float fromColor = Latte::colorBrightness(c);
    float fromRgb = Latte::colorBrightness(c.rgb());
    float fromFloats = Latte::colorBrightness(123.0f, 45.0f, 200.0f);
    QCOMPARE(fromColor, fromFloats);
    QCOMPARE(fromRgb, fromFloats);
}

void CommonToolsTest::luminaBlackIsZero()
{
    QCOMPARE(Latte::colorLumina(QColor(Qt::black)), 0.0f);
}

void CommonToolsTest::luminaWhiteIsOne()
{
    // WCAG relative luminance of pure white is 1.0 (weights sum to 1).
    QCOMPARE(Latte::colorLumina(QColor(Qt::white)), 1.0f);
}

void CommonToolsTest::luminaOverloadsAgree()
{
    QColor c(64, 128, 192);
    float fromColor = Latte::colorLumina(c);
    float fromRgb = Latte::colorLumina(c.rgb());
    // QColor::redF() path and the QRgb->/255 path should land on the same value.
    QVERIFY(qAbs(fromColor - fromRgb) < 1e-5f);
}

void CommonToolsTest::luminaMonotonic()
{
    // Brighter grey must have higher luminance, and the result stays in [0,1].
    float dark = Latte::colorLumina(QColor(32, 32, 32));
    float mid = Latte::colorLumina(QColor(128, 128, 128));
    float light = Latte::colorLumina(QColor(220, 220, 220));
    QVERIFY(dark >= 0.0f);
    QVERIFY(dark < mid);
    QVERIFY(mid < light);
    QVERIFY(light <= 1.0f);
}

void CommonToolsTest::rectToStringKnownValue()
{
    QCOMPARE(Latte::rectToString(QRect(10, 20, 30, 40)), QStringLiteral("10,20 30x40"));
    QCOMPARE(Latte::rectToString(QRect(-5, -7, 100, 200)), QStringLiteral("-5,-7 100x200"));
}

void CommonToolsTest::rectRoundTrip_data()
{
    QTest::addColumn<QRect>("rect");
    QTest::newRow("origin") << QRect(0, 0, 0, 0);
    QTest::newRow("typical") << QRect(10, 20, 30, 40);
    QTest::newRow("negative-pos") << QRect(-100, -200, 1920, 1080);
    QTest::newRow("large") << QRect(3840, 2160, 7680, 4320);
}

void CommonToolsTest::rectRoundTrip()
{
    QFETCH(QRect, rect);
    QRect back = Latte::stringToRect(Latte::rectToString(rect));
    QCOMPARE(back, rect);
}

void CommonToolsTest::configPathNonEmpty()
{
    // With a temp HOME and no XDG_CONFIG_HOME, QStandardPaths still yields a
    // config location, so configPath() returns it (never the homePath fallback
    // unless the location list is empty).
    QString path = Latte::configPath();
    QVERIFY(!path.isEmpty());
}

QTEST_GUILESS_MAIN(CommonToolsTest)

#include "commontoolstest.moc"
