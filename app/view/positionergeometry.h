/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef POSITIONERGEOMETRY_H
#define POSITIONERGEOMETRY_H

// local
#include <coretypes.h>

// Qt
#include <QPoint>
#include <QRect>
#include <QRegion>
#include <QSize>
#include <QtGlobal>

// Plasma
#include <Plasma/Plasma>

namespace Latte {
namespace ViewPart {
namespace PositionerGeometry {

// All inputs the sizing/placement functions need from the live View. Populated
// by the Positioner adapters from m_view->* before calling into the pure layer.
struct ViewGeometryInputs {
    Plasma::Types::Location location{Plasma::Types::BottomEdge};
    Plasma::Types::FormFactor formFactor{Plasma::Types::Horizontal};
    Latte::Types::Alignment alignment{Latte::Types::Center};
    bool behaveAsPlasmaPanel{false};
    int normalThickness{0};
    int maxThickness{0};
    int maxNormalThickness{0};
    int innerShadow{0};
    int screenEdgeMargin{0};
    int editThickness{0};
    int viewWidth{0};
    int viewHeight{0};
    float maxLength{1.0f};
    float offset{0.0f};
    int slideOffset{0};
};

// The top/bottom border flags set by validateTopBottomBorders.
struct ForcedBorders {
    bool top{false};
    bool bottom{false};
};

// Mirror of AbstractWindowInterface::Slide — same enumerators in the same
// order so the Positioner adapter can cast with a static_cast<Slide>(v)
// without touching the heavy abstractwindowinterface.h header.
// IMPORTANT: keep in sync with AbstractWindowInterface::Slide in
// app/wm/abstractwindowinterface.h.
enum class SlideEdge {
    None,
    Top,
    Left,
    Bottom,
    Right,
};

// -------------------------------------------------------------------------
// Pure geometry functions — ported verbatim from positioner.cpp
// -------------------------------------------------------------------------

// From Positioner::updatePosition(): the window's top-left position for the
// given available screen rect. Does not touch m_validGeometry or call
// m_view->setPosition(); those stay in the adapter.
inline QPoint dockPosition(const ViewGeometryInputs &in, const QRect &availableScreenRect)
{
    QRect screenGeometry{availableScreenRect};
    QPoint position{0, 0};

    const auto gap = [&](int scr_length) -> int {
        return static_cast<int>(scr_length * in.offset);
    };
    const auto gapCentered = [&](int scr_length) -> int {
        return static_cast<int>(scr_length * ((1 - in.maxLength) / 2) + scr_length * in.offset);
    };
    const auto gapReversed = [&](int scr_length) -> int {
        return static_cast<int>(scr_length - (scr_length * in.maxLength) - gap(scr_length));
    };

    int cleanThickness = in.normalThickness - in.innerShadow;
    int screenEdgeMargin = in.behaveAsPlasmaPanel ? in.screenEdgeMargin - qAbs(in.slideOffset) : 0;

    switch (in.location) {
    case Plasma::Types::TopEdge:
        if (in.behaveAsPlasmaPanel) {
            int y = screenGeometry.y() + screenEdgeMargin;

            if (in.alignment == Latte::Types::Left) {
                position = {screenGeometry.x() + gap(screenGeometry.width()), y};
            } else if (in.alignment == Latte::Types::Right) {
                position = {screenGeometry.x() + gapReversed(screenGeometry.width()) + 1, y};
            } else {
                position = {screenGeometry.x() + gapCentered(screenGeometry.width()), y};
            }
        } else {
            position = {screenGeometry.x(), screenGeometry.y()};
        }
        break;

    case Plasma::Types::BottomEdge:
        if (in.behaveAsPlasmaPanel) {
            int y = screenGeometry.y() + screenGeometry.height() - cleanThickness - screenEdgeMargin;

            if (in.alignment == Latte::Types::Left) {
                position = {screenGeometry.x() + gap(screenGeometry.width()), y};
            } else if (in.alignment == Latte::Types::Right) {
                position = {screenGeometry.x() + gapReversed(screenGeometry.width()) + 1, y};
            } else {
                position = {screenGeometry.x() + gapCentered(screenGeometry.width()), y};
            }
        } else {
            position = {screenGeometry.x(), screenGeometry.y() + screenGeometry.height() - in.viewHeight};
        }
        break;

    case Plasma::Types::RightEdge:
        if (in.behaveAsPlasmaPanel) {
            int x = availableScreenRect.right() - cleanThickness + 1 - screenEdgeMargin;

            if (in.alignment == Latte::Types::Top) {
                position = {x, availableScreenRect.y() + gap(availableScreenRect.height())};
            } else if (in.alignment == Latte::Types::Bottom) {
                position = {x, availableScreenRect.y() + gapReversed(availableScreenRect.height()) + 1};
            } else {
                position = {x, availableScreenRect.y() + gapCentered(availableScreenRect.height())};
            }
        } else {
            position = {availableScreenRect.right() - in.viewWidth + 1, availableScreenRect.y()};
        }
        break;

    case Plasma::Types::LeftEdge:
        if (in.behaveAsPlasmaPanel) {
            int x = availableScreenRect.x() + screenEdgeMargin;

            if (in.alignment == Latte::Types::Top) {
                position = {x, availableScreenRect.y() + gap(availableScreenRect.height())};
            } else if (in.alignment == Latte::Types::Bottom) {
                position = {x, availableScreenRect.y() + gapReversed(availableScreenRect.height()) + 1};
            } else {
                position = {x, availableScreenRect.y() + gapCentered(availableScreenRect.height())};
            }
        } else {
            position = {availableScreenRect.x(), availableScreenRect.y()};
        }
        break;

    default:
        break;
    }

    return position;
}

// From Positioner::resizeWindow(): the window size for the given available
// screen rect and physical screen size.
inline QSize windowSize(const ViewGeometryInputs &in,
                        const QRect &availableScreenRect,
                        const QSize &screenSize)
{
    QSize size = (in.formFactor == Plasma::Types::Vertical)
                     ? QSize(in.maxThickness, availableScreenRect.height())
                     : QSize(screenSize.width(), in.maxThickness);

    if (in.formFactor == Plasma::Types::Vertical) {
        if (in.behaveAsPlasmaPanel) {
            size.setWidth(in.normalThickness);
            size.setHeight(static_cast<int>(in.maxLength * availableScreenRect.height()));
        }
    } else {
        if (in.behaveAsPlasmaPanel) {
            size.setWidth(static_cast<int>(in.maxLength * screenSize.width()));
            size.setHeight(in.normalThickness);
        }
    }

    // Protect from invalid window sizes under Wayland.
    size.setWidth(qMax(1, size.width()));
    size.setHeight(qMax(1, size.height()));

    return size;
}

// From Positioner::maximumNormalGeometry(): the maximum geometry a vertical
// dock can occupy given its edge and screen geometry.
inline QRect maximumNormalGeometry(Plasma::Types::Location location,
                                   int maxNormalThickness,
                                   const QRect &currentScrGeometry)
{
    int xPos = 0;
    int yPos = currentScrGeometry.y();
    int maxHeight = currentScrGeometry.height();
    int maxWidth = maxNormalThickness;
    QRect maxGeometry;
    maxGeometry.setRect(0, 0, maxWidth, maxHeight);

    switch (location) {
    case Plasma::Types::LeftEdge:
        xPos = currentScrGeometry.x();
        maxGeometry.setRect(xPos, yPos, maxWidth, maxHeight);
        break;

    case Plasma::Types::RightEdge:
        xPos = currentScrGeometry.right() - maxWidth + 1;
        maxGeometry.setRect(xPos, yPos, maxWidth, maxHeight);
        break;

    default:
        // bypass clang warnings
        break;
    }

    return maxGeometry;
}

// From Positioner::updateCanvasGeometry(): the edit-mode canvas rect.
inline QRect canvasGeometry(Plasma::Types::Location location,
                             Plasma::Types::FormFactor formFactor,
                             int editThickness,
                             const QRect &screenGeometry,
                             const QRect &availableScreenRect)
{
    QRect canvas;
    int thickness{editThickness};

    if (formFactor == Plasma::Types::Vertical) {
        canvas.setWidth(thickness);
        canvas.setHeight(availableScreenRect.height());
    } else {
        canvas.setWidth(screenGeometry.width());
        canvas.setHeight(thickness);
    }

    switch (location) {
    case Plasma::Types::TopEdge:
        canvas.moveLeft(screenGeometry.x());
        canvas.moveTop(screenGeometry.y());
        break;

    case Plasma::Types::BottomEdge:
        canvas.moveLeft(screenGeometry.x());
        canvas.moveTop(screenGeometry.bottom() - thickness + 1);
        break;

    case Plasma::Types::RightEdge:
        canvas.moveLeft(screenGeometry.right() - thickness + 1);
        canvas.moveTop(availableScreenRect.y());
        break;

    case Plasma::Types::LeftEdge:
        canvas.moveLeft(availableScreenRect.x());
        canvas.moveTop(availableScreenRect.y());
        break;

    default:
        break;
    }

    return canvas;
}

// From Positioner::slideLocation() — the switch body only, without the
// Floating-resolution that belongs to the live caller. Returns SlideEdge::None
// for any location not one of the four edges (including Floating).
// The Positioner adapter casts the result to AbstractWindowInterface::Slide via
// a static_cast (the two enums share the same enumerators in the same order).
inline SlideEdge slideEdge(Plasma::Types::Location location)
{
    switch (location) {
    case Plasma::Types::TopEdge:
        return SlideEdge::Top;

    case Plasma::Types::RightEdge:
        return SlideEdge::Right;

    case Plasma::Types::BottomEdge:
        return SlideEdge::Bottom;

    case Plasma::Types::LeftEdge:
        return SlideEdge::Left;

    default:
        return SlideEdge::None;
    }
}

// From Positioner::validateTopBottomBorders(): decide whether the top/bottom
// internal panel borders must be forced on for a vertical dock. The probe rect
// is one pixel tall at the edge of the available area; if it sits entirely in
// the free region the border is drawn.
inline ForcedBorders forcedBorders(Plasma::Types::Location location,
                                   int screenEdgeMargin,
                                   const QRect &screenGeometry,
                                   const QRect &availableScreenRect,
                                   const QRegion &availableScreenRegion)
{
    ForcedBorders fb;
    int edgeMargin = qMax(1, screenEdgeMargin);

    if (availableScreenRect.top() != screenGeometry.top()) {
        int x = (location == Plasma::Types::LeftEdge)
                     ? screenGeometry.x()
                     : screenGeometry.right() - edgeMargin + 1;
        QRegion fitInRegion = QRect(x, availableScreenRect.y() - 1, edgeMargin, 1);
        QRegion subtracted = fitInRegion.subtracted(availableScreenRegion);

        if (subtracted.isNull()) {
            fb.top = true;
        }
    }

    if (availableScreenRect.bottom() != screenGeometry.bottom()) {
        int x = (location == Plasma::Types::LeftEdge)
                     ? screenGeometry.x()
                     : screenGeometry.right() - edgeMargin + 1;
        QRegion fitInRegion = QRect(x, availableScreenRect.bottom() + 1, edgeMargin, 1);
        QRegion subtracted = fitInRegion.subtracted(availableScreenRegion);

        if (subtracted.isNull()) {
            fb.bottom = true;
        }
    }

    return fb;
}

} // namespace PositionerGeometry
} // namespace ViewPart
} // namespace Latte

#endif // POSITIONERGEOMETRY_H
