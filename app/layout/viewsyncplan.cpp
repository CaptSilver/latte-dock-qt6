/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "viewsyncplan.h"

namespace Latte {
namespace Layout {

ViewSyncPlan ViewSyncPlanner::plan(const ViewSyncInputs &in)
{
    ViewSyncPlan result;

    //! a mapped containment that has no view yet must be added
    for (const uint id : in.containmentIds) {
        if (!in.viewedContainmentIds.contains(id) && in.mapIds.contains(id)) {
            result.toAdd << id;
        }
    }

    //! an existing original view is removed when the map no longer wants it, else reconsidered
    for (const uint id : in.originalViewContainmentIds) {
        if (in.mapIds.contains(id)) {
            result.toReconsider << id;
        } else {
            result.toRemove << id;
        }
    }

    return result;
}

}
}
