/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// libplasma 6.8 asks the corona about a containment's screen with the unsigned id the containment
// holds, and a containment that was never placed holds UINT_MAX. These tests drive the corona the
// way libplasma does, with that value and with real ids, and read what it tells libplasma back
// through the signals it emits: Latte screen ids go out as unsigned, "no screen" never goes out
// as a number a containment could match.

#include "../app/lattecorona.h"
#include "../app/plasmascreenid.h"
#include "../app/screenpool.h"
#include "coronafixture.h"

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

    QCOMPARE(m_corona->screenGeometry(UINT_MAX), expected);
}

void CoronaScreenIdBoundaryTest::unknownScreenIdGetsThePrimaryScreensGeometry()
{
    //! both kinds of unknown id get the same answer
    QCOMPARE(m_corona->screenGeometry(1000u), m_corona->screenGeometry(UINT_MAX));
}

void CoronaScreenIdBoundaryTest::neverPlacedContainmentHasNoAvailableArea()
{
    QVERIFY(m_corona->availableScreenRect(UINT_MAX).isNull());
    QVERIFY(m_corona->availableScreenRegion(UINT_MAX).isEmpty());
}

void CoronaScreenIdBoundaryTest::placedContainmentGetsTheScreensGeometry()
{
    const int primaryId = m_corona->screenPool()->primaryScreenId();
    QVERIFY(primaryId >= 0);

    const QRect expected = m_corona->screenPool()->primaryScreen()->geometry();
    QVERIFY(!expected.isNull());

    QCOMPARE(m_corona->screenGeometry(static_cast<uint>(primaryId)), expected);
    QVERIFY(!m_corona->availableScreenRect(static_cast<uint>(primaryId)).isNull());
}

void CoronaScreenIdBoundaryTest::screenCountIsTheNumberOfScreens()
{
    QCOMPARE(m_corona->numScreens(), static_cast<uint>(qGuiApp->screens().count()));
}

void CoronaScreenIdBoundaryTest::availableAreaChangedIsSentForEveryKnownScreen()
{
    QSignalSpy rectChanged(m_corona, &Plasma::Corona::availableScreenRectChanged);
    QSignalSpy regionChanged(m_corona, &Plasma::Corona::availableScreenRegionChanged);

    m_corona->emitAvailableScreenAreaChangedForAll();

    QList<uint> expected;
    for (QScreen *screen : qGuiApp->screens()) {
        const int id = m_corona->screenPool()->id(screen->name());
        QVERIFY2(id >= 0, "the pool has no id for a connected screen");
        expected << static_cast<uint>(id);
    }

    QVERIFY(!expected.isEmpty());
    QCOMPARE(rectChanged.count(), expected.count());
    QCOMPARE(regionChanged.count(), expected.count());

    for (int i = 0; i < expected.count(); ++i) {
        QCOMPARE(rectChanged.at(i).at(0).toUInt(), expected[i]);
        QCOMPARE(regionChanged.at(i).at(0).toUInt(), expected[i]);
        //! the old "all screens" -1 would arrive as this and match only never-placed containments
        QVERIFY(rectChanged.at(i).at(0).toUInt() != UINT_MAX);
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
    QCOMPARE(added.first().at(0).toUInt(), static_cast<uint>(id));
    QCOMPARE(rectChanged.count(), 1);
    QCOMPARE(rectChanged.first().at(0).toUInt(), static_cast<uint>(id));
}

QTEST_MAIN(CoronaScreenIdBoundaryTest)

#include "coronascreenidboundarytest.moc"
