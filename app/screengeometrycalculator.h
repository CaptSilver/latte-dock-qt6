/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SCREENGEOMETRYCALCULATOR_H
#define SCREENGEOMETRYCALCULATOR_H

// local
#include <coretypes.h>

// Qt
#include <QList>
#include <QRect>
#include <QRegion>

// Plasma
#include <Plasma/Plasma>

namespace Latte {

//! A snapshot of the single Latte::View properties that the available-screen
//! geometry math reads. Capturing them into a value type lets the calculation
//! run without a live View / Corona graph, so it can be unit-tested.
struct ViewFootprint
{
    Plasma::Types::Location location{Plasma::Types::Floating};
    Plasma::Types::FormFactor formFactor{Plasma::Types::Planar};
    Latte::Types::Alignment alignment{Latte::Types::Center};
    Latte::Types::Visibility visibilityMode{Latte::Types::None};

    bool hasVisibility{true}; //! view->visibility() is non-null
    bool isOffScreen{false};  //! view->positioner()->isOffScreen()
    bool behaveAsPlasmaPanel{false};

    int normalThickness{0};
    int screenEdgeMargin{0};
    float maxLength{1.0f};
    float offset{0.0f};

    QRect geometry; //! the view window geometry
};

//! Pure geometry math extracted from Latte::Corona. Given a screen and the set
//! of views living on it (as footprints), compute the screen area left free of
//! docks, either as an availableScreenRect (edges pushed inward) or as an
//! availableScreenRegion (footprints subtracted).
class ScreenGeometryCalculator
{
public:
    static QRect availableRect(const QRect &startRect,
                               const QRect &screenGeometry,
                               const QList<ViewFootprint> &footprints,
                               QList<Latte::Types::Visibility> ignoreModes,
                               const QList<Plasma::Types::Location> &ignoreEdges,
                               bool desktopUse);

    static QRegion availableRegion(const QRect &startRect,
                                   const QRect &screenGeometry,
                                   const QList<ViewFootprint> &footprints,
                                   QList<Latte::Types::Visibility> ignoreModes,
                                   const QList<Plasma::Types::Location> &ignoreEdges,
                                   bool desktopUse);
};

}

#endif // SCREENGEOMETRYCALCULATOR_H
