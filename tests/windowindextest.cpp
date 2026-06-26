/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Container contract for WindowSystem::WindowIndex: insert/lookup/remove/clear plus
// the invalid-key and null-window guards. Uses a fake int "window" so the id->window*
// fast path windowFor() relies on is tested without a live KWayland PlasmaWindow.

#include "../app/wm/windowindex.h"

#include <QObject>
#include <QtTest>

using namespace Latte::WindowSystem;

class WindowIndexTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void lookupReturnsInsertedWindow();
    void missingIdReturnsNull();
    void removeDropsEntry();
    void invalidIdIsIgnored();
    void nullWindowIsIgnored();
    void clearEmptiesIndex();
};

void WindowIndexTest::lookupReturnsInsertedWindow()
{
    int a = 1, b = 2;
    WindowIndex<int> index;
    index.insert(WindowId(QStringLiteral("a")), &a);
    index.insert(WindowId(QStringLiteral("b")), &b);

    QCOMPARE(index.lookup(WindowId(QStringLiteral("a"))), &a);
    QCOMPARE(index.lookup(WindowId(QStringLiteral("b"))), &b);
}

void WindowIndexTest::missingIdReturnsNull()
{
    WindowIndex<int> index;
    QCOMPARE(index.lookup(WindowId(QStringLiteral("nope"))), nullptr);
}

void WindowIndexTest::removeDropsEntry()
{
    int a = 1;
    WindowIndex<int> index;
    index.insert(WindowId(QStringLiteral("a")), &a);
    index.remove(WindowId(QStringLiteral("a")));
    QCOMPARE(index.lookup(WindowId(QStringLiteral("a"))), nullptr);
}

void WindowIndexTest::invalidIdIsIgnored()
{
    int a = 1;
    WindowIndex<int> index;
    index.insert(WindowId(), &a);   // invalid QVariant key
    QCOMPARE(index.lookup(WindowId()), nullptr);
}

void WindowIndexTest::nullWindowIsIgnored()
{
    WindowIndex<int> index;
    index.insert(WindowId(QStringLiteral("a")), nullptr);
    QCOMPARE(index.lookup(WindowId(QStringLiteral("a"))), nullptr);
}

void WindowIndexTest::clearEmptiesIndex()
{
    int a = 1;
    WindowIndex<int> index;
    index.insert(WindowId(QStringLiteral("a")), &a);
    index.clear();
    QCOMPARE(index.lookup(WindowId(QStringLiteral("a"))), nullptr);
}

QTEST_GUILESS_MAIN(WindowIndexTest)

#include "windowindextest.moc"
