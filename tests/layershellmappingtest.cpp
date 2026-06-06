/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../app/wm/waylandlayershell.h"

#include <QTest>
#include <QObject>

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

QTEST_MAIN(LayerShellMappingTest)
#include "layershellmappingtest.moc"
