/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screengeometrycalculator.h"

namespace Latte {

namespace {

//! None and NormalWindow never reserve space, regardless of the caller's choices.
void blacklistDefaultModes(QList<Latte::Types::Visibility> &ignoreModes)
{
    if (!ignoreModes.contains(Latte::Types::None)) {
        ignoreModes << Latte::Types::None;
    }

    if (!ignoreModes.contains(Latte::Types::NormalWindow)) {
        ignoreModes << Latte::Types::NormalWindow;
    }
}

//! Whether a view should carve space out of the screen, after applying the
//! desktop-startup, edge and visibility-mode filters.
bool isReserving(const ViewFootprint &view,
                 const QList<Latte::Types::Visibility> &ignoreModes,
                 const QList<Plasma::Types::Location> &ignoreEdges,
                 bool allEdges,
                 bool desktopUse)
{
    const bool inDesktopOffScreenStartup = desktopUse && view.isOffScreen;

    return !inDesktopOffScreenStartup && (allEdges || !ignoreEdges.contains(view.location)) && view.hasVisibility && !ignoreModes.contains(view.visibilityMode);
}

}

QRect ScreenGeometryCalculator::availableRect(const QRect &startRect,
                                              const QRect &screenGeometry,
                                              const QList<ViewFootprint> &footprints,
                                              QList<Latte::Types::Visibility> ignoreModes,
                                              const QList<Plasma::Types::Location> &ignoreEdges,
                                              bool desktopUse)
{
    QRect available = startRect;

    if (footprints.isEmpty()) {
        return available;
    }

    blacklistDefaultModes(ignoreModes);
    const bool allEdges = ignoreEdges.isEmpty();

    for (const auto &view : footprints) {
        if (!isReserving(view, ignoreModes, ignoreEdges, allEdges, desktopUse)) {
            continue;
        }

        const int appliedThickness = view.behaveAsPlasmaPanel
                                         ? view.screenEdgeMargin + view.normalThickness
                                         : view.normalThickness;

        // Usually availableScreenRect is used by the desktop, but Latte doesn't have a
        // desktop, so here we only need the available space for top and bottom edges;
        // left and right are the ones that dodge other docks.
        switch (view.location) {
        case Plasma::Types::TopEdge:
            if (view.behaveAsPlasmaPanel && desktopUse) {
                //! ignore any real window slide outs in all cases
                available.setTop(qMax(available.top(), screenGeometry.top() + appliedThickness));
            } else {
                available.setTop(qMax(available.top(), view.geometry.y() + appliedThickness));
            }
            break;

        case Plasma::Types::BottomEdge:
            if (view.behaveAsPlasmaPanel && desktopUse) {
                available.setBottom(qMin(available.bottom(), screenGeometry.bottom() - appliedThickness));
            } else {
                available.setBottom(qMin(available.bottom(), view.geometry.y() + view.geometry.height() - appliedThickness));
            }
            break;

        case Plasma::Types::LeftEdge:
            if (view.behaveAsPlasmaPanel && desktopUse) {
                available.setLeft(qMax(available.left(), screenGeometry.left() + appliedThickness));
            } else {
                available.setLeft(qMax(available.left(), view.geometry.x() + appliedThickness));
            }
            break;

        case Plasma::Types::RightEdge:
            if (view.behaveAsPlasmaPanel && desktopUse) {
                available.setRight(qMin(available.right(), screenGeometry.right() - appliedThickness));
            } else {
                available.setRight(qMin(available.right(), view.geometry.x() + view.geometry.width() - appliedThickness));
            }
            break;

        default:
            break;
        }
    }

    return available;
}

QRegion ScreenGeometryCalculator::availableRegion(const QRect &startRect,
                                                  const QRect &screenGeometry,
                                                  const QList<ViewFootprint> &footprints,
                                                  QList<Latte::Types::Visibility> ignoreModes,
                                                  const QList<Plasma::Types::Location> &ignoreEdges,
                                                  bool desktopUse)
{
    QRegion available = startRect;

    if (footprints.isEmpty()) {
        return available;
    }

    blacklistDefaultModes(ignoreModes);
    const bool allEdges = ignoreEdges.isEmpty();

    for (const auto &view : footprints) {
        if (!isReserving(view, ignoreModes, ignoreEdges, allEdges, desktopUse)) {
            continue;
        }

        const int realThickness = view.normalThickness;

        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;

        switch (view.formFactor) {
        case Plasma::Types::Horizontal:
            if (view.behaveAsPlasmaPanel) {
                w = view.geometry.width();
                x = view.geometry.x();
            } else {
                w = view.maxLength * view.geometry.width();
                int offsetW = view.offset * view.geometry.width();

                switch (view.alignment) {
                case Latte::Types::Left:
                    x = view.geometry.x() + offsetW;
                    break;

                case Latte::Types::Center:
                case Latte::Types::Justify:
                    x = (view.geometry.center().x() - w / 2) + 1 + offsetW;
                    break;

                case Latte::Types::Right:
                    x = view.geometry.right() + 1 - w - offsetW;
                    break;

                default:
                    break;
                }
            }
            break;
        case Plasma::Types::Vertical:
            if (view.behaveAsPlasmaPanel) {
                h = view.geometry.height();
                y = view.geometry.y();
            } else {
                h = view.maxLength * view.geometry.height();
                int offsetH = view.offset * view.geometry.height();

                switch (view.alignment) {
                case Latte::Types::Top:
                    y = view.geometry.y() + offsetH;
                    break;

                case Latte::Types::Center:
                case Latte::Types::Justify:
                    y = (view.geometry.center().y() - h / 2) + 1 + offsetH;
                    break;

                case Latte::Types::Bottom:
                    y = view.geometry.bottom() - h - offsetH;
                    break;

                default:
                    break;
                }
            }
            break;
        default:
            break;
        }

        switch (view.location) {
        case Plasma::Types::TopEdge:
            if (view.behaveAsPlasmaPanel) {
                QRect viewGeometry = view.geometry;

                if (desktopUse) {
                    //! ignore any real window slide outs in all cases
                    viewGeometry.moveTop(screenGeometry.top() + view.screenEdgeMargin);
                }

                available -= viewGeometry;
            } else {
                y = view.geometry.y();
                available -= QRect(x, y, w, realThickness);
            }
            break;

        case Plasma::Types::BottomEdge:
            if (view.behaveAsPlasmaPanel) {
                QRect viewGeometry = view.geometry;

                if (desktopUse) {
                    viewGeometry.moveTop(screenGeometry.bottom() - view.screenEdgeMargin - viewGeometry.height());
                }

                available -= viewGeometry;
            } else {
                y = view.geometry.bottom() - realThickness + 1;
                available -= QRect(x, y, w, realThickness);
            }
            break;

        case Plasma::Types::LeftEdge:
            if (view.behaveAsPlasmaPanel) {
                QRect viewGeometry = view.geometry;

                if (desktopUse) {
                    viewGeometry.moveLeft(screenGeometry.left() + view.screenEdgeMargin);
                }

                available -= viewGeometry;
            } else {
                x = view.geometry.x();
                available -= QRect(x, y, realThickness, h);
            }
            break;

        case Plasma::Types::RightEdge:
            if (view.behaveAsPlasmaPanel) {
                QRect viewGeometry = view.geometry;

                if (desktopUse) {
                    viewGeometry.moveLeft(screenGeometry.right() - view.screenEdgeMargin - viewGeometry.width());
                }

                available -= viewGeometry;
            } else {
                x = view.geometry.right() - realThickness + 1;
                available -= QRect(x, y, realThickness, h);
            }
            break;

        default:
            break;
        }
    }

    return available;
}

}
