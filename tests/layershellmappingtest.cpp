/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../app/wm/waylandlayershell.h"

#include <QTest>
#include <QObject>
#include <QRegion>

using namespace Latte::WindowSystem;
using LSW = LayerShellQt::Window;

class LayerShellMappingTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void anchors_bottom_alignments();
    void anchors_left_alignments();
    void layer_byMode();
    void exclusiveZone_byLocation();
    void seededSize_singleEdgeNeedsValidSize();
    void canvasPlacement_byEdge();
    void exclusiveEdge_isAlwaysAnchored();
    void canvasInputRegion_plainEditModeIsFullyInteractive();
    void canvasInputRegion_configureModeIsClickThroughOverDock();
    void canvasInputRegion_configureModeKeepsChromeInteractive();
};

void LayerShellMappingTest::anchors_bottom_alignments()
{
    QCOMPARE(LayerShell::anchorsFor(Plasma::Types::BottomEdge, Latte::Types::Center),
             LSW::Anchors(LSW::AnchorBottom));
    QCOMPARE(LayerShell::anchorsFor(Plasma::Types::BottomEdge, Latte::Types::Left),
             LSW::Anchors(LSW::AnchorBottom | LSW::AnchorLeft));
    QCOMPARE(LayerShell::anchorsFor(Plasma::Types::BottomEdge, Latte::Types::Right),
             LSW::Anchors(LSW::AnchorBottom | LSW::AnchorRight));
    QCOMPARE(LayerShell::anchorsFor(Plasma::Types::BottomEdge, Latte::Types::Justify),
             LSW::Anchors(LSW::AnchorBottom | LSW::AnchorLeft | LSW::AnchorRight));
}

void LayerShellMappingTest::anchors_left_alignments()
{
    QCOMPARE(LayerShell::anchorsFor(Plasma::Types::LeftEdge, Latte::Types::Center),
             LSW::Anchors(LSW::AnchorLeft));
    QCOMPARE(LayerShell::anchorsFor(Plasma::Types::LeftEdge, Latte::Types::Top),
             LSW::Anchors(LSW::AnchorLeft | LSW::AnchorTop));
    QCOMPARE(LayerShell::anchorsFor(Plasma::Types::LeftEdge, Latte::Types::Bottom),
             LSW::Anchors(LSW::AnchorLeft | LSW::AnchorBottom));
    QCOMPARE(LayerShell::anchorsFor(Plasma::Types::LeftEdge, Latte::Types::Justify),
             LSW::Anchors(LSW::AnchorLeft | LSW::AnchorTop | LSW::AnchorBottom));
}

void LayerShellMappingTest::layer_byMode()
{
    QCOMPARE(LayerShell::layerFor(Latte::Types::WindowsCanCover), LSW::LayerBottom);
    QCOMPARE(LayerShell::layerFor(Latte::Types::WindowsAlwaysCover), LSW::LayerBottom);
    QCOMPARE(LayerShell::layerFor(Latte::Types::WindowsGoBelow), LSW::LayerBottom);
    QCOMPARE(LayerShell::layerFor(Latte::Types::AlwaysVisible), LSW::LayerTop);
    QCOMPARE(LayerShell::layerFor(Latte::Types::AutoHide), LSW::LayerTop);
    QCOMPARE(LayerShell::layerFor(Latte::Types::NormalWindow), LSW::LayerTop);
}

void LayerShellMappingTest::exclusiveZone_byLocation()
{
    QCOMPARE(LayerShell::exclusiveZoneFor(QRect(0, 1040, 1920, 40), Plasma::Types::BottomEdge), 40);
    QCOMPARE(LayerShell::exclusiveZoneFor(QRect(0, 0, 48, 1080), Plasma::Types::LeftEdge), 48);
    QCOMPARE(LayerShell::exclusiveZoneFor(QRect(), Plasma::Types::BottomEdge), 0);
}

void LayerShellMappingTest::seededSize_singleEdgeNeedsValidSize()
{
    const QSize screen(1920, 1080);

    //! A Center-aligned bottom dock anchors to the single bottom edge. A 0x0 window (e.g. an
    //! as-yet-unsized edge helper) must be seeded to a legal size or the compositor rejects the
    //! surface: length axis -> full screen width, thickness axis -> 1px. (Regression: this exact
    //! 0-size single-edge surface aborted the client with a zwlr_layer_surface protocol error.)
    const auto bottomCenter = LayerShell::anchorsFor(Plasma::Types::BottomEdge, Latte::Types::Center);
    QCOMPARE(LayerShell::seededLayerSize(bottomCenter, Plasma::Types::BottomEdge, QSize(0, 0), screen),
             QSize(1920, 1));

    //! An already-sized window is left untouched.
    QCOMPARE(LayerShell::seededLayerSize(bottomCenter, Plasma::Types::BottomEdge, QSize(800, 48), screen),
             QSize(800, 48));

    //! A Justify dock spans left+right, so a 0 width is legal (the compositor stretches it) and must
    //! NOT be overwritten; only the unspanned thickness axis is touched.
    const auto bottomJustify = LayerShell::anchorsFor(Plasma::Types::BottomEdge, Latte::Types::Justify);
    QCOMPARE(LayerShell::seededLayerSize(bottomJustify, Plasma::Types::BottomEdge, QSize(0, 48), screen),
             QSize(0, 48));

    //! A Center-aligned left dock anchors to the single left edge: thickness -> 1px, length axis
    //! (vertical) -> full screen height.
    const auto leftCenter = LayerShell::anchorsFor(Plasma::Types::LeftEdge, Latte::Types::Center);
    QCOMPARE(LayerShell::seededLayerSize(leftCenter, Plasma::Types::LeftEdge, QSize(0, 0), screen),
             QSize(1, 1080));
}

void LayerShellMappingTest::canvasPlacement_byEdge()
{
    const QRect screen(0, 0, 1920, 1080);

    //! The edit-mode canvas must overlay the dock's canvasGeometry exactly, sitting ON the edge
    //! (margin 0), unlike a config view which is pushed OFF the edge. Horizontal docks span the
    //! full screen width, so the canvas anchors both length edges (left+right) and the dock edge.
    const auto bottom = LayerShell::canvasPlacement(Plasma::Types::BottomEdge, QRect(0, 1040, 1920, 40), screen);
    QCOMPARE(bottom.anchors, LSW::Anchors(LSW::AnchorBottom | LSW::AnchorLeft | LSW::AnchorRight));
    QCOMPARE(bottom.margins, QMargins(0, 0, 0, 0));

    const auto top = LayerShell::canvasPlacement(Plasma::Types::TopEdge, QRect(0, 0, 1920, 40), screen);
    QCOMPARE(top.anchors, LSW::Anchors(LSW::AnchorTop | LSW::AnchorLeft | LSW::AnchorRight));
    QCOMPARE(top.margins, QMargins(0, 0, 0, 0));

    //! Vertical docks don't span the full screen height (the canvas starts at the available area's
    //! top, e.g. below a top panel at y=100). Anchor the dock edge + top, then push down with a top
    //! margin = canvasGeometry.y() - screen.y(); the explicit height carries the rest. A single
    //! perpendicular anchor would NOT work here — a layer-shell margin only bites on an anchored edge.
    const auto left = LayerShell::canvasPlacement(Plasma::Types::LeftEdge, QRect(0, 100, 48, 980), screen);
    QCOMPARE(left.anchors, LSW::Anchors(LSW::AnchorLeft | LSW::AnchorTop));
    QCOMPARE(left.margins, QMargins(0, 100, 0, 0));

    const auto right = LayerShell::canvasPlacement(Plasma::Types::RightEdge, QRect(1872, 200, 48, 880), screen);
    QCOMPARE(right.anchors, LSW::Anchors(LSW::AnchorRight | LSW::AnchorTop));
    QCOMPARE(right.margins, QMargins(0, 200, 0, 0));
}

void LayerShellMappingTest::exclusiveEdge_isAlwaysAnchored()
{
    //! The dock re-asserts its exclusive edge (edgeFor) right after applying the anchors. That is
    //! only legal if the edge is one of the anchors for EVERY location/alignment, otherwise the
    //! compositor kills the surface with "exclusive edge is not of the anchors". (The edit-mode
    //! canvas hit exactly that error by keeping a strut edge under multi-edge overlay anchors that
    //! no longer contained it — fixed by clearing the canvas's exclusive edge entirely.)
    const QList<Plasma::Types::Location> locations{
        Plasma::Types::TopEdge, Plasma::Types::BottomEdge, Plasma::Types::LeftEdge, Plasma::Types::RightEdge};
    const QList<Latte::Types::Alignment> alignments{
        Latte::Types::Center, Latte::Types::Left, Latte::Types::Right,
        Latte::Types::Top, Latte::Types::Bottom, Latte::Types::Justify};

    for (const auto location : locations) {
        for (const auto alignment : alignments) {
            const LSW::Anchors anchors = LayerShell::anchorsFor(location, alignment);
            QVERIFY2(anchors.testFlag(LayerShell::edgeFor(location)),
                     qPrintable(QStringLiteral("exclusive edge not anchored for location=%1 alignment=%2")
                                    .arg(int(location)).arg(int(alignment))));
        }
    }
}

void LayerShellMappingTest::canvasInputRegion_plainEditModeIsFullyInteractive()
{
    //! Plain edit mode (NOT configuring applets): the canvas legitimately owns the whole surface so
    //! the mouse wheel changes background opacity and the max-length ruler is draggable everywhere.
    //! The input region must cover the entire canvas, including the centre over the dock.
    const QSize canvas(1920, 40);
    const QRegion region = LayerShell::canvasInputRegion(false, canvas, QRect());

    QCOMPARE(region, QRegion(QRect(0, 0, 1920, 40)));
    QVERIFY2(region.contains(QPoint(960, 20)),
             "plain edit mode: the canvas must catch input over the dock (wheel->opacity, ruler)");
}

void LayerShellMappingTest::canvasInputRegion_configureModeIsClickThroughOverDock()
{
    //! Configure-applets mode: the canvas sits ON TOP of the dock, so with a full input region every
    //! right-click/drag/wheel hits the grid instead of the widgets (the reported bug). With no
    //! interactive chrome the canvas must catch NOTHING over its own surface so those events reach the
    //! dock beneath. Crucially it must do that with a deliberately off-surface region, NOT an empty
    //! QRegion: the Qt6 wayland plugin maps QWindow::setMask() to the wl_surface input region, and an
    //! empty mask there means the INFINITE (grab-all) region -- the exact opposite of click-through
    //! (qwaylandwindow_p.h: "Empty QRegion maps to 'infinite' input region").
    const QSize canvas(1920, 40);
    const QRegion region = LayerShell::canvasInputRegion(true, canvas, QRect());

    QVERIFY2(region.intersected(QRect(QPoint(0, 0), canvas)).isEmpty(),
             "configure-applets mode: the canvas must catch no on-surface pixel so right-click/drag/"
             "remove reach the dock, not the edit grid");
    QVERIFY2(!region.isEmpty(),
             "click-through must use an off-surface region, not an empty QRegion (empty == infinite/"
             "grab-all on the Qt6 wayland plugin)");
    QVERIFY(!region.contains(QPoint(960, 20)));
}

void LayerShellMappingTest::canvasInputRegion_configureModeKeepsChromeInteractive()
{
    //! Configure-applets mode with interactive chrome (e.g. an 8px max-length ruler strip at the canvas
    //! edge): the chrome keeps catching input while the rest of the canvas over the dock stays
    //! click-through. This lets the ruler work without re-blocking the widgets.
    const QSize canvas(1920, 40);
    const QRect chrome(0, 0, 1920, 8);
    const QRegion region = LayerShell::canvasInputRegion(true, canvas, chrome);

    QVERIFY2(region.contains(QPoint(960, 4)),
             "configure-applets mode: the kept-interactive chrome (the rearrange/exit toggle rect) must catch input");
    QVERIFY2(!region.contains(QPoint(960, 30)),
             "configure-applets mode: the dock area outside the toggle rect must be click-through");
}

QTEST_MAIN(LayerShellMappingTest)
#include "layershellmappingtest.moc"
