/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Qt
#include <QList>
#include <QRect>
#include <QtGlobal>

namespace Latte {
namespace WindowSystem {

//! Pure geometry classification lifted out of AbstractWindowInterface so the
//! fullscreen/panel/sidepanel tests are unit-testable without qGuiApp or a live
//! window list. The caller passes the (already devicePixelRatio-adjusted) screen
//! geometries; these functions only reason about rectangles.
namespace WindowGeometryPredicates {

constexpr int MAXPLASMAPANELTHICKNESS = 96;
constexpr int MAXSIDEPANELTHICKNESS = 512;

//! A window whose geometry exactly fills one of the screens.
inline bool isFullScreenWindow(const QRect &wGeometry, const QList<QRect> &screenGeometries)
{
    if (wGeometry.isEmpty()) {
        return false;
    }

    for (const auto &screenGeometry : screenGeometries) {
        if (wGeometry == screenGeometry) {
            return true;
        }
    }

    return false;
}

//! A thin window snapped to a screen edge, i.e. a Plasma panel.
inline bool isPlasmaPanel(const QRect &wGeometry, const QList<QRect> &screenGeometries)
{
    if (wGeometry.isEmpty()) {
        return false;
    }

    bool isTouchingHorizontalEdge{false};
    bool isTouchingVerticalEdge{false};

    for (const auto &screenGeometry : screenGeometries) {
        if (screenGeometry.contains(wGeometry.center())) {
            if (wGeometry.y() == screenGeometry.y() || wGeometry.bottom() == screenGeometry.bottom()) {
                isTouchingHorizontalEdge = true;
            }

            if (wGeometry.left() == screenGeometry.left() || wGeometry.right() == screenGeometry.right()) {
                isTouchingVerticalEdge = true;
            }

            if (isTouchingVerticalEdge && isTouchingHorizontalEdge) {
                break;
            }
        }
    }

    return (isTouchingHorizontalEdge && wGeometry.height() < MAXPLASMAPANELTHICKNESS) || (isTouchingVerticalEdge && wGeometry.width() < MAXPLASMAPANELTHICKNESS);
}

//! A tall, narrow window covering most of a screen's height, i.e. a side panel.
inline bool isSidepanel(const QRect &wGeometry, const QList<QRect> &screenGeometries)
{
    bool isVertical = wGeometry.height() > wGeometry.width();

    int thickness = qMin(wGeometry.width(), wGeometry.height());
    int length = qMax(wGeometry.width(), wGeometry.height());

    QRect screenGeometry;

    for (const auto &curScrGeometry : screenGeometries) {
        if (curScrGeometry.contains(wGeometry.center())) {
            screenGeometry = curScrGeometry;
            break;
        }
    }

    bool thicknessIsAcccepted = isVertical && ((thickness > MAXPLASMAPANELTHICKNESS) && (thickness < MAXSIDEPANELTHICKNESS));
    bool lengthIsAccepted = isVertical && !screenGeometry.isEmpty() && (length > 0.6 * screenGeometry.height());
    float sideRatio = (float)wGeometry.width() / (float)wGeometry.height();

    return (thicknessIsAcccepted && lengthIsAccepted && sideRatio < 0.4);
}

} // namespace WindowGeometryPredicates
} // namespace WindowSystem
} // namespace Latte
