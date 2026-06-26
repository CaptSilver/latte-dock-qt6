/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include "../app/layouts/activitysetalgebra.h"

using namespace Latte::Layouts::ActivitySetAlgebra;

class ActivitySetAlgebraTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void freeActivities_removesAssigned();
    void freeActivities_removeAllSemantics();
    void freeActivities_emptyAssignedLeavesUnchanged();
    void freeRunningActivities_filtersAssigned();
    void freeRunningActivities_preservesOrder();
    void validActivities_dropsUnknown();
    void validActivities_emptyLayoutGivesEmpty();
};

void ActivitySetAlgebraTest::freeActivities_removesAssigned()
{
    const QStringList all = {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")};
    const QStringList assigned = {QStringLiteral("b"), QStringLiteral("d")};
    const QStringList result = freeActivities(all, assigned);
    QCOMPARE(result, (QStringList{QStringLiteral("a"), QStringLiteral("c")}));
}

void ActivitySetAlgebraTest::freeActivities_removeAllSemantics()
{
    // removeAll: all occurrences of an assigned id are dropped, non-assigned duplicates survive
    const QStringList all = {QStringLiteral("a"), QStringLiteral("a"), QStringLiteral("b")};
    const QStringList assigned = {QStringLiteral("b")};
    const QStringList result = freeActivities(all, assigned);
    QCOMPARE(result, (QStringList{QStringLiteral("a"), QStringLiteral("a")}));
}

void ActivitySetAlgebraTest::freeActivities_emptyAssignedLeavesUnchanged()
{
    const QStringList all = {QStringLiteral("x"), QStringLiteral("y")};
    QCOMPARE(freeActivities(all, {}), all);
}

void ActivitySetAlgebraTest::freeRunningActivities_filtersAssigned()
{
    const QStringList running = {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    const QStringList assigned = {QStringLiteral("b")};
    const QStringList result = freeRunningActivities(running, assigned);
    QCOMPARE(result, (QStringList{QStringLiteral("a"), QStringLiteral("c")}));
}

void ActivitySetAlgebraTest::freeRunningActivities_preservesOrder()
{
    const QStringList running = {QStringLiteral("z"), QStringLiteral("m"), QStringLiteral("a")};
    const QStringList result = freeRunningActivities(running, {});
    QCOMPARE(result, running);
}

void ActivitySetAlgebraTest::validActivities_dropsUnknown()
{
    const QStringList layout = {QStringLiteral("a"), QStringLiteral("x"), QStringLiteral("c")};
    const QStringList all = {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    const QStringList result = validActivities(layout, all);
    QCOMPARE(result, (QStringList{QStringLiteral("a"), QStringLiteral("c")}));
}

void ActivitySetAlgebraTest::validActivities_emptyLayoutGivesEmpty()
{
    const QStringList all = {QStringLiteral("a"), QStringLiteral("b")};
    QVERIFY(validActivities({}, all).isEmpty());
}

QTEST_GUILESS_MAIN(ActivitySetAlgebraTest)
#include "activitysetalgebratest.moc"
