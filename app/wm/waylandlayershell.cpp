/*
    SPDX-FileCopyrightText: 2026 Latte Dingo
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandlayershell.h"

#include <QMargins>
#include <QScreen>
#include <QWindow>

namespace Latte {
namespace WindowSystem {
namespace LayerShell {

using LSW = LayerShellQt::Window;

LSW::Anchors anchorsFor(Plasma::Types::Location location, Latte::Types::Alignment alignment)
{
    LSW::Anchors anchors;
    const bool horizontal = (location == Plasma::Types::TopEdge || location == Plasma::Types::BottomEdge);

    switch (location) {
    case Plasma::Types::TopEdge:    anchors = LSW::AnchorTop;    break;
    case Plasma::Types::BottomEdge: anchors = LSW::AnchorBottom; break;
    case Plasma::Types::LeftEdge:   anchors = LSW::AnchorLeft;   break;
    case Plasma::Types::RightEdge:  anchors = LSW::AnchorRight;  break;
    default:                        anchors = LSW::AnchorBottom; break;
    }

    const LSW::Anchor nearAnchor = horizontal ? LSW::AnchorLeft : LSW::AnchorTop;
    const LSW::Anchor farAnchor  = horizontal ? LSW::AnchorRight : LSW::AnchorBottom;

    switch (alignment) {
    case Latte::Types::Justify:
        anchors |= nearAnchor;
        anchors |= farAnchor;
        break;
    case Latte::Types::Left:   // horizontal docks
    case Latte::Types::Top:    // vertical docks
        anchors |= nearAnchor;
        break;
    case Latte::Types::Right:  // horizontal docks
    case Latte::Types::Bottom: // vertical docks
        anchors |= farAnchor;
        break;
    case Latte::Types::Center:
    default:
        break; // single-edge anchor -> compositor centres along the length axis
    }

    return anchors;
}

LSW::Layer layerFor(Latte::Types::Visibility mode)
{
    switch (mode) {
    case Latte::Types::WindowsCanCover:
    case Latte::Types::WindowsAlwaysCover:
    case Latte::Types::WindowsGoBelow:
        return LSW::LayerBottom;
    default:
        return LSW::LayerTop;
    }
}

LSW::Anchor edgeFor(Plasma::Types::Location location)
{
    switch (location) {
    case Plasma::Types::TopEdge:    return LSW::AnchorTop;
    case Plasma::Types::BottomEdge: return LSW::AnchorBottom;
    case Plasma::Types::LeftEdge:   return LSW::AnchorLeft;
    case Plasma::Types::RightEdge:  return LSW::AnchorRight;
    default:                        return LSW::AnchorBottom;
    }
}

int exclusiveZoneFor(const QRect &strutRect, Plasma::Types::Location location)
{
    if (strutRect.isEmpty()) {
        return 0;
    }
    switch (location) {
    case Plasma::Types::TopEdge:
    case Plasma::Types::BottomEdge:
        return strutRect.height();
    case Plasma::Types::LeftEdge:
    case Plasma::Types::RightEdge:
        return strutRect.width();
    default:
        return 0;
    }
}

QSize seededLayerSize(LSW::Anchors anchors, Plasma::Types::Location location,
                      const QSize &currentSize, const QSize &screenSize)
{
    const bool horizontal = (location == Plasma::Types::TopEdge || location == Plasma::Types::BottomEdge);
    const bool spansH = anchors.testFlag(LSW::AnchorLeft) && anchors.testFlag(LSW::AnchorRight);
    const bool spansV = anchors.testFlag(LSW::AnchorTop) && anchors.testFlag(LSW::AnchorBottom);

    int w = currentSize.width();
    int h = currentSize.height();

    //! Only seed an axis the anchors do NOT span (a spanned axis may legally be 0 — the compositor
    //! stretches it). The length axis takes the screen extent; the thickness axis a 1px minimum.
    if (!spansH && w <= 0) {
        w = horizontal ? screenSize.width() : 1;
    }
    if (!spansV && h <= 0) {
        h = horizontal ? 1 : screenSize.height();
    }

    return QSize(w, h);
}

void updateAnchoring(QWindow *window, QScreen *screen,
                     Plasma::Types::Location location, Latte::Types::Alignment alignment)
{
    LSW *ls = LSW::get(window);
    if (!ls) {
        return;
    }

    const LSW::Anchors anchors = anchorsFor(location, alignment);

    //! wlr-layer-shell rejects a surface whose size is 0 on an axis its anchors do not span
    //! (e.g. a single-edge-anchored Center dock, or an as-yet-unsized edge helper). Seed a legal
    //! initial size so the first commit succeeds; the window's own geometry management resizes it
    //! immediately after. seededLayerSize leaves an already-sized window untouched, so this is safe
    //! to re-run when re-anchoring at runtime.
    if (screen) {
        const QSize seeded = seededLayerSize(anchors, location, window->size(), screen->geometry().size());
        if (seeded != window->size()) {
            window->resize(seeded);
        }
        ls->setScreen(screen);
    }

    //! The exclusive edge is one of the anchors by construction (edgeFor(location) is always in
    //! anchorsFor(location, ...)), so setting it right after the anchors keeps the committed state
    //! consistent — the compositor rejects a surface whose exclusive edge is not among its anchors.
    ls->setAnchors(anchors);
    ls->setExclusiveEdge(edgeFor(location));
}

void configureView(QWindow *window, QScreen *screen,
                   Plasma::Types::Location location, Latte::Types::Alignment alignment)
{
    LSW *ls = LSW::get(window);
    if (!ls) {
        return;
    }

    ls->setScope(QStringLiteral("dock"));
    updateAnchoring(window, screen, location, alignment);
    ls->setLayer(LSW::LayerTop);
    ls->setKeyboardInteractivity(LSW::KeyboardInteractivityNone);
}

void applyLayer(QWindow *window, Latte::Types::Visibility mode)
{
    if (LSW *ls = LSW::get(window)) {
        ls->setLayer(layerFor(mode));
    }
}

void setFocusPolicy(QWindow *window, bool takesFocus)
{
    if (LSW *ls = LSW::get(window)) {
        ls->setKeyboardInteractivity(takesFocus ? LSW::KeyboardInteractivityOnDemand
                                                : LSW::KeyboardInteractivityNone);
    }
}

void setExclusiveZone(QWindow *window, int zone)
{
    if (LSW *ls = LSW::get(window)) {
        ls->setExclusiveZone(zone);
    }
}

void setUnanchored(QWindow *window)
{
    if (LSW *ls = LSW::get(window)) {
        ls->setExclusiveEdge(LSW::AnchorNone);
        ls->setAnchors(LSW::Anchors());
        ls->setMargins(QMargins());
    }
}

CanvasPlacement canvasPlacement(Plasma::Types::Location location,
                                const QRect &canvasGeometry, const QRect &screenGeometry)
{
    CanvasPlacement placement;

    switch (location) {
    case Plasma::Types::TopEdge:
        placement.anchors = LSW::Anchors(LSW::AnchorTop) | LSW::AnchorLeft | LSW::AnchorRight;
        break;
    case Plasma::Types::BottomEdge:
        placement.anchors = LSW::Anchors(LSW::AnchorBottom) | LSW::AnchorLeft | LSW::AnchorRight;
        break;
    case Plasma::Types::LeftEdge:
        placement.anchors = LSW::Anchors(LSW::AnchorLeft) | LSW::AnchorTop;
        placement.margins.setTop(canvasGeometry.y() - screenGeometry.y());
        break;
    case Plasma::Types::RightEdge:
        placement.anchors = LSW::Anchors(LSW::AnchorRight) | LSW::AnchorTop;
        placement.margins.setTop(canvasGeometry.y() - screenGeometry.y());
        break;
    default:
        placement.anchors = LSW::Anchors(LSW::AnchorBottom) | LSW::AnchorLeft | LSW::AnchorRight;
        break;
    }

    return placement;
}

void applyCanvasPlacement(QWindow *window, Plasma::Types::Location location,
                          const QRect &canvasGeometry, const QRect &screenGeometry)
{
    if (LSW *ls = LSW::get(window)) {
        const CanvasPlacement placement = canvasPlacement(location, canvasGeometry, screenGeometry);
        //! The canvas is an edit-mode overlay, not a strut-reserving panel: it must NOT carry an
        //! exclusive edge. configureView() set one to the dock's location edge, which on a multi-edge
        //! (or transition-stale) canvas is no longer among these anchors — the compositor then kills
        //! the surface with "exclusive edge is not of the anchors". Clear it; AnchorNone is always legal.
        ls->setExclusiveEdge(LSW::AnchorNone);
        ls->setAnchors(placement.anchors);
        ls->setMargins(placement.margins);
    }
}

} // namespace LayerShell
} // namespace WindowSystem
} // namespace Latte
