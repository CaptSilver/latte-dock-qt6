/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ACTIVITYSETALGEBRA_H
#define ACTIVITYSETALGEBRA_H

// Qt
#include <QString>
#include <QStringList>

namespace Latte {
namespace Layouts {

//! Pure set-algebra over activity-id lists, lifted out of Synchronizer so the
//! free/free-running/valid filtering is unit-testable without the live Corona graph.
//! Synchronizer keeps gathering the inputs (all activities, running activities, the
//! assigned-layout keys) from its live managers and delegates the filtering here.
namespace ActivitySetAlgebra {

//! All activities with every assigned activity id removed (removeAll semantics:
//! duplicates of an assigned id are all dropped).
inline QStringList freeActivities(const QStringList &allActivities, const QStringList &assignedActivityIds)
{
    QStringList frees = allActivities;

    for (const auto &assigned : assignedActivityIds) {
        frees.removeAll(assigned);
    }

    return frees;
}

//! Running activities that are not assigned to a layout, order preserved.
inline QStringList freeRunningActivities(const QStringList &runningActivities, const QStringList &assignedActivityIds)
{
    QStringList result;

    for (const auto &activity : runningActivities) {
        if (!assignedActivityIds.contains(activity)) {
            result.append(activity);
        }
    }

    return result;
}

//! The subset of a layout's activities that still exist in the full activity list.
inline QStringList validActivities(const QStringList &layoutActivities, const QStringList &allActivities)
{
    QStringList valids;

    for (const auto &activity : layoutActivities) {
        if (allActivities.contains(activity)) {
            valids << activity;
        }
    }

    return valids;
}

}
}
}

#endif
