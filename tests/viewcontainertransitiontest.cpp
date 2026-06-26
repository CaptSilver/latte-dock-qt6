/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Container-transition helpers for GenericLayout's active (m_latteViews) and waiting
// (m_waitingLatteViews) maps. Tested with plain QHash<int,int> so the move/take
// bookkeeping is verified without a live GenericLayout or View graph.

#include "../app/layout/viewcontainertransition.h"

#include <QHash>
#include <QObject>
#include <QtTest>

using namespace Latte::Layout;

class ViewContainerTransitionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void moveBetweenTransfersValue();
    void moveBetweenInsertsDefaultWhenAbsent();
    void takeFromEitherPrefersFirst();
    void takeFromEitherFallsBackToSecond();
    void takeFromEitherReturnsDefaultWhenNeither();
};

void ViewContainerTransitionTest::moveBetweenTransfersValue()
{
    QHash<int, int> from{{1, 100}};
    QHash<int, int> to;

    const int moved = ViewContainerTransition::moveBetween(from, to, 1);

    QCOMPARE(moved, 100);
    QVERIFY(!from.contains(1));
    QCOMPARE(to.value(1), 100);
}

void ViewContainerTransitionTest::moveBetweenInsertsDefaultWhenAbsent()
{
    QHash<int, int> from;
    QHash<int, int> to;

    // absent key -> take() yields 0, still inserted (matches the original
    // unconditional m_waitingLatteViews[sender] = view with a null view)
    const int moved = ViewContainerTransition::moveBetween(from, to, 7);

    QCOMPARE(moved, 0);
    QVERIFY(to.contains(7));
    QCOMPARE(to.value(7), 0);
}

void ViewContainerTransitionTest::takeFromEitherPrefersFirst()
{
    QHash<int, int> first{{1, 10}};
    QHash<int, int> second{{1, 20}};

    QCOMPARE(ViewContainerTransition::takeFromEither(first, second, 1), 10);
    QVERIFY(!first.contains(1));
    // second is consulted only on a first-map miss, so its entry is untouched
    QVERIFY(second.contains(1));
}

void ViewContainerTransitionTest::takeFromEitherFallsBackToSecond()
{
    QHash<int, int> first;
    QHash<int, int> second{{1, 20}};

    QCOMPARE(ViewContainerTransition::takeFromEither(first, second, 1), 20);
    QVERIFY(!second.contains(1));
}

void ViewContainerTransitionTest::takeFromEitherReturnsDefaultWhenNeither()
{
    QHash<int, int> first;
    QHash<int, int> second;

    QCOMPARE(ViewContainerTransition::takeFromEither(first, second, 99), 0);
}

QTEST_GUILESS_MAIN(ViewContainerTransitionTest)

#include "viewcontainertransitiontest.moc"
