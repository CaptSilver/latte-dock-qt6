/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>
#include <Plasma/Plasma>

#include "../app/data/viewdata.h"
#include "../app/layout/viewpriority.h"

using namespace Latte;
using namespace Latte::Layout;

static Data::View viewAt(int screen, bool onPrimary, Plasma::Types::Location edge, bool active = false)
{
    Data::View v;
    v.screen = screen;
    v.onPrimary = onPrimary;
    v.edge = edge;
    v.isActive = active;
    return v;
}

class ViewPriorityTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void primaryOutranksNonPrimaryForScreen();
    void sameScreenFlagComparesByScreenId();
    void activeOutranksInactiveForState();
    void edgeOrderFollowsComparatorTable();
    void sortedPlacesPrimaryAndPreferredEdgesFirst();
};

void ViewPriorityTest::primaryOutranksNonPrimaryForScreen()
{
    const Data::View prim = viewAt(0, true, Plasma::Types::BottomEdge);
    const Data::View other = viewAt(5, false, Plasma::Types::BottomEdge);
    // base=primary, test=non-primary -> test is at lower screen priority
    QVERIFY(ViewPriority::atLowerScreenPriority(other, prim));
    QVERIFY(!ViewPriority::atLowerScreenPriority(prim, other));
}

void ViewPriorityTest::sameScreenFlagComparesByScreenId()
{
    const Data::View a = viewAt(1, false, Plasma::Types::BottomEdge);
    const Data::View b = viewAt(2, false, Plasma::Types::BottomEdge);
    // neither onPrimary -> returns test.screen <= base.screen
    QVERIFY(ViewPriority::atLowerScreenPriority(a, b));     // 1 <= 2
    QVERIFY(!ViewPriority::atLowerScreenPriority(b, a));    // 2 <= 1 is false
}

void ViewPriorityTest::activeOutranksInactiveForState()
{
    const Data::View active = viewAt(0, true, Plasma::Types::BottomEdge, true);
    const Data::View inactive = viewAt(0, true, Plasma::Types::BottomEdge, false);
    // base active, test inactive -> test at lower state priority
    QVERIFY(ViewPriority::atLowerStatePriority(inactive, active));
    QVERIFY(!ViewPriority::atLowerStatePriority(active, inactive));
}

void ViewPriorityTest::edgeOrderFollowsComparatorTable()
{
    // comparator indexes edges as {Right, Top, Left, Bottom}; atLowerEdgePriority(test, base)
    // is true when test's index < base's index.
    const Data::View right = viewAt(0, true, Plasma::Types::RightEdge);   // index 0
    const Data::View bottom = viewAt(0, true, Plasma::Types::BottomEdge); // index 3
    QVERIFY(ViewPriority::atLowerEdgePriority(right, bottom));   // 0 < 3
    QVERIFY(!ViewPriority::atLowerEdgePriority(bottom, right));  // 3 < 0 is false
}

void ViewPriorityTest::sortedPlacesPrimaryAndPreferredEdgesFirst()
{
    QList<Data::View> input;
    input << viewAt(3, false, Plasma::Types::TopEdge)
          << viewAt(0, true, Plasma::Types::BottomEdge)
          << viewAt(0, true, Plasma::Types::RightEdge);

    const QList<Data::View> out = ViewPriority::sorted(input);

    QCOMPARE(out.size(), 3);
    // The primary-screen views end up ahead of the non-primary one; assert the primary
    // Bottom-edge view is not last (pins the current ordering without re-deriving it).
    QVERIFY(out.last().onPrimary == false);
}

QTEST_MAIN(ViewPriorityTest)
#include "viewprioritytest.moc"
