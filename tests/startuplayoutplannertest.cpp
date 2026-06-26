/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include "../app/apptypes.h"
#include "../app/startuplayoutplanner.h"

using namespace Latte;

class StartupLayoutPlannerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void multipleLayoutsModeLoadsEmptyName();
    void singleModeLoadsExistingLayout();
    void singleModeMissingLayoutFallsBackToDefaultTemplate();
    void singleModeMissingLayoutAndMissingDefaultEnsuresDefault();
    void defaultOnStartupForcesFreshDefaultAndSingleMemory();
    void explicitStartupNameLoadsItAndForcesSingleMemory();
    void cliMemoryOverrideIsPersistedAndApplied();
};

static StartupInputs baseInputs()
{
    StartupInputs in;
    in.userSetMemoryUsage = -1;
    in.currentMemoryUsage = MemoryUsage::SingleLayout;
    in.singleModeLayoutName = QStringLiteral("My Layout");
    in.defaultLayoutOnStartup = false;
    in.layoutNameOnStartUp = QString();
    in.defaultLayoutTemplateName = QStringLiteral("Default");
    in.existingLayoutNames = {QStringLiteral("My Layout"), QStringLiteral("Default")};
    return in;
}

void StartupLayoutPlannerTest::multipleLayoutsModeLoadsEmptyName()
{
    StartupInputs in = baseInputs();
    in.currentMemoryUsage = MemoryUsage::MultipleLayouts;
    StartupPlan p = StartupLayoutPlanner::plan(in);
    QVERIFY(p.loadLayoutName.isEmpty());
    QVERIFY(!p.createFreshDefaultLayout);
    QVERIFY(!p.ensureDefaultLayoutExists);
    QVERIFY(!p.memoryUsageToSet.has_value());
}

void StartupLayoutPlannerTest::singleModeLoadsExistingLayout()
{
    StartupPlan p = StartupLayoutPlanner::plan(baseInputs());
    QCOMPARE(p.loadLayoutName, QStringLiteral("My Layout"));
    QVERIFY(!p.ensureDefaultLayoutExists);
    QVERIFY(!p.createFreshDefaultLayout);
}

void StartupLayoutPlannerTest::singleModeMissingLayoutFallsBackToDefaultTemplate()
{
    StartupInputs in = baseInputs();
    in.singleModeLayoutName = QStringLiteral("Gone");
    StartupPlan p = StartupLayoutPlanner::plan(in);
    QCOMPARE(p.loadLayoutName, QStringLiteral("Default"));
    QVERIFY(!p.ensureDefaultLayoutExists);   // Default already exists
}

void StartupLayoutPlannerTest::singleModeMissingLayoutAndMissingDefaultEnsuresDefault()
{
    StartupInputs in = baseInputs();
    in.singleModeLayoutName = QStringLiteral("Gone");
    in.existingLayoutNames = {};   // Default missing too
    StartupPlan p = StartupLayoutPlanner::plan(in);
    QCOMPARE(p.loadLayoutName, QStringLiteral("Default"));
    QVERIFY(p.ensureDefaultLayoutExists);    // create Default + setOnAllActivities
}

void StartupLayoutPlannerTest::defaultOnStartupForcesFreshDefaultAndSingleMemory()
{
    StartupInputs in = baseInputs();
    in.defaultLayoutOnStartup = true;
    StartupPlan p = StartupLayoutPlanner::plan(in);
    QVERIFY(p.createFreshDefaultLayout);
    QVERIFY(p.loadResolvedAfterCreate);
    QCOMPARE(p.memoryUsageToSet.value(), MemoryUsage::SingleLayout);
}

void StartupLayoutPlannerTest::explicitStartupNameLoadsItAndForcesSingleMemory()
{
    StartupInputs in = baseInputs();
    in.layoutNameOnStartUp = QStringLiteral("Chosen");
    StartupPlan p = StartupLayoutPlanner::plan(in);
    QCOMPARE(p.loadLayoutName, QStringLiteral("Chosen"));
    QCOMPARE(p.memoryUsageToSet.value(), MemoryUsage::SingleLayout);
    QVERIFY(!p.createFreshDefaultLayout);
}

void StartupLayoutPlannerTest::cliMemoryOverrideIsPersistedAndApplied()
{
    StartupInputs in = baseInputs();
    in.userSetMemoryUsage = static_cast<int>(MemoryUsage::MultipleLayouts);
    in.currentMemoryUsage = MemoryUsage::SingleLayout;   // override flips effective mode
    StartupPlan p = StartupLayoutPlanner::plan(in);
    QCOMPARE(p.memoryUsageToSet.value(), MemoryUsage::MultipleLayouts);
    QVERIFY(p.loadLayoutName.isEmpty());   // effective mode is Multiple -> empty name
}

QTEST_MAIN(StartupLayoutPlannerTest)
#include "startuplayoutplannertest.moc"
