/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wm/windowtrackingpredicates.h"
#include "wm/windowinfowrap.h"

#include <QRect>
#include <QtTest>

using namespace Latte::WindowSystem;

// Helper to build a WindowInfoWrap fixture
static WindowInfoWrap makeWinfo(bool valid, bool active, bool minimized, bool shaded,
                                 bool maximized, QRect geom,
                                 const QString &wid = QStringLiteral("w1"))
{
    WindowInfoWrap w;
    w.setIsValid(valid);
    w.setIsActive(active);
    w.setIsMinimized(minimized);
    w.setIsShaded(shaded);
    w.setIsMaxVert(maximized);
    w.setIsMaxHoriz(maximized);
    w.setGeometry(geom);
    w.setWid(QVariant(wid));
    return w;
}

class WindowTrackingPredicatesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void intersects_minimizedOrShaded_false();
    void isActive_requiresValidActiveNotMinimized();
    void isActiveInViewScreen_intersectsScaledScreen();
    void isMaximizedInViewScreen_maximizedOnScreen();
    void registry_isIgnoredPlasmaWhitelisted_contains();
    void hasBlockedTracking_whitelistWins();
    void shouldRegister_idempotentAndSkipNull();
};

void WindowTrackingPredicatesTest::intersects_minimizedOrShaded_false()
{
    QRect viewGeo(0, 0, 200, 50);
    QRect overlap(50, 10, 100, 30); // overlaps viewGeo

    // minimized → false
    auto min = makeWinfo(true, false, true, false, false, overlap);
    QVERIFY(!WindowTrackingPredicates::intersects(min, viewGeo));

    // shaded → false
    auto shaded = makeWinfo(true, false, false, true, false, overlap);
    QVERIFY(!WindowTrackingPredicates::intersects(shaded, viewGeo));

    // non-overlapping → false
    auto nooverlap = makeWinfo(true, false, false, false, false, QRect(500, 500, 100, 100));
    QVERIFY(!WindowTrackingPredicates::intersects(nooverlap, viewGeo));

    // overlapping → true
    auto ok = makeWinfo(true, false, false, false, false, overlap);
    QVERIFY(WindowTrackingPredicates::intersects(ok, viewGeo));
}

void WindowTrackingPredicatesTest::isActive_requiresValidActiveNotMinimized()
{
    // valid + active + not-minimized → true
    QVERIFY(WindowTrackingPredicates::isActive(makeWinfo(true, true, false, false, false, QRect(0,0,100,100))));
    // invalid → false
    QVERIFY(!WindowTrackingPredicates::isActive(makeWinfo(false, true, false, false, false, QRect(0,0,100,100))));
    // not active → false
    QVERIFY(!WindowTrackingPredicates::isActive(makeWinfo(true, false, false, false, false, QRect(0,0,100,100))));
    // minimized → false
    QVERIFY(!WindowTrackingPredicates::isActive(makeWinfo(true, true, true, false, false, QRect(0,0,100,100))));
}

void WindowTrackingPredicatesTest::isActiveInViewScreen_intersectsScaledScreen()
{
    QRect screen(0, 0, 1920, 1080);
    QRect otherScreen(1920, 0, 1920, 1080);

    // on-screen valid active non-minimized → true
    auto ok = makeWinfo(true, true, false, false, false, QRect(100, 100, 200, 200));
    QVERIFY(WindowTrackingPredicates::isActiveInViewScreen(ok, screen));

    // on other screen → false
    auto other = makeWinfo(true, true, false, false, false, QRect(2000, 100, 200, 200));
    QVERIFY(!WindowTrackingPredicates::isActiveInViewScreen(other, screen));

    // minimized → false
    auto min = makeWinfo(true, true, true, false, false, QRect(100, 100, 200, 200));
    QVERIFY(!WindowTrackingPredicates::isActiveInViewScreen(min, screen));

    // not active → false
    auto notactive = makeWinfo(true, false, false, false, false, QRect(100, 100, 200, 200));
    QVERIFY(!WindowTrackingPredicates::isActiveInViewScreen(notactive, screen));
}

void WindowTrackingPredicatesTest::isMaximizedInViewScreen_maximizedOnScreen()
{
    QRect screen(0, 0, 1920, 1080);
    QRect otherScreen(1920, 0, 1920, 1080);

    // maximized valid not-min not-shaded inside screen → true
    auto ok = makeWinfo(true, false, false, false, true, QRect(0, 0, 1920, 1080));
    QVERIFY(WindowTrackingPredicates::isMaximizedInViewScreen(ok, screen));

    // other screen → false
    auto other = makeWinfo(true, false, false, false, true, QRect(1920, 0, 1920, 1080));
    QVERIFY(!WindowTrackingPredicates::isMaximizedInViewScreen(other, screen));

    // shaded → false
    auto shaded = makeWinfo(true, false, false, true, true, QRect(0, 0, 1920, 1080));
    QVERIFY(!WindowTrackingPredicates::isMaximizedInViewScreen(shaded, screen));

    // not maximized → false
    auto notmax = makeWinfo(true, false, false, false, false, QRect(0, 0, 1920, 1080));
    QVERIFY(!WindowTrackingPredicates::isMaximizedInViewScreen(notmax, screen));
}

void WindowTrackingPredicatesTest::registry_isIgnoredPlasmaWhitelisted_contains()
{
    using WId = Latte::WindowSystem::WindowId;
    WId w1 = QVariant(QStringLiteral("aaa"));
    WId w2 = QVariant(QStringLiteral("bbb"));
    QList<WId> ignored = {w1};
    QList<WId> plasmaIgnored = {w2};
    QList<WId> whitelisted = {w1};

    QVERIFY(WindowTrackingPredicates::isIgnored(ignored, w1));
    QVERIFY(!WindowTrackingPredicates::isIgnored(ignored, w2));
    QVERIFY(WindowTrackingPredicates::isRegisteredPlasmaIgnored(plasmaIgnored, w2));
    QVERIFY(!WindowTrackingPredicates::isRegisteredPlasmaIgnored(plasmaIgnored, w1));
    QVERIFY(WindowTrackingPredicates::isWhitelisted(whitelisted, w1));
    QVERIFY(!WindowTrackingPredicates::isWhitelisted(whitelisted, w2));
}

void WindowTrackingPredicatesTest::hasBlockedTracking_whitelistWins()
{
    using WId = Latte::WindowSystem::WindowId;
    WId w = QVariant(QStringLiteral("ww"));
    WId other = QVariant(QStringLiteral("xx"));

    QList<WId> ignored = {w};
    QList<WId> plasma = {w};
    QList<WId> white = {w};
    QList<WId> empty;

    // plasmaIgnored+whitelisted → false (whitelist wins)
    QVERIFY(!WindowTrackingPredicates::hasBlockedTracking(empty, plasma, white, w));
    // ignored-only → true
    QVERIFY(WindowTrackingPredicates::hasBlockedTracking(ignored, empty, empty, w));
    // plasmaIgnored-only → true
    QVERIFY(WindowTrackingPredicates::hasBlockedTracking(empty, plasma, empty, w));
    // none → false
    QVERIFY(!WindowTrackingPredicates::hasBlockedTracking(empty, empty, empty, w));
    // ignored-but-whitelisted → false
    QVERIFY(!WindowTrackingPredicates::hasBlockedTracking(ignored, empty, white, w));
}

void WindowTrackingPredicatesTest::shouldRegister_idempotentAndSkipNull()
{
    using WId = Latte::WindowSystem::WindowId;
    WId w = QVariant(QStringLiteral("zz"));
    WId nullW;
    QList<WId> set = {w};
    QList<WId> empty;

    // null → false
    QVERIFY(!WindowTrackingPredicates::shouldRegister(empty, nullW));
    // already present → false
    QVERIFY(!WindowTrackingPredicates::shouldRegister(set, w));
    // fresh → true
    QVERIFY(WindowTrackingPredicates::shouldRegister(empty, w));
}

QTEST_GUILESS_MAIN(WindowTrackingPredicatesTest)

#include "windowtrackingpredicatestest.moc"
