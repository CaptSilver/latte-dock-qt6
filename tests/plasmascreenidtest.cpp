/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// libplasma 6.8 carries screen ids as unsigned and no longer accepts -1; older libplasma
// uses plain ints. Latte keeps signed ids (ScreenPool::NOSCREENID is -1, real ids start at
// FIRSTSCREENID). The helpers in plasmascreenid.h are the only place the two domains meet,
// so they are tested at the edges: the unplaced sentinel libplasma stores, the Latte "no
// screen" value, and what a real Containment ends up holding.

#include "plasmascreenid.h"

#include <QGuiApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <climits>
#include <type_traits>

#include <KConfig>
#include <KConfigGroup>
#include <Plasma/Containment>
#include <Plasma/Corona>

using namespace Latte;

class TestCorona : public Plasma::Corona
{
public:
    using Plasma::Corona::Corona;
    QRect screenGeometry(PlasmaScreenId) const override { return QRect(0, 0, 1920, 1080); }

#if !LATTE_LIBPLASMA_6_8_API
    //! older libplasma has no setScreen(): the containment's screen is whatever this says
    int screenForContainment(const Plasma::Containment *) const override { return m_answer; }
    int m_answer{-1};
#endif

    //! puts the containment on the screen the way the installed libplasma allows
    void place(Plasma::Containment *containment, int latteId)
    {
#if !LATTE_LIBPLASMA_6_8_API
        m_answer = latteId;
#endif
        setContainmentScreen(containment, latteId);
    }
};

//! Throwaway config home, armed in main() before QGuiApplication exists.
static QTemporaryDir s_xdgConfig;

class PlasmaScreenIdTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void screenIdAliasMatchesWhatContainmentReturns();
    void latteScreenId_keepsValidIds();
    void latteScreenId_unplacedSentinelIsNoScreen();
    void latteScreenId_idsOutsideNonNegativeIntAreNoScreen();
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

void PlasmaScreenIdTest::screenIdAliasMatchesWhatContainmentReturns()
{
    static_assert(std::is_same_v<PlasmaScreenId, decltype(std::declval<const Plasma::Containment &>().screen())>);
    static_assert(std::is_same_v<PlasmaScreenId, decltype(std::declval<const Plasma::Corona &>().numScreens())>);

#if LATTE_LIBPLASMA_6_8_API
    static_assert(std::is_same_v<PlasmaScreenId, uint>);
#else
    static_assert(std::is_same_v<PlasmaScreenId, int>);
#endif
}

void PlasmaScreenIdTest::latteScreenId_keepsValidIds()
{
    QCOMPARE(latteScreenId(0), 0);
    QCOMPARE(latteScreenId(10), 10);
    QCOMPARE(latteScreenId(static_cast<PlasmaScreenId>(INT_MAX)), INT_MAX);
}

void PlasmaScreenIdTest::latteScreenId_unplacedSentinelIsNoScreen()
{
    //! createContainment() and loadLayout() hand the containment -1, which is UINT_MAX when
    //! ids are unsigned
    QCOMPARE(PLASMA_UNPLACED_SCREEN, static_cast<PlasmaScreenId>(-1));
    QCOMPARE(latteScreenId(PLASMA_UNPLACED_SCREEN), NOSCREEN);
    QCOMPARE(NOSCREEN, -1);
}

void PlasmaScreenIdTest::latteScreenId_idsOutsideNonNegativeIntAreNoScreen()
{
#if LATTE_LIBPLASMA_6_8_API
    QCOMPARE(latteScreenId(static_cast<uint>(INT_MAX) + 1u), NOSCREEN);
    QCOMPARE(latteScreenId(4000000000u), NOSCREEN);
#else
    //! signed ids cannot exceed INT_MAX; any negative one is no screen
    QCOMPARE(latteScreenId(INT_MIN), NOSCREEN);
    QCOMPARE(latteScreenId(-12), NOSCREEN);
#endif
}

void PlasmaScreenIdTest::plasmaScreenId_keepsValidIds()
{
    QVERIFY(plasmaScreenId(0).has_value());
    QCOMPARE(*plasmaScreenId(0), static_cast<PlasmaScreenId>(0));
    QCOMPARE(*plasmaScreenId(10), static_cast<PlasmaScreenId>(10));
    QCOMPARE(*plasmaScreenId(INT_MAX), static_cast<PlasmaScreenId>(INT_MAX));
}

void PlasmaScreenIdTest::plasmaScreenId_noScreenHasNoPlasmaId()
{
    //! -1 must not turn into libplasma's unplaced id (UINT_MAX when ids are unsigned) or any
    //! other id: that would look like a placed screen to libplasma.
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

//! libplasma 6.8 stores what setContainmentScreen() is given. Older libplasma has nothing to
//! store: the containment asks the corona, and setContainmentScreen() makes it ask again, so
//! the test corona's answer has to reach screen() and the saved lastScreen.
void PlasmaScreenIdTest::setContainmentScreen_storesTheId()
{
    QVERIFY2(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation).startsWith(s_xdgConfig.path()),
             "config home escaped the sandbox");

    auto *corona = new TestCorona();
    Plasma::Containment *cont = corona->createContainment(QStringLiteral("null"));
    QVERIFY(cont);
    QCOMPARE(latteScreenId(cont->screen()), NOSCREEN);
    QCOMPARE(cont->config().readEntry(QStringLiteral("lastScreen"), -1), -1);

    QSignalSpy spy(cont, &Plasma::Containment::screenChanged);
    corona->place(cont, 11);
    QCOMPARE(cont->screen(), static_cast<PlasmaScreenId>(11));
    QCOMPARE(latteScreenId(cont->screen()), 11);
    QCOMPARE(cont->config().readEntry(QStringLiteral("lastScreen"), -1), 11);
    QVERIFY(spy.count() >= 1);
    QCOMPARE(spy.last().at(0).value<PlasmaScreenId>(), static_cast<PlasmaScreenId>(11));
}

void PlasmaScreenIdTest::setContainmentScreen_ignoresNoScreen()
{
    auto *corona = new TestCorona();
    Plasma::Containment *cont = corona->createContainment(QStringLiteral("null"));
    QVERIFY(cont);

    corona->place(cont, 12);
    setContainmentScreen(cont, NOSCREEN);

    //! Older libplasma asks the corona, so screen() alone can't tell a bridge that obeys
    //! NOSCREEN from one that ignores it there. The stored lastScreen can: it is what
    //! Plasma restores the containment to, and "no screen" must not wipe it.
    QCOMPARE(cont->screen(), static_cast<PlasmaScreenId>(12));
    QCOMPARE(cont->config().readEntry(QStringLiteral("lastScreen"), -1), 12);
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
