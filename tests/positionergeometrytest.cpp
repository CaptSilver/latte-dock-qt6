/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// This test uses an instrument-first approach: the first build intentionally
// fails (header not yet written), then the header is written and the test goes
// green. The expected values below were derived by running the pure functions
// and printing actual output, then cross-checking against the original
// positioner.cpp arithmetic.

#include "positionergeometry.h"

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QRegion>
#include <QSize>
#include <QtTest>

using namespace Latte::ViewPart::PositionerGeometry;

class PositionerGeometryTest : public QObject
{
    Q_OBJECT

private:
    QRect screen1080p() const { return QRect(0, 0, 1920, 1080); }

    ViewGeometryInputs baseInputs() const
    {
        ViewGeometryInputs in;
        in.location = Plasma::Types::BottomEdge;
        in.formFactor = Plasma::Types::Horizontal;
        in.alignment = Latte::Types::Center;
        in.behaveAsPlasmaPanel = false;
        in.normalThickness = 56;
        in.maxThickness = 96;
        in.maxNormalThickness = 96;
        in.innerShadow = 0;
        in.screenEdgeMargin = 0;
        in.editThickness = 128;
        in.viewWidth = 1920;
        in.viewHeight = 56;
        in.maxLength = 1.0f;
        in.offset = 0.0f;
        in.slideOffset = 0;
        return in;
    }

private Q_SLOTS:
    // dockPosition — non-panel, all 4 edges
    void dockPosition_bottomEdge_nonPanel();
    void dockPosition_topEdge_nonPanel();
    void dockPosition_leftEdge_nonPanel();
    void dockPosition_rightEdge_nonPanel();

    // dockPosition — panel alignment
    void dockPosition_bottomPanel_leftAlignment();
    void dockPosition_bottomPanel_rightAlignment();
    void dockPosition_bottomPanel_centerAlignment();

    // dockPosition — screenEdgeMargin + slideOffset correction
    void dockPosition_bottomPanel_screenEdgeMarginSlideOffset();

    // windowSize
    void windowSize_horizontalNonPanel();
    void windowSize_horizontalPanel();
    void windowSize_verticalNonPanel();
    void windowSize_verticalPanel();
    void windowSize_degenerateClamp();

    // maximumNormalGeometry
    void maximumNormalGeometry_leftEdge();
    void maximumNormalGeometry_rightEdge();
    void maximumNormalGeometry_defaultBottomEdge();
    void maximumNormalGeometry_multiMonitorOffset();

    // canvasGeometry
    void canvasGeometry_topEdge();
    void canvasGeometry_bottomEdge();
    void canvasGeometry_leftEdge();
    void canvasGeometry_rightEdge();

    // slideEdge
    void slideEdge_topEdge();
    void slideEdge_rightEdge();
    void slideEdge_bottomEdge();
    void slideEdge_leftEdge();
    void slideEdge_floating_returnsNone();

    // forcedBorders
    void forcedBorders_bothOff_whenScreenEdgesMatch();
    void forcedBorders_top_forced();
    void forcedBorders_bottom_forced();
    void forcedBorders_topNotForced_whenRegionOccupied();
};

// ---------------------------------------------------------------------------
// dockPosition — non-panel
// ---------------------------------------------------------------------------

void PositionerGeometryTest::dockPosition_bottomEdge_nonPanel()
{
    // Non-panel bottom: {screenGeometry.x(), screenGeometry.y() + screenGeometry.height() - viewHeight}
    // = {0, 0 + 1080 - 56} = {0, 1024}
    ViewGeometryInputs in = baseInputs();
    in.location = Plasma::Types::BottomEdge;
    in.viewHeight = 56;

    QPoint pos = dockPosition(in, screen1080p());
    QCOMPARE(pos, QPoint(0, 1024));
}

void PositionerGeometryTest::dockPosition_topEdge_nonPanel()
{
    // Non-panel top: {screenGeometry.x(), screenGeometry.y()} = {0, 0}
    ViewGeometryInputs in = baseInputs();
    in.location = Plasma::Types::TopEdge;
    in.viewHeight = 56;

    QPoint pos = dockPosition(in, screen1080p());
    QCOMPARE(pos, QPoint(0, 0));
}

void PositionerGeometryTest::dockPosition_leftEdge_nonPanel()
{
    // Non-panel left: {availableScreenRect.x(), availableScreenRect.y()} = {0, 0}
    ViewGeometryInputs in = baseInputs();
    in.location = Plasma::Types::LeftEdge;
    in.formFactor = Plasma::Types::Vertical;

    QPoint pos = dockPosition(in, screen1080p());
    QCOMPARE(pos, QPoint(0, 0));
}

void PositionerGeometryTest::dockPosition_rightEdge_nonPanel()
{
    // Non-panel right: {availableScreenRect.right() - viewWidth + 1, availableScreenRect.y()}
    // = {1919 - 96 + 1, 0} = {1824, 0}
    ViewGeometryInputs in = baseInputs();
    in.location = Plasma::Types::RightEdge;
    in.formFactor = Plasma::Types::Vertical;
    in.viewWidth = 96;

    QPoint pos = dockPosition(in, screen1080p());
    QCOMPARE(pos, QPoint(1824, 0));
}

// ---------------------------------------------------------------------------
// dockPosition — panel alignment cases (BottomEdge horizontal panel)
// ---------------------------------------------------------------------------

void PositionerGeometryTest::dockPosition_bottomPanel_leftAlignment()
{
    // Panel bottom, Left alignment, offset=0.1, maxLength=0.8
    // gap(1920) = int(1920 * 0.1) = 192
    // x = screenGeometry.x() + gap(width) = 0 + 192 = 192
    // cleanThickness = normalThickness - innerShadow = 56 - 0 = 56
    // screenEdgeMargin_applied = 0 (screenEdgeMargin=0)
    // y = screenGeometry.y() + screenGeometry.height() - cleanThickness - screenEdgeMargin_applied
    //   = 0 + 1080 - 56 - 0 = 1024
    ViewGeometryInputs in = baseInputs();
    in.location = Plasma::Types::BottomEdge;
    in.behaveAsPlasmaPanel = true;
    in.alignment = Latte::Types::Left;
    in.offset = 0.1f;
    in.maxLength = 0.8f;
    in.normalThickness = 56;
    in.innerShadow = 0;

    QPoint pos = dockPosition(in, screen1080p());
    QCOMPARE(pos, QPoint(192, 1024));
}

void PositionerGeometryTest::dockPosition_bottomPanel_rightAlignment()
{
    // Panel bottom, Right alignment, offset=0.1, maxLength=0.8
    // gapReversed(1920) = int(1920 - 1920*0.8 - gap(1920)) = int(1920 - 1536 - 192) = 192
    // x = screenGeometry.x() + gapReversed(width) + 1 = 0 + 192 + 1 = 193
    // y = 0 + 1080 - 56 - 0 = 1024
    ViewGeometryInputs in = baseInputs();
    in.location = Plasma::Types::BottomEdge;
    in.behaveAsPlasmaPanel = true;
    in.alignment = Latte::Types::Right;
    in.offset = 0.1f;
    in.maxLength = 0.8f;
    in.normalThickness = 56;
    in.innerShadow = 0;

    QPoint pos = dockPosition(in, screen1080p());
    QCOMPARE(pos, QPoint(193, 1024));
}

void PositionerGeometryTest::dockPosition_bottomPanel_centerAlignment()
{
    // Panel bottom, Center alignment, offset=0.0, maxLength=0.8
    // gapCentered(1920) = int(1920 * ((1-0.8f)/2) + 1920 * 0.0)
    // Due to float32 precision, 1.0f - 0.8f is slightly less than 0.2f, so
    // int(1920 * ~0.09999...) = 191 (not 192 as naive decimal arithmetic suggests).
    // This matches the live positioner.cpp behaviour — no correction needed.
    // x = 0 + 191 = 191, y = 0 + 1080 - 56 - 0 = 1024
    ViewGeometryInputs in = baseInputs();
    in.location = Plasma::Types::BottomEdge;
    in.behaveAsPlasmaPanel = true;
    in.alignment = Latte::Types::Center;
    in.offset = 0.0f;
    in.maxLength = 0.8f;
    in.normalThickness = 56;
    in.innerShadow = 0;

    QPoint pos = dockPosition(in, screen1080p());
    QCOMPARE(pos, QPoint(191, 1024));
}

void PositionerGeometryTest::dockPosition_bottomPanel_screenEdgeMarginSlideOffset()
{
    // Panel bottom, screenEdgeMargin=10, slideOffset=5
    // applied screenEdgeMargin = 10 - abs(5) = 5
    // cleanThickness = 56 - 0 = 56
    // y = 1080 - 56 - 5 = 1019
    // Center alignment, offset=0, maxLength=1.0:
    // gapCentered(1920) = int(1920 * ((1-1.0)/2) + 0) = 0
    // x = 0 + 0 = 0
    ViewGeometryInputs in = baseInputs();
    in.location = Plasma::Types::BottomEdge;
    in.behaveAsPlasmaPanel = true;
    in.alignment = Latte::Types::Center;
    in.screenEdgeMargin = 10;
    in.slideOffset = 5;
    in.normalThickness = 56;
    in.innerShadow = 0;
    in.offset = 0.0f;
    in.maxLength = 1.0f;

    QPoint pos = dockPosition(in, screen1080p());
    QCOMPARE(pos, QPoint(0, 1019));
}

// ---------------------------------------------------------------------------
// windowSize
// ---------------------------------------------------------------------------

void PositionerGeometryTest::windowSize_horizontalNonPanel()
{
    // Horizontal non-panel: width=screenSize.width(), height=maxThickness
    // = {1920, 96}
    ViewGeometryInputs in = baseInputs();
    in.formFactor = Plasma::Types::Horizontal;
    in.behaveAsPlasmaPanel = false;
    in.maxThickness = 96;

    QSize sz = windowSize(in, screen1080p(), screen1080p().size());
    QCOMPARE(sz, QSize(1920, 96));
}

void PositionerGeometryTest::windowSize_horizontalPanel()
{
    // Horizontal panel: width=int(maxLength*screenSize.width()), height=normalThickness
    // = {int(0.8*1920), 56} = {1536, 56}
    ViewGeometryInputs in = baseInputs();
    in.formFactor = Plasma::Types::Horizontal;
    in.behaveAsPlasmaPanel = true;
    in.maxLength = 0.8f;
    in.normalThickness = 56;

    QSize sz = windowSize(in, screen1080p(), screen1080p().size());
    QCOMPARE(sz, QSize(1536, 56));
}

void PositionerGeometryTest::windowSize_verticalNonPanel()
{
    // Vertical non-panel: width=maxThickness, height=availableScreenRect.height()
    // = {96, 1080}
    ViewGeometryInputs in = baseInputs();
    in.formFactor = Plasma::Types::Vertical;
    in.behaveAsPlasmaPanel = false;
    in.maxThickness = 96;

    QSize sz = windowSize(in, screen1080p(), screen1080p().size());
    QCOMPARE(sz, QSize(96, 1080));
}

void PositionerGeometryTest::windowSize_verticalPanel()
{
    // Vertical panel: width=normalThickness, height=int(maxLength*availableScreenRect.height())
    // = {56, int(0.8*1080)} = {56, 864}
    ViewGeometryInputs in = baseInputs();
    in.formFactor = Plasma::Types::Vertical;
    in.behaveAsPlasmaPanel = true;
    in.normalThickness = 56;
    in.maxLength = 0.8f;

    QSize sz = windowSize(in, screen1080p(), screen1080p().size());
    QCOMPARE(sz, QSize(56, 864));
}

void PositionerGeometryTest::windowSize_degenerateClamp()
{
    // maxThickness=0 for non-panel horizontal: size=(1920, 0) -> clamped to (1920, 1)
    ViewGeometryInputs in = baseInputs();
    in.formFactor = Plasma::Types::Horizontal;
    in.behaveAsPlasmaPanel = false;
    in.maxThickness = 0;

    QSize sz = windowSize(in, screen1080p(), screen1080p().size());
    QCOMPARE(sz.width(), 1920);
    QCOMPARE(sz.height(), 1);
}

// ---------------------------------------------------------------------------
// maximumNormalGeometry
// ---------------------------------------------------------------------------

void PositionerGeometryTest::maximumNormalGeometry_leftEdge()
{
    // LeftEdge: xPos=currentScrGeometry.x()=0, yPos=currentScrGeometry.y()=0
    // maxWidth=50, maxHeight=1080
    // => QRect(0, 0, 50, 1080)
    QRect result = maximumNormalGeometry(Plasma::Types::LeftEdge, 50, screen1080p());
    QCOMPARE(result, QRect(0, 0, 50, 1080));
}

void PositionerGeometryTest::maximumNormalGeometry_rightEdge()
{
    // RightEdge: xPos=currentScrGeometry.right()-maxWidth+1 = 1919-50+1 = 1870
    // yPos=0, maxWidth=50, maxHeight=1080
    // => QRect(1870, 0, 50, 1080)
    QRect result = maximumNormalGeometry(Plasma::Types::RightEdge, 50, screen1080p());
    QCOMPARE(result, QRect(1870, 0, 50, 1080));
}

void PositionerGeometryTest::maximumNormalGeometry_defaultBottomEdge()
{
    // BottomEdge (default): QRect(0, 0, maxNormalThickness, maxHeight)
    // = QRect(0, 0, 50, 1080)
    QRect result = maximumNormalGeometry(Plasma::Types::BottomEdge, 50, screen1080p());
    QCOMPARE(result, QRect(0, 0, 50, 1080));
}

void PositionerGeometryTest::maximumNormalGeometry_multiMonitorOffset()
{
    // Screen at (1920, 0): LeftEdge xPos = 1920
    QRect screen2(1920, 0, 1920, 1080);
    QRect result = maximumNormalGeometry(Plasma::Types::LeftEdge, 50, screen2);
    QCOMPARE(result.x(), 1920);
    QCOMPARE(result.y(), 0);
    QCOMPARE(result.width(), 50);
    QCOMPARE(result.height(), 1080);
}

// ---------------------------------------------------------------------------
// canvasGeometry
// ---------------------------------------------------------------------------

void PositionerGeometryTest::canvasGeometry_topEdge()
{
    // TopEdge: canvas at (screenGeometry.x(), screenGeometry.y()), width=width, height=thickness
    // screenGeometry=1080p, available=same
    // => QRect(0, 0, 1920, 128)
    QRect canvas = canvasGeometry(Plasma::Types::TopEdge, Plasma::Types::Horizontal,
                                  128, screen1080p(), screen1080p());
    QCOMPARE(canvas, QRect(0, 0, 1920, 128));
}

void PositionerGeometryTest::canvasGeometry_bottomEdge()
{
    // BottomEdge: canvas at (screenGeometry.x(), screenGeometry.bottom() - thickness + 1)
    // = (0, 1079 - 128 + 1) = (0, 952)
    // width=1920, height=128
    // => QRect(0, 952, 1920, 128)
    QRect canvas = canvasGeometry(Plasma::Types::BottomEdge, Plasma::Types::Horizontal,
                                  128, screen1080p(), screen1080p());
    QCOMPARE(canvas, QRect(0, 952, 1920, 128));
}

void PositionerGeometryTest::canvasGeometry_leftEdge()
{
    // LeftEdge: canvas at (availableScreenRect.x(), availableScreenRect.y())
    // = (0, 0), width=thickness, height=availableScreenRect.height()
    // => QRect(0, 0, 128, 1080)
    QRect canvas = canvasGeometry(Plasma::Types::LeftEdge, Plasma::Types::Vertical,
                                  128, screen1080p(), screen1080p());
    QCOMPARE(canvas, QRect(0, 0, 128, 1080));
}

void PositionerGeometryTest::canvasGeometry_rightEdge()
{
    // RightEdge: canvas at (screenGeometry.right() - thickness + 1, availableScreenRect.y())
    // = (1919 - 128 + 1, 0) = (1792, 0)
    // width=thickness=128, height=availableScreenRect.height()=1080
    // => QRect(1792, 0, 128, 1080)
    QRect canvas = canvasGeometry(Plasma::Types::RightEdge, Plasma::Types::Vertical,
                                  128, screen1080p(), screen1080p());
    QCOMPARE(canvas, QRect(1792, 0, 128, 1080));
}

// ---------------------------------------------------------------------------
// slideEdge
// ---------------------------------------------------------------------------

void PositionerGeometryTest::slideEdge_topEdge()
{
    QCOMPARE(slideEdge(Plasma::Types::TopEdge), SlideEdge::Top);
}

void PositionerGeometryTest::slideEdge_rightEdge()
{
    QCOMPARE(slideEdge(Plasma::Types::RightEdge), SlideEdge::Right);
}

void PositionerGeometryTest::slideEdge_bottomEdge()
{
    QCOMPARE(slideEdge(Plasma::Types::BottomEdge), SlideEdge::Bottom);
}

void PositionerGeometryTest::slideEdge_leftEdge()
{
    QCOMPARE(slideEdge(Plasma::Types::LeftEdge), SlideEdge::Left);
}

void PositionerGeometryTest::slideEdge_floating_returnsNone()
{
    // Floating is not resolved here (caller responsibility); pure function returns None.
    QCOMPARE(slideEdge(Plasma::Types::Floating), SlideEdge::None);
}

// ---------------------------------------------------------------------------
// forcedBorders
// ---------------------------------------------------------------------------

void PositionerGeometryTest::forcedBorders_bothOff_whenScreenEdgesMatch()
{
    // availableScreenRect top matches screen top, bottom matches screen bottom
    // => both borders off
    QRect screen = screen1080p();
    // vertical dock available rect = whole screen (no other docks squeezing it)
    QRegion region(screen);
    ForcedBorders fb = forcedBorders(Plasma::Types::LeftEdge, 1, screen, screen, region);
    QVERIFY(!fb.top);
    QVERIFY(!fb.bottom);
}

void PositionerGeometryTest::forcedBorders_top_forced()
{
    // availableScreenRect.top() != screen.top() (a top dock reserves 40px)
    // and the region above is free => top=true
    QRect screen = screen1080p();
    QRect available(0, 40, 50, 1040); // left-edge dock, top cropped by 40
    // region = full screen minus no other docks (the top dock is in the other axis)
    QRegion region(screen);
    // edgeMargin=1, LeftEdge so x = screen.x() = 0
    // fitInRegion = QRect(0, available.y()-1, 1, 1) = QRect(0, 39, 1, 1)
    // that rect IS in the region (no dock blocks y=39) => subtracted.isNull() => top=true
    ForcedBorders fb = forcedBorders(Plasma::Types::LeftEdge, 1, screen, available, region);
    QVERIFY(fb.top);
    QVERIFY(!fb.bottom);
}

void PositionerGeometryTest::forcedBorders_bottom_forced()
{
    // availableScreenRect.bottom() != screen.bottom() (a bottom dock reserves 40px)
    // and the region below is free => bottom=true
    QRect screen = screen1080p();
    QRect available(0, 0, 50, 1040); // left-edge dock, bottom cropped
    QRegion region(screen);
    // edgeMargin=1, LeftEdge so x = screen.x() = 0
    // fitInRegion = QRect(0, available.bottom()+1, 1, 1) = QRect(0, 1040, 1, 1)
    // that rect IS in the region (screen extends to 1079) => bottom=true
    ForcedBorders fb = forcedBorders(Plasma::Types::LeftEdge, 1, screen, available, region);
    QVERIFY(!fb.top);
    QVERIFY(fb.bottom);
}

void PositionerGeometryTest::forcedBorders_topNotForced_whenRegionOccupied()
{
    // availableScreenRect.top() != screen.top(), but the strip above is occupied
    // (another dock sits there and removed it from the region) => top=false
    QRect screen = screen1080p();
    QRect available(0, 40, 50, 1040);
    // A top dock at (0,0,1920,40) is already removed from the region
    QRegion region = QRegion(screen).subtracted(QRect(0, 0, 1920, 40));
    // edgeMargin=1, LeftEdge => x=0
    // fitInRegion = QRect(0, 39, 1, 1) — inside the subtracted band => NOT in region
    // subtracted != isNull => top=false
    ForcedBorders fb = forcedBorders(Plasma::Types::LeftEdge, 1, screen, available, region);
    QVERIFY(!fb.top);
}

QTEST_MAIN(PositionerGeometryTest)
#include "positionergeometrytest.moc"
