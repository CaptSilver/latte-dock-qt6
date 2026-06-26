/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWSYNCPLAN_H
#define VIEWSYNCPLAN_H

// Qt
#include <QList>
#include <QSet>

namespace Latte {
namespace Layout {

struct ViewSyncInputs
{
    QList<uint> containmentIds;             //! all containment ids, in order
    QSet<uint> viewedContainmentIds;        //! ids that already have ANY latte view
    QList<uint> originalViewContainmentIds; //! ids whose latte view is original(), in view order
    QSet<uint> mapIds;                      //! ids the ViewsMap wants (flattened)
};

struct ViewSyncPlan
{
    QList<uint> toAdd;          //! create a view for these
    QList<uint> toRemove;       //! delete the original view for these
    QList<uint> toReconsider;   //! original view stays; re-check its screen
};

//! Pure add/remove/reconsider diff for syncing latte views to the valid-views map.
//! Extracted from GenericLayout::syncLatteViewsToScreens so it can be unit-tested.
class ViewSyncPlanner
{
public:
    static ViewSyncPlan plan(const ViewSyncInputs &in);
};

}
}

#endif
