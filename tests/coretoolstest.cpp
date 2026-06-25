/*
    SPDX-FileCopyrightText: 2026 Latte Qt6 port
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Production helpers under declarativeimports/core: the colour math in tools.cpp,
// the version/duration constants in environment.cpp, and the inline string/geometry
// helpers in extras.h. All engine-free; the QML singleton providers are not exercised.

#include "tools.h"
#include "environment.h"
#include "extras.h"

// Plasma
#include <Plasma/Plasma>
#include <Plasma/plasma_version.h>

// Qt
#include <QColor>
#include <QRect>
#include <QString>
#include <QtTest>

class CoreToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void colorBrightness_matchesWeightedAverage();
    void colorBrightness_blackAndWhite();
    void colorLumina_blackAndWhite();
    void colorLumina_monotonicWithGrey();

    void environment_constants();
    void environment_makeVersion();
    void environment_frameworksVersionMatchesPlasma();

    void extras_qRectToStr();
    void extras_qEnumToStrLocation();
    void extras_almostEqual();
};

void CoreToolsTest::colorBrightness_matchesWeightedAverage()
{
    Latte::Tools tools;

    // brightness = (r*299 + g*587 + b*114) / 1000 over the 0..255 channels.
    // (255*299 + 128*587 + 64*114) / 1000 = 158677 / 1000 = 158.677
    const float expected = (255.0f * 299 + 128 * 587 + 64 * 114) / 1000;
    QCOMPARE(tools.colorBrightness(QColor(255, 128, 64)), expected);
    QVERIFY(qAbs(tools.colorBrightness(QColor(255, 128, 64)) - 158.677f) < 0.001f);
}

void CoreToolsTest::colorBrightness_blackAndWhite()
{
    Latte::Tools tools;

    QCOMPARE(tools.colorBrightness(QColor(0, 0, 0)), 0.0f);

    // (255*299 + 255*587 + 255*114) / 1000 = 255 * 1000 / 1000 = 255
    QCOMPARE(tools.colorBrightness(QColor(255, 255, 255)), 255.0f);
}

void CoreToolsTest::colorLumina_blackAndWhite()
{
    Latte::Tools tools;

    // Pure black has zero relative luminance; pure white sits at 1.0 (WCAG 2.0).
    QCOMPARE(tools.colorLumina(QColor(0, 0, 0)), 0.0f);
    QVERIFY(qAbs(tools.colorLumina(QColor(255, 255, 255)) - 1.0f) < 0.0001f);
}

void CoreToolsTest::colorLumina_monotonicWithGrey()
{
    Latte::Tools tools;

    // Luminance rises monotonically from black through mid-grey to white.
    const float dark = tools.colorLumina(QColor(64, 64, 64));
    const float mid = tools.colorLumina(QColor(128, 128, 128));
    const float light = tools.colorLumina(QColor(200, 200, 200));

    QVERIFY(dark < mid);
    QVERIFY(mid < light);
    QVERIFY(dark >= 0.0f);
    QVERIFY(light <= 1.0f);
}

void CoreToolsTest::environment_constants()
{
    Latte::Environment env;

    QCOMPARE(env.separatorLength(), Latte::Environment::SeparatorLength);
    QCOMPARE(env.separatorLength(), 5);

    QCOMPARE(env.shortDuration(), 40u);
    QCOMPARE(env.longDuration(), 240u);
    QVERIFY(env.shortDuration() < env.longDuration());
}

void CoreToolsTest::environment_makeVersion()
{
    Latte::Environment env;

    // (major << 16) | (minor << 8) | release
    QCOMPARE(env.makeVersion(0, 0, 0), 0u);
    QCOMPARE(env.makeVersion(1, 0, 0), 0x10000u);
    QCOMPARE(env.makeVersion(0, 1, 0), 0x100u);
    QCOMPARE(env.makeVersion(0, 0, 1), 0x1u);

    // 5.27.11 -> (5<<16)|(27<<8)|11 = 327680 + 6912 + 11 = 334603
    QCOMPARE(env.makeVersion(5, 27, 11), 334603u);
}

void CoreToolsTest::environment_frameworksVersionMatchesPlasma()
{
    Latte::Environment env;

    // frameworksVersion() returns the compiled-in libplasma version verbatim.
    QCOMPARE(env.frameworksVersion(), static_cast<uint>(PLASMA_VERSION));
}

void CoreToolsTest::extras_qRectToStr()
{
    // Format is "(<x>, <y>) <width>x<height>".
    QCOMPARE(qRectToStr(QRect(1, 2, 3, 4)), QStringLiteral("(1, 2) 3x4"));
    QCOMPARE(qRectToStr(QRect(0, 0, 0, 0)), QStringLiteral("(0, 0) 0x0"));
    QCOMPARE(qRectToStr(QRect(-5, -6, 100, 200)), QStringLiteral("(-5, -6) 100x200"));
}

void CoreToolsTest::extras_qEnumToStrLocation()
{
    // The Plasma::Types::Location overload resolves the key name via the static metaobject.
    QCOMPARE(QString::fromLatin1(qEnumToStr(Plasma::Types::BottomEdge)),
             QStringLiteral("BottomEdge"));
    QCOMPARE(QString::fromLatin1(qEnumToStr(Plasma::Types::TopEdge)),
             QStringLiteral("TopEdge"));
    QCOMPARE(QString::fromLatin1(qEnumToStr(Plasma::Types::Floating)),
             QStringLiteral("Floating"));
}

void CoreToolsTest::extras_almostEqual()
{
    // Floating-point rounding noise is within tolerance; clearly different values are not.
    QVERIFY(almost_equal(0.1 + 0.2, 0.3, 2));
    QVERIFY(almost_equal(1.0f, 1.0f, 1));
    QVERIFY(!almost_equal(0.3, 0.30001, 2));
    QVERIFY(almost_equal(0.0, 0.0, 1));
}

QTEST_GUILESS_MAIN(CoreToolsTest)

#include "coretoolstest.moc"
