/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Memoization contract for ActivitiesInfo::StatesCache: a cold read fetches once,
// repeated reads reuse the cached records, and invalidate() forces a refetch. The
// fetch callback counts calls, so "one activity-manager query per sync" is asserted
// without a live DBus activity manager.

#include "../app/data/activitystatescache.h"
#include "../app/data/activitiesinfo.h"
#include "../app/data/activitydata.h"

#include <QObject>
#include <QtTest>

using namespace Latte;

static QVector<ActivitiesInfo::Record> sample()
{
    return {
        {QStringLiteral("a"), Data::Activity::Running},
        {QStringLiteral("b"), Data::Activity::Stopped},
        {QStringLiteral("c"), Data::Activity::Running},
    };
}

class ActivityStatesCacheTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void coldReadFetchesOnce();
    void repeatedReadsReuseCache();
    void invalidateForcesRefetch();
    void runningActivitiesFiltersCachedRecords();
};

void ActivityStatesCacheTest::coldReadFetchesOnce()
{
    int calls = 0;
    ActivitiesInfo::StatesCache cache([&calls]() { ++calls; return sample(); });

    QCOMPARE(cache.records().size(), 3);
    QCOMPARE(calls, 1);
}

void ActivityStatesCacheTest::repeatedReadsReuseCache()
{
    int calls = 0;
    ActivitiesInfo::StatesCache cache([&calls]() { ++calls; return sample(); });

    cache.records();
    cache.runningActivities();
    cache.records();

    // three reads, one fetch -- the >=2-round-trips-per-sync reduction
    QCOMPARE(calls, 1);
}

void ActivityStatesCacheTest::invalidateForcesRefetch()
{
    int calls = 0;
    ActivitiesInfo::StatesCache cache([&calls]() { ++calls; return sample(); });

    cache.records();
    QCOMPARE(calls, 1);

    cache.invalidate();
    cache.records();
    QCOMPARE(calls, 2);
}

void ActivityStatesCacheTest::runningActivitiesFiltersCachedRecords()
{
    ActivitiesInfo::StatesCache cache([]() { return sample(); });

    QCOMPARE(cache.runningActivities(),
             QStringList({QStringLiteral("a"), QStringLiteral("c")}));
}

QTEST_GUILESS_MAIN(ActivityStatesCacheTest)

#include "activitystatescachetest.moc"
