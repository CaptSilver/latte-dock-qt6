/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Direct-link unit tests for the pure panel/window geometry predicates extracted
// from AbstractWindowInterface. They take a window rect plus the screen geometries
// and classify the window as fullscreen / Plasma panel / side panel, so a fixture
// list of screen rects is all that is needed.

#include "wm/windowgeometrypredicates.h"

#include <QRect>
#include <QtTest>

using namespace Latte::WindowSystem::WindowGeometryPredicates;

class WindowGeometryPredicatesTest : public QObject
{
    Q_OBJECT

private:
    // A single 1920x1080 screen at the origin, and a second one to its right.
    QList<QRect> single() const { return {QRect(0, 0, 1920, 1080)}; }
    QList<QRect> dual() const { return {QRect(0, 0, 1920, 1080), QRect(1920, 0, 1920, 1080)}; }

private Q_SLOTS:
    void isFullScreen_data();
    void isFullScreen();

    void isPlasmaPanel_data();
    void isPlasmaPanel();

    void isSidepanel_data();
    void isSidepanel();

    void scaledForGlobalScale_data();
    void scaledForGlobalScale();
};

void WindowGeometryPredicatesTest::isFullScreen_data()
{
    QTest::addColumn<QRect>("window");
    QTest::addColumn<bool>("expected");

    QTest::newRow("empty") << QRect() << false;
    QTest::newRow("exact match") << QRect(0, 0, 1920, 1080) << true;
    QTest::newRow("off by one") << QRect(0, 0, 1920, 1079) << false;
    QTest::newRow("second screen") << QRect(1920, 0, 1920, 1080) << true;
    QTest::newRow("centred window") << QRect(400, 300, 600, 400) << false;
}

void WindowGeometryPredicatesTest::isFullScreen()
{
    QFETCH(QRect, window);
    QFETCH(bool, expected);
    // Every row is valid against the dual-screen set (which contains the single one).
    QCOMPARE(isFullScreenWindow(window, dual()), expected);
}

void WindowGeometryPredicatesTest::isPlasmaPanel_data()
{
    QTest::addColumn<QRect>("window");
    QTest::addColumn<bool>("expected");

    QTest::newRow("empty") << QRect() << false;
    // Thin strips snapped to an edge.
    QTest::newRow("top panel") << QRect(0, 0, 1920, 30) << true;
    QTest::newRow("bottom panel") << QRect(0, 1050, 1920, 30) << true;
    QTest::newRow("left panel") << QRect(0, 0, 40, 1080) << true;
    QTest::newRow("right panel") << QRect(1880, 0, 40, 1080) << true;
    // Touches the top edge but is too thick to be a panel.
    QTest::newRow("thick top band") << QRect(0, 0, 1920, 200) << false;
    // Fills the screen: touches every edge but exceeds the thickness on both axes.
    QTest::newRow("fullscreen not panel") << QRect(0, 0, 1920, 1080) << false;
    // Floating window that touches no edge.
    QTest::newRow("floating") << QRect(500, 500, 100, 50) << false;
}

void WindowGeometryPredicatesTest::isPlasmaPanel()
{
    QFETCH(QRect, window);
    QFETCH(bool, expected);
    QCOMPARE(Latte::WindowSystem::WindowGeometryPredicates::isPlasmaPanel(window, single()), expected);
}

void WindowGeometryPredicatesTest::isSidepanel_data()
{
    QTest::addColumn<QRect>("window");
    QTest::addColumn<bool>("expected");

    // Vertical, thickness in (96,512), taller than 60% of the screen, narrow ratio.
    QTest::newRow("valid sidepanel") << QRect(0, 0, 120, 900) << true;
    // Horizontal orientation is never a side panel.
    QTest::newRow("horizontal") << QRect(0, 0, 900, 120) << false;
    // Too thin (<=96) and too thick (>=512) both fail.
    QTest::newRow("too thin") << QRect(0, 0, 80, 900) << false;
    QTest::newRow("too thick") << QRect(0, 0, 520, 900) << false;
    // Tall enough shape but not long enough versus the screen height.
    QTest::newRow("too short") << QRect(0, 0, 120, 600) << false;
    // Right thickness/length but the width/height ratio is too wide.
    QTest::newRow("ratio too wide") << QRect(0, 0, 500, 900) << false;
    // Its centre is off every screen, so no screen height is available.
    QTest::newRow("off screen") << QRect(5000, 5000, 120, 900) << false;
}

void WindowGeometryPredicatesTest::isSidepanel()
{
    QFETCH(QRect, window);
    QFETCH(bool, expected);
    QCOMPARE(Latte::WindowSystem::WindowGeometryPredicates::isSidepanel(window, single()), expected);
}

void WindowGeometryPredicatesTest::scaledForGlobalScale_data()
{
    QTest::addColumn<QRect>("geometry");
    QTest::addColumn<qreal>("factor");
    QTest::addColumn<QRect>("expected");

    QTest::newRow("identity") << QRect(10, 20, 300, 40) << 1.0 << QRect(10, 20, 300, 40);
    QTest::newRow("double") << QRect(10, 20, 300, 40) << 2.0 << QRect(20, 40, 600, 80);
    // Each of x/y/width/height is rounded on its own, so the scaled rect's right edge
    // (5 + 11 - 1 = 15) is not the scaled original right edge (qRound(9 * 1.5) = 14).
    // Going through QRectF, QRect::scaled() or rounding right()/bottom() lands on 14
    // and shifts X11 geometry by a pixel.
    QTest::newRow("odd rounding") << QRect(3, 5, 7, 9) << 1.5 << QRect(5, 8, 11, 14);
    // A screen left of the primary one, as currentScreenGeometries() reports it.
    QTest::newRow("negative origin") << QRect(-1920, 0, 1920, 1080) << 2.0 << QRect(-3840, 0, 3840, 2160);
    // qRound rounds away from zero, so a negative half goes down, not toward zero.
    QTest::newRow("negative fractional") << QRect(-1281, -3, 1280, 5) << 1.5 << QRect(-1922, -5, 1920, 8);
    // No isEmpty() early-out: a null rect scales like any other, as the call sites expect.
    QTest::newRow("null rect") << QRect() << 2.0 << QRect(0, 0, 0, 0);
}

void WindowGeometryPredicatesTest::scaledForGlobalScale()
{
    QFETCH(QRect, geometry);
    QFETCH(qreal, factor);
    QFETCH(QRect, expected);
    QCOMPARE(Latte::WindowSystem::WindowGeometryPredicates::scaledForGlobalScale(geometry, factor), expected);
}

QTEST_APPLESS_MAIN(WindowGeometryPredicatesTest)
#include "windowgeometrypredicatestest.moc"
