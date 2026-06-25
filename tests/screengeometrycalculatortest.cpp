/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screengeometrycalculator.h"

#include <QObject>
#include <QRect>
#include <QRegion>
#include <QtTest>

using namespace Latte;

//! Fixtures trace the arithmetic of Corona::availableScreen{Rect,Region}WithCriteria
//! against a 1920x1080 screen anchored at the origin (right=1919, bottom=1079).

class ScreenGeometryCalculatorTest : public QObject
{
    Q_OBJECT

private:
    QRect screen() const { return QRect(0, 0, 1920, 1080); }

    //! a footprint that passes every gate (always-visible, on-screen, has visibility)
    ViewFootprint visibleView(Plasma::Types::Location loc, const QRect &geom, int thickness) const
    {
        ViewFootprint fp;
        fp.location = loc;
        fp.geometry = geom;
        fp.normalThickness = thickness;
        fp.visibilityMode = Latte::Types::AlwaysVisible;
        fp.hasVisibility = true;
        fp.behaveAsPlasmaPanel = false;
        fp.formFactor = (loc == Plasma::Types::LeftEdge || loc == Plasma::Types::RightEdge)
                            ? Plasma::Types::Vertical
                            : Plasma::Types::Horizontal;
        fp.alignment = Latte::Types::Center;
        fp.maxLength = 1.0f;
        fp.offset = 0.0f;
        return fp;
    }

private Q_SLOTS:
    void emptyFootprints_returnsStartRect();
    void bottomEdgeNonPanel_reservesThickness();
    void topEdgeNonPanel_reservesThickness();
    void leftEdgeNonPanel_reservesThickness();
    void topAndBottom_accumulate();
    void panelDesktopUse_bottom_usesScreenEdgeMargin();
    void ignoredVisibilityMode_skipped();
    void ignoredEdge_skipped();
    void normalWindowAndNone_autoBlacklisted();
    void offScreenDesktopUse_skipped();
    void region_bottomNonPanel_subtractsFootprint();
};

void ScreenGeometryCalculatorTest::emptyFootprints_returnsStartRect()
{
    QCOMPARE(ScreenGeometryCalculator::availableRect(screen(), screen(), {}, {}, {}, false),
             screen());
}

void ScreenGeometryCalculatorTest::bottomEdgeNonPanel_reservesThickness()
{
    //! bottom dock occupying y=1040..1079, thickness 40
    //! setBottom(qMin(1079, y + h - thickness = 1040+40-40 = 1040)) => bottom 1040
    QList<ViewFootprint> fps{visibleView(Plasma::Types::BottomEdge, QRect(0, 1040, 1920, 40), 40)};

    QCOMPARE(ScreenGeometryCalculator::availableRect(screen(), screen(), fps, {}, {}, false),
             QRect(0, 0, 1920, 1041));
}

void ScreenGeometryCalculatorTest::topEdgeNonPanel_reservesThickness()
{
    //! top dock at y=0, thickness 30: setTop(qMax(0, y + thickness = 30)) => top 30
    QList<ViewFootprint> fps{visibleView(Plasma::Types::TopEdge, QRect(0, 0, 1920, 30), 30)};

    QCOMPARE(ScreenGeometryCalculator::availableRect(screen(), screen(), fps, {}, {}, false),
             QRect(0, 30, 1920, 1050));
}

void ScreenGeometryCalculatorTest::leftEdgeNonPanel_reservesThickness()
{
    //! left dock at x=0, thickness 50: setLeft(qMax(0, x + thickness = 50)) => left 50
    QList<ViewFootprint> fps{visibleView(Plasma::Types::LeftEdge, QRect(0, 0, 50, 1080), 50)};

    QCOMPARE(ScreenGeometryCalculator::availableRect(screen(), screen(), fps, {}, {}, false),
             QRect(50, 0, 1870, 1080));
}

void ScreenGeometryCalculatorTest::topAndBottom_accumulate()
{
    QList<ViewFootprint> fps{
        visibleView(Plasma::Types::TopEdge, QRect(0, 0, 1920, 30), 30),
        visibleView(Plasma::Types::BottomEdge, QRect(0, 1040, 1920, 40), 40)};

    //! top => 30, bottom => 1040 ; height = 1040 - 30 + 1 = 1011
    QCOMPARE(ScreenGeometryCalculator::availableRect(screen(), screen(), fps, {}, {}, false),
             QRect(0, 30, 1920, 1011));
}

void ScreenGeometryCalculatorTest::panelDesktopUse_bottom_usesScreenEdgeMargin()
{
    //! plasma panel + desktopUse: appliedThickness = screenEdgeMargin(10) + normalThickness(40) = 50
    //! setBottom(qMin(1079, screenGeometry.bottom() - 50 = 1029)) => bottom 1029
    ViewFootprint fp = visibleView(Plasma::Types::BottomEdge, QRect(0, 1030, 1920, 50), 40);
    fp.behaveAsPlasmaPanel = true;
    fp.screenEdgeMargin = 10;

    QCOMPARE(ScreenGeometryCalculator::availableRect(screen(), screen(), {fp}, {}, {}, true),
             QRect(0, 0, 1920, 1030));
}

void ScreenGeometryCalculatorTest::ignoredVisibilityMode_skipped()
{
    ViewFootprint fp = visibleView(Plasma::Types::BottomEdge, QRect(0, 1040, 1920, 40), 40);
    fp.visibilityMode = Latte::Types::AutoHide;

    QList<Latte::Types::Visibility> ignoreModes{Latte::Types::AutoHide};

    QCOMPARE(ScreenGeometryCalculator::availableRect(screen(), screen(), {fp}, ignoreModes, {}, false),
             screen());
}

void ScreenGeometryCalculatorTest::ignoredEdge_skipped()
{
    QList<ViewFootprint> fps{visibleView(Plasma::Types::BottomEdge, QRect(0, 1040, 1920, 40), 40)};
    QList<Plasma::Types::Location> ignoreEdges{Plasma::Types::BottomEdge};

    QCOMPARE(ScreenGeometryCalculator::availableRect(screen(), screen(), fps, {}, ignoreEdges, false),
             screen());
}

void ScreenGeometryCalculatorTest::normalWindowAndNone_autoBlacklisted()
{
    //! None and NormalWindow are always blacklisted even with empty ignoreModes
    ViewFootprint normalWin = visibleView(Plasma::Types::BottomEdge, QRect(0, 1040, 1920, 40), 40);
    normalWin.visibilityMode = Latte::Types::NormalWindow;

    ViewFootprint none = visibleView(Plasma::Types::TopEdge, QRect(0, 0, 1920, 30), 30);
    none.visibilityMode = Latte::Types::None;

    QCOMPARE(ScreenGeometryCalculator::availableRect(screen(), screen(), {normalWin, none}, {}, {}, false),
             screen());
}

void ScreenGeometryCalculatorTest::offScreenDesktopUse_skipped()
{
    //! a view sliding off-screen during desktop startup is ignored when desktopUse is set
    ViewFootprint fp = visibleView(Plasma::Types::BottomEdge, QRect(0, 1040, 1920, 40), 40);
    fp.isOffScreen = true;

    QCOMPARE(ScreenGeometryCalculator::availableRect(screen(), screen(), {fp}, {}, {}, true),
             screen());
}

void ScreenGeometryCalculatorTest::region_bottomNonPanel_subtractsFootprint()
{
    //! horizontal centered, maxLength 1.0 => full-width strip subtracted at the bottom
    //! w = 1920, x = center.x()-w/2+1 = 959-960+1 = 0
    //! y = geometry.bottom() - thickness + 1 = 1079 - 40 + 1 = 1040
    //! subtract QRect(0, 1040, 1920, 40) from the screen => QRect(0,0,1920,1040)
    QList<ViewFootprint> fps{visibleView(Plasma::Types::BottomEdge, QRect(0, 1040, 1920, 40), 40)};

    QRegion result = ScreenGeometryCalculator::availableRegion(screen(), screen(), fps, {}, {}, false);

    QCOMPARE(result, QRegion(QRect(0, 0, 1920, 1040)));
}

QTEST_MAIN(ScreenGeometryCalculatorTest)
#include "screengeometrycalculatortest.moc"
