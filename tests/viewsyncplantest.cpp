/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include "../app/layout/viewsyncplan.h"

using namespace Latte::Layout;

class ViewSyncPlanTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void mappedContainmentWithoutViewIsAdded();
    void originalViewMissingFromMapIsRemoved();
    void originalViewStillInMapIsReconsidered();
    void mappedContainmentThatAlreadyHasViewIsNotAdded();
    void emptyInputsGiveEmptyPlan();
};

void ViewSyncPlanTest::mappedContainmentWithoutViewIsAdded()
{
    ViewSyncInputs in;
    in.containmentIds = {1, 2};
    in.viewedContainmentIds = {2};       // 1 has no view
    in.mapIds = {1, 2};
    const ViewSyncPlan p = ViewSyncPlanner::plan(in);
    QCOMPARE(p.toAdd, QList<uint>{1});
}

void ViewSyncPlanTest::originalViewMissingFromMapIsRemoved()
{
    ViewSyncInputs in;
    in.originalViewContainmentIds = {7};
    in.mapIds = {};                       // 7 not wanted
    const ViewSyncPlan p = ViewSyncPlanner::plan(in);
    QCOMPARE(p.toRemove, QList<uint>{7});
    QVERIFY(p.toReconsider.isEmpty());
}

void ViewSyncPlanTest::originalViewStillInMapIsReconsidered()
{
    ViewSyncInputs in;
    in.originalViewContainmentIds = {7};
    in.mapIds = {7};
    const ViewSyncPlan p = ViewSyncPlanner::plan(in);
    QCOMPARE(p.toReconsider, QList<uint>{7});
    QVERIFY(p.toRemove.isEmpty());
}

void ViewSyncPlanTest::mappedContainmentThatAlreadyHasViewIsNotAdded()
{
    ViewSyncInputs in;
    in.containmentIds = {3};
    in.viewedContainmentIds = {3};       // already has a view
    in.mapIds = {3};
    const ViewSyncPlan p = ViewSyncPlanner::plan(in);
    QVERIFY(p.toAdd.isEmpty());
}

void ViewSyncPlanTest::emptyInputsGiveEmptyPlan()
{
    const ViewSyncPlan p = ViewSyncPlanner::plan(ViewSyncInputs{});
    QVERIFY(p.toAdd.isEmpty() && p.toRemove.isEmpty() && p.toReconsider.isEmpty());
}

QTEST_MAIN(ViewSyncPlanTest)
#include "viewsyncplantest.moc"
