/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// libplasma 6.8 carries screen ids as unsigned and no longer accepts -1. Latte keeps
// signed ids (ScreenPool::NOSCREENID is -1, real ids start at FIRSTSCREENID). The
// helpers in plasmascreenid.h are the only place the two domains meet, so they are
// tested at the edges: the unplaced sentinel libplasma stores, the Latte "no screen"
// value, and what a real Containment ends up holding.

#include "plasmascreenid.h"

#include <QGuiApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <climits>

#include <KConfig>
#include <KConfigGroup>
#include <Plasma/Containment>
#include <Plasma/Corona>

using namespace Latte;

class TestCorona : public Plasma::Corona
{
public:
    using Plasma::Corona::Corona;
    QRect screenGeometry(uint) const override { return QRect(0, 0, 1920, 1080); }
};

//! Throwaway config home, armed in main() before QGuiApplication exists.
static QTemporaryDir s_xdgConfig;

class PlasmaScreenIdTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void latteScreenId_keepsValidIds();
    void latteScreenId_unplacedSentinelIsNoScreen();
    void latteScreenId_valuesAboveIntMaxAreNoScreen();
    void plasmaScreenId_keepsValidIds();
    void plasmaScreenId_noScreenHasNoPlasmaId();
    void roundTrip();
    void storedScreenId_keepsValidIds();
    void storedScreenId_libplasmaUnplacedValueIsNoScreen();
    void storedScreenId_negativeValuesAreNoScreen();
    void storedScreenId_readsWhatLibplasmaWrites();
    void setContainmentScreen_storesTheId();
    void setContainmentScreen_ignoresNoScreen();
};

void PlasmaScreenIdTest::latteScreenId_keepsValidIds()
{
    QCOMPARE(latteScreenId(0u), 0);
    QCOMPARE(latteScreenId(10u), 10);
    QCOMPARE(latteScreenId(static_cast<uint>(INT_MAX)), INT_MAX);
}

void PlasmaScreenIdTest::latteScreenId_unplacedSentinelIsNoScreen()
{
    //! createContainment() and loadLayout() hand the containment UINT_MAX, the -1 they pass
    //! into the unsigned screen parameter.
    QCOMPARE(latteScreenId(UINT_MAX), NOSCREEN);
    QCOMPARE(NOSCREEN, -1);
}

void PlasmaScreenIdTest::latteScreenId_valuesAboveIntMaxAreNoScreen()
{
    QCOMPARE(latteScreenId(static_cast<uint>(INT_MAX) + 1u), NOSCREEN);
    QCOMPARE(latteScreenId(4000000000u), NOSCREEN);
}

void PlasmaScreenIdTest::plasmaScreenId_keepsValidIds()
{
    QVERIFY(plasmaScreenId(0).has_value());
    QCOMPARE(*plasmaScreenId(0), 0u);
    QCOMPARE(*plasmaScreenId(10), 10u);
    QCOMPARE(*plasmaScreenId(INT_MAX), static_cast<uint>(INT_MAX));
}

void PlasmaScreenIdTest::plasmaScreenId_noScreenHasNoPlasmaId()
{
    //! -1 must not wrap into UINT_MAX: that would look like a placed screen to libplasma.
    QVERIFY(!plasmaScreenId(-1).has_value());
    QVERIFY(!plasmaScreenId(NOSCREEN).has_value());
    QVERIFY(!plasmaScreenId(INT_MIN).has_value());
}

void PlasmaScreenIdTest::roundTrip()
{
    for (int id : {0, 1, 10, 11, 12345, INT_MAX}) {
        QCOMPARE(latteScreenId(*plasmaScreenId(id)), id);
    }
}

void PlasmaScreenIdTest::storedScreenId_keepsValidIds()
{
    QCOMPARE(storedScreenId(0), 0);
    QCOMPARE(storedScreenId(10), 10);
    QCOMPARE(storedScreenId(INT_MAX), INT_MAX);
}

void PlasmaScreenIdTest::storedScreenId_libplasmaUnplacedValueIsNoScreen()
{
    //! 6.8 writes the unsigned value of -1 for a containment that was never placed
    QCOMPARE(storedScreenId(4294967295LL), NOSCREEN);
    QCOMPARE(storedScreenId(static_cast<qlonglong>(INT_MAX) + 1), NOSCREEN);
}

void PlasmaScreenIdTest::storedScreenId_negativeValuesAreNoScreen()
{
    //! what Latte itself has always written
    QCOMPARE(storedScreenId(-1), NOSCREEN);
    QCOMPARE(storedScreenId(-12), NOSCREEN);
}

void PlasmaScreenIdTest::storedScreenId_readsWhatLibplasmaWrites()
{
    //! The value goes through KConfig as text. Read as qlonglong, "4294967295" arrives intact
    //! to be normalised instead of relying on the int conversion wrapping it.
    KConfig config(s_xdgConfig.filePath(QStringLiteral("storedscreenid.rc")));
    KConfigGroup group = config.group(QStringLiteral("Containments")).group(QStringLiteral("12"));

    group.writeEntry(QStringLiteral("lastScreen"), 4294967295u);
    QCOMPARE(storedScreenId(group.readEntry(QStringLiteral("lastScreen"), qlonglong(NOSCREEN))), NOSCREEN);

    group.writeEntry(QStringLiteral("lastScreen"), -1);
    QCOMPARE(storedScreenId(group.readEntry(QStringLiteral("lastScreen"), qlonglong(NOSCREEN))), NOSCREEN);

    group.writeEntry(QStringLiteral("lastScreen"), 11);
    QCOMPARE(storedScreenId(group.readEntry(QStringLiteral("lastScreen"), qlonglong(NOSCREEN))), 11);
}

void PlasmaScreenIdTest::setContainmentScreen_storesTheId()
{
    QVERIFY2(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation).startsWith(s_xdgConfig.path()),
             "config home escaped the sandbox");

    auto *corona = new TestCorona();
    Plasma::Containment *cont = corona->createContainment(QStringLiteral("null"));
    QVERIFY(cont);
    QCOMPARE(latteScreenId(cont->screen()), NOSCREEN);

    setContainmentScreen(cont, 11);
    QCOMPARE(cont->screen(), 11u);
    QCOMPARE(latteScreenId(cont->screen()), 11);
    QCOMPARE(cont->config().readEntry(QStringLiteral("lastScreen"), -1), 11);
}

void PlasmaScreenIdTest::setContainmentScreen_ignoresNoScreen()
{
    auto *corona = new TestCorona();
    Plasma::Containment *cont = corona->createContainment(QStringLiteral("null"));
    QVERIFY(cont);

    setContainmentScreen(cont, 12);
    setContainmentScreen(cont, NOSCREEN);

    QCOMPARE(cont->screen(), 12u);
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // The real Plasma::Corona writes its applets rc under the config home.
    qputenv("XDG_CONFIG_HOME", s_xdgConfig.path().toUtf8());
    qputenv("XDG_DATA_HOME", s_xdgConfig.path().toUtf8());
    QGuiApplication app(argc, argv);
    PlasmaScreenIdTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "plasmascreenidtest.moc"
