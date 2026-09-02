/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "viewedges.h"

namespace Latte {
namespace Layout {

QList<Plasma::Types::Location> ViewEdges::all()
{
    return QList<Plasma::Types::Location>{Plasma::Types::BottomEdge, Plasma::Types::LeftEdge,
                                          Plasma::Types::TopEdge, Plasma::Types::RightEdge};
}

QList<Plasma::Types::Location> ViewEdges::freeFrom(const QList<Plasma::Types::Location> &occupied)
{
    QList<Plasma::Types::Location> edges = all();

    //! removeOne() on an already-removed edge is a no-op, so views sharing an edge are fine
    for (const Plasma::Types::Location &edge : occupied) {
        edges.removeOne(edge);
    }

    return edges;
}

Plasma::Types::Location ViewEdges::forNewView(const QList<Plasma::Types::Location> &freeEdges, Plasma::Types::Location preferred)
{
    if (freeEdges.contains(preferred)) {
        return preferred;
    }

    return freeEdges.isEmpty() ? Plasma::Types::BottomEdge : freeEdges.first();
}

}
}
