/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Covers the pure logic of the activity-manager helper (app/data/activitiesinfo).
// KActivities 6 removed Consumer::runningActivities()/Info::State, so Latte reads
// the running/stopped distinction from org.kde.ActivityManager. The state mapping
// (Running == 2; a stopped activity == 4 must be excluded from the running set)
// is the bit that was previously lost, so it gets a deterministic test. The live
// DBus query is exercised by Track B on a session, not here.

#include "../app/data/activitiesinfo.h"
#include "../app/data/activitydata.h"

#include <QObject>
#include <QtTest>

using namespace Latte;

class ActivitiesInfoTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stateFromManager_data();
    void stateFromManager();
    void runningExcludesStopped();
    void runningPreservesManagerOrder();
};

void ActivitiesInfoTest::stateFromManager_data()
{
    QTest::addColumn<int>("managerState");
    QTest::addColumn<int>("expected");

    QTest::newRow("invalid")  << 0 << int(Data::Activity::Invalid);
    QTest::newRow("unknown")  << 1 << int(Data::Activity::Invalid);
    QTest::newRow("running")  << 2 << int(Data::Activity::Running);
    QTest::newRow("starting") << 3 << int(Data::Activity::Starting);
    QTest::newRow("stopped")  << 4 << int(Data::Activity::Stopped);
    QTest::newRow("stopping") << 5 << int(Data::Activity::Stopping);
    QTest::newRow("garbage")  << 99 << int(Data::Activity::Invalid);
}

void ActivitiesInfoTest::stateFromManager()
{
    QFETCH(int, managerState);
    QFETCH(int, expected);
    QCOMPARE(int(ActivitiesInfo::stateFromManager(managerState)), expected);
}

void ActivitiesInfoTest::runningExcludesStopped()
{
    const QVector<ActivitiesInfo::Record> records = {
        {QStringLiteral("a"), Data::Activity::Running},
        {QStringLiteral("b"), Data::Activity::Stopped},
        {QStringLiteral("c"), Data::Activity::Running},
        {QStringLiteral("d"), Data::Activity::Stopping},
        {QStringLiteral("e"), Data::Activity::Invalid},
    };

    const QStringList running = ActivitiesInfo::runningActivitiesFrom(records);

    QCOMPARE(running, QStringList({QStringLiteral("a"), QStringLiteral("c")}));
    QVERIFY2(!running.contains(QStringLiteral("b")), "a stopped activity leaked into the running set");
}

void ActivitiesInfoTest::runningPreservesManagerOrder()
{
    // Order must follow the manager's list so next/previous cycling is stable.
    const QVector<ActivitiesInfo::Record> records = {
        {QStringLiteral("z"), Data::Activity::Running},
        {QStringLiteral("m"), Data::Activity::Running},
        {QStringLiteral("a"), Data::Activity::Running},
    };

    QCOMPARE(ActivitiesInfo::runningActivitiesFrom(records),
             QStringList({QStringLiteral("z"), QStringLiteral("m"), QStringLiteral("a")}));
}

QTEST_GUILESS_MAIN(ActivitiesInfoTest)

#include "activitiesinfotest.moc"
