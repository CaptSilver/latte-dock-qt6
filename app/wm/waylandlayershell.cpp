/*
    SPDX-FileCopyrightText: 2026 Latte Dingo
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "waylandlayershell.h"

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

void configureView(QWindow *window, QScreen *screen,
                   Plasma::Types::Location location, Latte::Types::Alignment alignment)
{
    LSW *ls = LSW::get(window);
    if (!ls) {
        return;
    }

    ls->setScope(QStringLiteral("dock"));
    if (screen) {
        ls->setScreen(screen);
    }
    ls->setAnchors(anchorsFor(location, alignment));
    ls->setExclusiveEdge(edgeFor(location));
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

} // namespace LayerShell
} // namespace WindowSystem
} // namespace Latte
