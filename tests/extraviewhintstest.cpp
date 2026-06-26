/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wm/tracker/extraviewhints.h"

#include <QtTest>
#include <Plasma/Plasma>

using namespace Latte::WindowSystem::Tracker;

class ExtraViewHintsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptyOrNoHorizontal_emptyResult();
    void horizontalTouchesBusyVerticalSameScreen_true();
    void differentScreen_false();
    void notBusy_false();
    void wrongLocationPairing_false();
    void disabledOrNotTracking_skipped();
    void edgeTouchCallbackGatesResult();
};

// always-true edge touch stub
static auto alwaysTouch = [](const TrackedViewGeometry &, const TrackedViewGeometry &) { return true; };

static TrackedViewGeometry makeHor(int key, int screenId, Plasma::Types::Location loc, bool enabled = true, bool tracking = true)
{
    TrackedViewGeometry v;
    v.viewKey = key;
    v.enabled = enabled;
    v.trackingCurrentActivity = tracking;
    v.isHorizontal = true;
    v.isVertical = false;
    v.screenId = screenId;
    v.location = loc;
    return v;
}

static TrackedViewGeometry makeVer(int key, int screenId, bool topBusy, bool bottomBusy, bool enabled = true, bool tracking = true)
{
    TrackedViewGeometry v;
    v.viewKey = key;
    v.enabled = enabled;
    v.trackingCurrentActivity = tracking;
    v.isHorizontal = false;
    v.isVertical = true;
    v.screenId = screenId;
    v.isTouchingTopViewAndIsBusy = topBusy;
    v.isTouchingBottomViewAndIsBusy = bottomBusy;
    return v;
}

void ExtraViewHintsTest::emptyOrNoHorizontal_emptyResult()
{
    // empty list
    auto r1 = ExtraViewHints::bucketHorizontalTouchingBusyVertical({}, alwaysTouch);
    QVERIFY(r1.isEmpty());

    // only vertical views
    QList<TrackedViewGeometry> views = { makeVer(1, 0, true, true) };
    auto r2 = ExtraViewHints::bucketHorizontalTouchingBusyVertical(views, alwaysTouch);
    QVERIFY(r2.isEmpty());
}

void ExtraViewHintsTest::horizontalTouchesBusyVerticalSameScreen_true()
{
    // BottomEdge hor + isTouchingBottomViewAndIsBusy ver on same screen, edge touch → true
    auto hor = makeHor(1, 0, Plasma::Types::BottomEdge);
    auto ver = makeVer(2, 0, false, true); // bottomBusy=true
    QList<TrackedViewGeometry> views = { hor, ver };
    auto r = ExtraViewHints::bucketHorizontalTouchingBusyVertical(views, alwaysTouch);
    QVERIFY(r.contains(1));
    QVERIFY(r.value(1) == true);
}

void ExtraViewHintsTest::differentScreen_false()
{
    auto hor = makeHor(1, 0, Plasma::Types::BottomEdge);
    auto ver = makeVer(2, 1, false, true); // different screen
    QList<TrackedViewGeometry> views = { hor, ver };
    auto r = ExtraViewHints::bucketHorizontalTouchingBusyVertical(views, alwaysTouch);
    QVERIFY(!r.contains(1) || r.value(1) == false);
}

void ExtraViewHintsTest::notBusy_false()
{
    auto hor = makeHor(1, 0, Plasma::Types::BottomEdge);
    auto ver = makeVer(2, 0, false, false); // not busy
    QList<TrackedViewGeometry> views = { hor, ver };
    auto r = ExtraViewHints::bucketHorizontalTouchingBusyVertical(views, alwaysTouch);
    QVERIFY(!r.contains(1) || r.value(1) == false);
}

void ExtraViewHintsTest::wrongLocationPairing_false()
{
    // TopEdge hor but ver only has bottomBusy → false
    auto hor = makeHor(1, 0, Plasma::Types::TopEdge);
    auto ver = makeVer(2, 0, false, true); // only bottomBusy
    QList<TrackedViewGeometry> views = { hor, ver };
    auto r1 = ExtraViewHints::bucketHorizontalTouchingBusyVertical(views, alwaysTouch);
    QVERIFY(!r1.value(1, false));

    // TopEdge + topBusy → true
    auto ver2 = makeVer(3, 0, true, false); // topBusy
    QList<TrackedViewGeometry> views2 = { makeHor(2, 0, Plasma::Types::TopEdge), ver2 };
    auto r2 = ExtraViewHints::bucketHorizontalTouchingBusyVertical(views2, alwaysTouch);
    QVERIFY(r2.value(2, false));
}

void ExtraViewHintsTest::disabledOrNotTracking_skipped()
{
    // disabled hor → not in result
    auto hor = makeHor(1, 0, Plasma::Types::BottomEdge, false, true); // disabled
    auto ver = makeVer(2, 0, false, true);
    QList<TrackedViewGeometry> views = { hor, ver };
    auto r1 = ExtraViewHints::bucketHorizontalTouchingBusyVertical(views, alwaysTouch);
    QVERIFY(!r1.contains(1));

    // not tracking hor → not in result
    auto hor2 = makeHor(3, 0, Plasma::Types::BottomEdge, true, false); // not tracking
    QList<TrackedViewGeometry> views2 = { hor2, ver };
    auto r2 = ExtraViewHints::bucketHorizontalTouchingBusyVertical(views2, alwaysTouch);
    QVERIFY(!r2.contains(3));

    // disabled ver → ignored
    auto hor3 = makeHor(5, 0, Plasma::Types::BottomEdge);
    auto ver_disabled = makeVer(6, 0, false, true, false, true); // disabled
    QList<TrackedViewGeometry> views3 = { hor3, ver_disabled };
    auto r3 = ExtraViewHints::bucketHorizontalTouchingBusyVertical(views3, alwaysTouch);
    QVERIFY(!r3.value(5, false));
}

void ExtraViewHintsTest::edgeTouchCallbackGatesResult()
{
    // all conditions met but hasEdgeTouch → false → result false
    auto hor = makeHor(1, 0, Plasma::Types::BottomEdge);
    auto ver = makeVer(2, 0, false, true);
    QList<TrackedViewGeometry> views = { hor, ver };
    auto neverTouch = [](const TrackedViewGeometry &, const TrackedViewGeometry &) { return false; };
    auto r = ExtraViewHints::bucketHorizontalTouchingBusyVertical(views, neverTouch);
    QVERIFY(!r.value(1, false));
}

QTEST_GUILESS_MAIN(ExtraViewHintsTest)

#include "extraviewhintstest.moc"
