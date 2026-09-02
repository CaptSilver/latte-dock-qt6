/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWEDGES_H
#define VIEWEDGES_H

// Qt
#include <QList>

// Plasma
#include <Plasma/Plasma>

namespace Latte {
namespace Layout {

//! Pure screen-edge arithmetic for view placement: which edges of a screen are still spare
//! and which one a new view should take. Extracted from GenericLayout and View so the
//! "every edge is taken" fallback is testable without a live View/Corona graph.
class ViewEdges
{
public:
    //! canonical order; a new view whose preferred edge is taken walks this list from the front
    static QList<Plasma::Types::Location> all();

    static QList<Plasma::Types::Location> freeFrom(const QList<Plasma::Types::Location> &occupied);

    static Plasma::Types::Location forNewView(const QList<Plasma::Types::Location> &freeEdges, Plasma::Types::Location preferred);
};

}
}

#endif
