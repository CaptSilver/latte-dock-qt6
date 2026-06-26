/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VALIDVIEWSMAPBUILDER_H
#define VALIDVIEWSMAPBUILDER_H

// local
#include "viewsmap.h"

// Qt
#include <QList>
#include <QString>

// Plasma
#include <Plasma/Plasma>

namespace Latte {
namespace Layout {

//! One containment's resolved placement, gathered from live state by GenericLayout.
struct ViewPlacement
{
    uint containmentId{0};
    Plasma::Types::Location edge{Plasma::Types::BottomEdge};
    bool onPrimary{true};
    QString expectedScreenName;     //! connector for the view's screen; unused when onPrimary
    bool expectedScreenActive{false}; //! whether that screen is active; unused when onPrimary
};

//! Pure construction of the valid-views map (SCREEN_NAME -> EDGE -> ids) from placements:
//! onPrimary -> primary screen; else if the expected screen is active -> expected screen;
//! else dropped. Extracted from GenericLayout::validViewsMap so it can be unit-tested.
class ValidViewsMapBuilder
{
public:
    static ViewsMap build(const QList<ViewPlacement> &placements, const QString &primaryScreenName);
};

}
}

#endif
