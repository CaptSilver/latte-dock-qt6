/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "validviewsmapbuilder.h"

namespace Latte {
namespace Layout {

ViewsMap ValidViewsMapBuilder::build(const QList<ViewPlacement> &placements, const QString &primaryScreenName)
{
    ViewsMap map;

    for (const auto &p : placements) {
        if (p.onPrimary) {
            map[primaryScreenName][p.edge] << p.containmentId;
        } else if (p.expectedScreenActive) {
            map[p.expectedScreenName][p.edge] << p.containmentId;
        }
    }

    return map;
}

}
}
