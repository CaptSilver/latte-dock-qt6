/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../app/wm/windowinfowrap.h"

#include <QTest>

using namespace Latte::WindowSystem;

class WindowInfoWrapTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultConstructed_isMainWindow();
    void noParent_isMainWindow();
    void uuidParent_isChildWindow();
    void wid_emptiness_contract();
};

void WindowInfoWrapTest::defaultConstructed_isMainWindow()
{
    WindowInfoWrap w;                       // never setParentId
    QVERIFY(w.isMainWindow());
    QVERIFY(!w.isChildWindow());
}

void WindowInfoWrapTest::noParent_isMainWindow()
{
    WindowInfoWrap w;
    w.setParentId(QVariant());              // explicit "no parent"
    QVERIFY(w.isMainWindow());

    WindowInfoWrap w2;
    w2.setParentId(QVariant(QString()));    // empty uuid string
    QVERIFY(w2.isMainWindow());
}

void WindowInfoWrapTest::uuidParent_isChildWindow()
{
    WindowInfoWrap w;
    // a real kwayland uuid is non-numeric -> toInt() would be 0 (the old bug)
    w.setParentId(QVariant(QStringLiteral("a1b2c3d4-0000-1111-2222-deadbeef")));
    QVERIFY(w.isChildWindow());
    QVERIFY(!w.isMainWindow());
}

void WindowInfoWrapTest::wid_emptiness_contract()
{
    // Windows::cleanupFaultyWindows() treats a wid whose string form is empty as
    // garbage. A default-constructed wrap must be empty (removed); a real uuid wid
    // must be non-empty (kept). Guards the kwayland6 uuid-id port: a non-numeric
    // uuid would yield toInt()==0 and wrongly flag every real window as faulty.
    WindowInfoWrap fresh;                                  // default-constructed
    QVERIFY(fresh.wid().toString().isEmpty());             // -> removed as faulty

    WindowInfoWrap real;
    real.setWid(QVariant(QStringLiteral("a1b2c3d4-0000-1111-2222-deadbeef")));
    QVERIFY(!real.wid().toString().isEmpty());             // -> kept
}

QTEST_MAIN(WindowInfoWrapTest)
#include "windowinfowraptest.moc"
