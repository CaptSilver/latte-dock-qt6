/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// libplasma asks the corona about a containment's screen with the id the containment holds (unsigned
// from 6.8, int before), and a containment that was never placed holds PLASMA_UNPLACED_SCREEN
// (UINT_MAX, or -1 before 6.8). These tests drive the corona the way libplasma does, with that
// value and with real ids, and read what it tells libplasma back through the signals it emits:
// Latte screen ids go out as libplasma ids, "no screen" never goes out as a number a containment
// could match.

#include "../app/lattecorona.h"
#include "../app/plasmascreenid.h"
#include "../app/screenpool.h"
#include "coronafixture.h"

#include <KConfigGroup>
#include <Plasma/Applet>
#include <Plasma/Containment>

#include <QScreen>
#include <QSignalSpy>
#include <QtTest>

#include <climits>

using namespace Latte;

class CoronaScreenIdBoundaryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void neverPlacedContainmentGetsThePrimaryScreensGeometry();
    void unknownScreenIdGetsThePrimaryScreensGeometry();
    void neverPlacedContainmentHasNoAvailableArea();
    void placedContainmentGetsTheScreensGeometry();
    void screenCountIsTheNumberOfScreens();
    void availableAreaChangedIsSentForEveryKnownScreen();
    void screenAddedAnnouncesTheScreensId();

#if !LATTE_LIBPLASMA_6_8_API
    //! libplasma before 6.7.90 has no setScreen(): it asks Corona::screenForContainment each time
    //! Containment::screen() is read, so these are what a containment's screen is made of there
    void neverPlacedContainmentIsOnNoScreen();
    void containmentOutsideAnyLayoutFallsBackToItsLastScreen();
    void containmentOwnedByAnAppletIsOnTheScreenOfTheAppletsContainment();
#endif

private:
    CoronaSandbox m_sandbox;
    Latte::Corona *m_corona{nullptr};
};

void CoronaScreenIdBoundaryTest::initTestCase()
{
    QVERIFY(m_sandbox.arm());
    qputenv("XDG_DATA_HOME", m_sandbox.dir.path().toUtf8());

    m_corona = buildHeadlessCorona();
    QVERIFY(m_corona->screenPool() != nullptr);
    QVERIFY(m_corona->screenPool()->primaryScreen() != nullptr);
}

void CoronaScreenIdBoundaryTest::neverPlacedContainmentGetsThePrimaryScreensGeometry()
{
    //! plasmashell answers the primary screen for a containment that was never placed, and
    //! applets read this while their containment is still being restored
    const QRect expected = m_corona->screenPool()->primaryScreen()->geometry();
    QVERIFY(!expected.isNull());

    QCOMPARE(m_corona->screenGeometry(PLASMA_UNPLACED_SCREEN), expected);
}

void CoronaScreenIdBoundaryTest::unknownScreenIdGetsThePrimaryScreensGeometry()
{
    //! both kinds of unknown id get the same answer
    QCOMPARE(m_corona->screenGeometry(static_cast<PlasmaScreenId>(1000)), m_corona->screenGeometry(PLASMA_UNPLACED_SCREEN));
}

void CoronaScreenIdBoundaryTest::neverPlacedContainmentHasNoAvailableArea()
{
    QVERIFY(m_corona->availableScreenRect(PLASMA_UNPLACED_SCREEN).isNull());
    QVERIFY(m_corona->availableScreenRegion(PLASMA_UNPLACED_SCREEN).isEmpty());
}

void CoronaScreenIdBoundaryTest::placedContainmentGetsTheScreensGeometry()
{
    const int primaryId = m_corona->screenPool()->primaryScreenId();
    QVERIFY(primaryId >= 0);

    const QRect expected = m_corona->screenPool()->primaryScreen()->geometry();
    QVERIFY(!expected.isNull());

    QCOMPARE(m_corona->screenGeometry(static_cast<PlasmaScreenId>(primaryId)), expected);
    QVERIFY(!m_corona->availableScreenRect(static_cast<PlasmaScreenId>(primaryId)).isNull());
}

void CoronaScreenIdBoundaryTest::screenCountIsTheNumberOfScreens()
{
    QCOMPARE(m_corona->numScreens(), static_cast<PlasmaScreenId>(qGuiApp->screens().count()));
}

void CoronaScreenIdBoundaryTest::availableAreaChangedIsSentForEveryKnownScreen()
{
    QSignalSpy rectChanged(m_corona, &Plasma::Corona::availableScreenRectChanged);
    QSignalSpy regionChanged(m_corona, &Plasma::Corona::availableScreenRegionChanged);

    m_corona->emitAvailableScreenAreaChangedForAll();

    QList<PlasmaScreenId> expected;
    for (QScreen *screen : qGuiApp->screens()) {
        const int id = m_corona->screenPool()->id(screen->name());
        QVERIFY2(id >= 0, "the pool has no id for a connected screen");
        expected << static_cast<PlasmaScreenId>(id);
    }

    QVERIFY(!expected.isEmpty());
    QCOMPARE(rectChanged.count(), expected.count());
    QCOMPARE(regionChanged.count(), expected.count());

    for (int i = 0; i < expected.count(); ++i) {
        QCOMPARE(rectChanged.at(i).at(0).value<PlasmaScreenId>(), expected[i]);
        QCOMPARE(regionChanged.at(i).at(0).value<PlasmaScreenId>(), expected[i]);
        //! the old "all screens" -1 would arrive as this and match only never-placed containments
        QVERIFY(rectChanged.at(i).at(0).value<PlasmaScreenId>() != PLASMA_UNPLACED_SCREEN);
    }
}

void CoronaScreenIdBoundaryTest::screenAddedAnnouncesTheScreensId()
{
    QScreen *screen = m_corona->screenPool()->primaryScreen();
    const int id = m_corona->screenPool()->id(screen->name());
    QVERIFY(id >= 0);

    QSignalSpy added(m_corona, &Plasma::Corona::screenAdded);
    QSignalSpy rectChanged(m_corona, &Plasma::Corona::availableScreenRectChanged);

    QVERIFY(QMetaObject::invokeMethod(m_corona, "onScreenAdded", Q_ARG(QScreen *, screen)));

    QCOMPARE(added.count(), 1);
    QCOMPARE(added.first().at(0).value<PlasmaScreenId>(), static_cast<PlasmaScreenId>(id));
    QCOMPARE(rectChanged.count(), 1);
    QCOMPARE(rectChanged.first().at(0).value<PlasmaScreenId>(), static_cast<PlasmaScreenId>(id));
}

#if !LATTE_LIBPLASMA_6_8_API
void CoronaScreenIdBoundaryTest::neverPlacedContainmentIsOnNoScreen()
{
    Plasma::Containment *containment = m_corona->createContainment(QStringLiteral("null"));
    QVERIFY(containment);

    QCOMPARE(m_corona->screenForContainment(containment), NOSCREEN);
    QCOMPARE(latteScreenId(containment->screen()), NOSCREEN);
}

void CoronaScreenIdBoundaryTest::containmentOutsideAnyLayoutFallsBackToItsLastScreen()
{
    Plasma::Containment *containment = m_corona->createContainment(QStringLiteral("null"));
    QVERIFY(containment);

    containment->config().writeEntry(QStringLiteral("lastScreen"), 4);
    KConfigGroup group = containment->config();
    containment->restore(group);
    QCOMPARE(containment->lastScreen(), 4);

    QCOMPARE(m_corona->screenForContainment(containment), 4);
    QCOMPARE(containment->screen(), 4);
}

void CoronaScreenIdBoundaryTest::containmentOwnedByAnAppletIsOnTheScreenOfTheAppletsContainment()
{
    //! a system tray is a containment owned by an applet of the dock's containment
    Plasma::Containment *host = m_corona->createContainment(QStringLiteral("null"));
    QVERIFY(host);

    host->config().writeEntry(QStringLiteral("lastScreen"), 5);
    KConfigGroup group = host->config();
    host->restore(group);

    auto *applet = new Plasma::Applet(host, KPluginMetaData(), QVariantList());
    auto *tray = new Plasma::Containment(applet, KPluginMetaData(), QVariantList());

    QCOMPARE(m_corona->screenForContainment(tray), 5);
}
#endif

QTEST_MAIN(CoronaScreenIdBoundaryTest)

#include "coronascreenidboundarytest.moc"
