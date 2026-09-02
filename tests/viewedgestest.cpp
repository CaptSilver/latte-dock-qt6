/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include "../app/layout/viewedges.h"

using namespace Latte;
using namespace Latte::Layout;

class ViewEdgesTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void canonicalEdgeOrderIsBottomLeftTopRight();
    void freeEdgesDropOccupiedAndKeepOrder();
    void freeEdgesWithNothingOccupiedReturnsAll();
    void freeEdgesWithEverythingOccupiedReturnsEmpty();
    void freeEdgesIgnoreRepeatedOccupiedEdge();
    void newViewKeepsPreferredEdgeWhenFree();
    void newViewTakesFirstFreeEdgeWhenPreferredIsTaken();
    void newViewFallsBackToBottomWhenNoEdgeIsFree();
};

void ViewEdgesTest::canonicalEdgeOrderIsBottomLeftTopRight()
{
    //! this order decides where a new view lands once its preferred edge is taken
    const QList<Plasma::Types::Location> expected{Plasma::Types::BottomEdge, Plasma::Types::LeftEdge,
                                                  Plasma::Types::TopEdge, Plasma::Types::RightEdge};
    QCOMPARE(ViewEdges::all(), expected);
}

void ViewEdgesTest::freeEdgesDropOccupiedAndKeepOrder()
{
    const QList<Plasma::Types::Location> occupied{Plasma::Types::LeftEdge};
    const QList<Plasma::Types::Location> expected{Plasma::Types::BottomEdge, Plasma::Types::TopEdge,
                                                  Plasma::Types::RightEdge};
    QCOMPARE(ViewEdges::freeFrom(occupied), expected);
}

void ViewEdgesTest::freeEdgesWithNothingOccupiedReturnsAll()
{
    QCOMPARE(ViewEdges::freeFrom(QList<Plasma::Types::Location>()), ViewEdges::all());
}

void ViewEdgesTest::freeEdgesWithEverythingOccupiedReturnsEmpty()
{
    QVERIFY(ViewEdges::freeFrom(ViewEdges::all()).isEmpty());
}

void ViewEdgesTest::freeEdgesIgnoreRepeatedOccupiedEdge()
{
    //! two views can share an edge with different alignments; the second one must not
    //! consume a different edge from the free list
    const QList<Plasma::Types::Location> occupied{Plasma::Types::TopEdge, Plasma::Types::TopEdge};
    const QList<Plasma::Types::Location> expected{Plasma::Types::BottomEdge, Plasma::Types::LeftEdge,
                                                  Plasma::Types::RightEdge};
    QCOMPARE(ViewEdges::freeFrom(occupied), expected);
}

void ViewEdgesTest::newViewKeepsPreferredEdgeWhenFree()
{
    const QList<Plasma::Types::Location> free{Plasma::Types::BottomEdge, Plasma::Types::TopEdge};
    QCOMPARE(ViewEdges::forNewView(free, Plasma::Types::TopEdge), Plasma::Types::TopEdge);
}

void ViewEdgesTest::newViewTakesFirstFreeEdgeWhenPreferredIsTaken()
{
    const QList<Plasma::Types::Location> free{Plasma::Types::BottomEdge, Plasma::Types::LeftEdge};
    QCOMPARE(ViewEdges::forNewView(free, Plasma::Types::TopEdge), Plasma::Types::BottomEdge);
}

void ViewEdgesTest::newViewFallsBackToBottomWhenNoEdgeIsFree()
{
    QCOMPARE(ViewEdges::forNewView(QList<Plasma::Types::Location>(), Plasma::Types::TopEdge),
             Plasma::Types::BottomEdge);
}

QTEST_MAIN(ViewEdgesTest)
#include "viewedgestest.moc"
