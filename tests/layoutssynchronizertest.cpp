/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-object test for Latte::Layouts::Synchronizer (app/layouts/synchronizer.cpp).
//
// Synchronizer is the layout book-keeper: it holds the passive layouts table
// (every known layout) and the active central layouts (those loaded in memory).
// Its header drags in lattecorona.h, so we build a real headless Corona and take
// its layoutsManager()->synchronizer(). Loading real layouts needs a live dock,
// but setLayoutsTable() lets us inject the passive table directly, which makes
// the whole query/filter surface reachable: the table round-trip and change
// detection, data()/layouts()/menuLayouts()/layoutExists() lookups, the
// empty-active-state accessors, and the activity-set algebra
// (activities/validActivities/freeActivities/runningActivities). The
// switch/sync/init orchestrators mutate live views and are left to the live
// capture.

#include "../app/lattecorona.h"
#include "coronafixture.h"

#include "../app/tools/commontools.h"
#include "../app/layouts/manager.h"
#include "../app/layouts/synchronizer.h"
#include "../app/data/layoutdata.h"
#include "../app/data/layoutstable.h"

#include <QSignalSpy>
#include <QStringList>
#include <QDBusConnection>
#include <QFileInfo>
#include <QtTest>

using namespace Latte;

class LayoutsSynchronizerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void coronaWritesInsideTheSandbox();
    void cleanupTestCase();

    void injectsLayoutsTable();
    void dataLooksUpByName();
    void layoutsAndExistence();
    void menuLayoutsFiltersHiddenAndSorts();
    void activeStateEmptyWithoutLoadedLayouts();
    void moreEmptyStateAccessors();
    void activitySetAlgebra();
    void updateLayoutsTableKeepsCleanRows();

private:
    static Data::LayoutsTable makeTable();

    Latte::Corona *m_corona{nullptr};

    CoronaSandbox m_sandbox;
    Layouts::Synchronizer *m_sync{nullptr};
};

Data::LayoutsTable LayoutsSynchronizerTest::makeTable()
{
    Data::LayoutsTable table;

    Data::Layout zebra;
    zebra.id = QStringLiteral("/tmp/latte-cov/zebra.layout.latte");
    zebra.name = QStringLiteral("Zebra");
    zebra.isShownInMenu = true;

    Data::Layout alpha;
    alpha.id = QStringLiteral("/tmp/latte-cov/alpha.layout.latte");
    alpha.name = QStringLiteral("alpha");
    alpha.isShownInMenu = true;

    Data::Layout hidden;
    hidden.id = QStringLiteral("/tmp/latte-cov/hidden.layout.latte");
    hidden.name = QStringLiteral("Hidden");
    hidden.isShownInMenu = false;

    table << zebra << alpha << hidden;
    return table;
}

void LayoutsSynchronizerTest::initTestCase()
{
    // Redirect the config home before the Corona opens anything.
    QVERIFY(m_sandbox.arm());

    m_corona = buildHeadlessCorona();
    QVERIFY(m_corona->layoutsManager() != nullptr);
    m_sync = m_corona->layoutsManager()->synchronizer();
    QVERIFY(m_sync != nullptr);
    QVERIFY(m_corona->activitiesConsumer() != nullptr);
}

void LayoutsSynchronizerTest::coronaWritesInsideTheSandbox()
{
    // The Corona's rc must land in the throwaway dir, not the developer's home.
    QVERIFY2(Latte::configPath().startsWith(m_sandbox.dir.path()),
             qPrintable(QStringLiteral("config path escaped the sandbox: ") + Latte::configPath()));
    QVERIFY(QFileInfo::exists(m_sandbox.dir.path() + QLatin1Char('/')
                              + QCoreApplication::applicationName() + QStringLiteral("rc")));

    // GlobalShortcuts registers through KGlobalAccel over the session bus, and the
    // daemon on the other end writes kglobalshortcutsrc itself -- redirecting the
    // config home cannot stop that. The bus has to be unreachable instead.
    QVERIFY2(!QDBusConnection::sessionBus().isConnected(),
             "a reachable session bus lets this test register global shortcuts for real");
}

void LayoutsSynchronizerTest::cleanupTestCase()
{
    // Leak the Corona: its headless teardown can re-enter live-shell paths.
    m_corona = nullptr;
}

void LayoutsSynchronizerTest::injectsLayoutsTable()
{
    QSignalSpy spy(m_sync, &Layouts::Synchronizer::layoutsChanged);

    m_sync->setLayoutsTable(makeTable());
    QVERIFY(spy.count() >= 1);
    QCOMPARE(m_sync->layoutsTable(), makeTable());

    // An identical table is a no-op: no further layoutsChanged.
    const int before = spy.count();
    m_sync->setLayoutsTable(makeTable());
    QCOMPARE(spy.count(), before);
}

void LayoutsSynchronizerTest::dataLooksUpByName()
{
    const Data::Layout found = m_sync->data(QStringLiteral("Zebra"));
    QCOMPARE(found.name, QStringLiteral("Zebra"));
    QCOMPARE(found.id, QStringLiteral("/tmp/latte-cov/zebra.layout.latte"));

    const Data::Layout missing = m_sync->data(QStringLiteral("No Such Layout"));
    QVERIFY(missing.id.isEmpty());
    QVERIFY(missing.name.isEmpty());
}

void LayoutsSynchronizerTest::layoutsAndExistence()
{
    const QStringList names = m_sync->layouts();
    QCOMPARE(names.size(), 3);
    QVERIFY(names.contains(QStringLiteral("Zebra")));
    QVERIFY(names.contains(QStringLiteral("alpha")));
    QVERIFY(names.contains(QStringLiteral("Hidden")));

    QVERIFY(m_sync->layoutExists(QStringLiteral("alpha")));
    QVERIFY(!m_sync->layoutExists(QStringLiteral("No Such Layout")));
}

void LayoutsSynchronizerTest::menuLayoutsFiltersHiddenAndSorts()
{
    // Only the in-menu layouts, case-insensitively sorted; Hidden is dropped.
    const QStringList menu = m_sync->menuLayouts();
    QCOMPARE(menu, QStringList({QStringLiteral("alpha"), QStringLiteral("Zebra")}));
}

void LayoutsSynchronizerTest::activeStateEmptyWithoutLoadedLayouts()
{
    QVERIFY(m_sync->currentLayouts().isEmpty());
    QVERIFY(m_sync->currentViews().isEmpty());
    QVERIFY(m_sync->currentLayoutsNames().isEmpty());
    QVERIFY(m_sync->centralLayoutsNames().isEmpty());
    QVERIFY(m_sync->centralLayoutsForActivity(QStringLiteral("any")).isEmpty());

    QCOMPARE(m_sync->centralLayoutPos(QStringLiteral("Zebra")), -1);
    QCOMPARE(m_sync->centralLayout(QStringLiteral("Zebra")), nullptr);
    QCOMPARE(m_sync->layout(QStringLiteral("Zebra")), nullptr);
    QCOMPARE(m_sync->viewForContainment(uint(123)), nullptr);
    QVERIFY(!m_sync->latteViewExists(nullptr));
}

void LayoutsSynchronizerTest::moreEmptyStateAccessors()
{
    QVERIFY(m_sync->currentViewsWithPlasmaShortcuts().isEmpty());
    QVERIFY(m_sync->currentOriginalViews().isEmpty());
    QVERIFY(m_sync->sortedCurrentViews().isEmpty());
    QVERIFY(m_sync->sortedCurrentOriginalViews().isEmpty());
    QVERIFY(m_sync->viewsBasedOnActivityId(QStringLiteral("some-activity")).isEmpty());

    // The activities controller is created at construction.
    QVERIFY(m_sync->activitiesController() != nullptr);

    // No active layouts, so syncing to files is a no-op.
    m_sync->syncActiveLayoutsToOriginalFiles();

    m_sync->setIsSingleLayoutInDeprecatedRenaming(true);
    m_sync->setIsSingleLayoutInDeprecatedRenaming(false);
}

void LayoutsSynchronizerTest::activitySetAlgebra()
{
    const QStringList all = m_sync->activities();

    // Filtering an empty request yields nothing, and an id that is not a real
    // activity is filtered out.
    QVERIFY(m_sync->validActivities(QStringList()).isEmpty());
    QVERIFY(m_sync->validActivities({QStringLiteral("nonexistent-activity-xyz")}).isEmpty());

    // Nothing is assigned to a layout, so every activity is free.
    QCOMPARE(m_sync->freeActivities().size(), all.size());

    // free-running activities are the running set minus the assigned ones; with
    // nothing assigned the two sets coincide. (The running set is read from the
    // activity manager, which need not match the KActivities consumer view, so
    // we do not tie it back to activities().)
    const QStringList running = m_sync->runningActivities();
    const QStringList freeRunning = m_sync->freeRunningActivities();
    QCOMPARE(freeRunning.size(), running.size());
    for (const QString &id : freeRunning) {
        QVERIFY(running.contains(id));
    }
}

void LayoutsSynchronizerTest::updateLayoutsTableKeepsCleanRows()
{
    // With no active central layouts and no error/warning rows, the refresh is a
    // walk that changes nothing.
    m_sync->updateLayoutsTable();
    QCOMPARE(m_sync->data(QStringLiteral("Zebra")).name, QStringLiteral("Zebra"));
    QCOMPARE(m_sync->layouts().size(), 3);
}

QTEST_MAIN(LayoutsSynchronizerTest)
#include "layoutssynchronizertest.moc"
