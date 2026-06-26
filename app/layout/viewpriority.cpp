/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "viewpriority.h"

// Plasma
#include <Plasma/Plasma>

namespace Latte {
namespace Layout {

bool ViewPriority::atLowerScreenPriority(const Latte::Data::View &test, const Latte::Data::View &base)
{
    if (test.onPrimary && base.onPrimary) {
        return false;
    } else if (!base.onPrimary && test.onPrimary) {
        return false;
    } else if (base.onPrimary && !test.onPrimary) {
        return true;
    } else {
        return test.screen <= base.screen;
    }
}

bool ViewPriority::atLowerStatePriority(const Latte::Data::View &test, const Latte::Data::View &base)
{
    if (test.isActive == base.isActive) {
        return false;
    } else if (!base.isActive && test.isActive) {
        return false;
    } else if (base.isActive && !test.isActive) {
        return true;
    }

    return false;
}

bool ViewPriority::atLowerEdgePriority(const Latte::Data::View &test, const Latte::Data::View &base)
{
    QList<Plasma::Types::Location> edges{Plasma::Types::RightEdge, Plasma::Types::TopEdge,
                Plasma::Types::LeftEdge, Plasma::Types::BottomEdge};

    int testPriority = -1;
    int basePriority = -1;

    for (int i = 0; i < edges.count(); ++i) {
        if (edges[i] == base.edge) {
            basePriority = i;
        }

        if (edges[i] == test.edge) {
            testPriority = i;
        }
    }

    if (testPriority < basePriority) {
        return true;
    } else {
        return false;
    }
}

QList<Latte::Data::View> ViewPriority::sorted(const QList<Latte::Data::View> &viewsData)
{
    QList<Latte::Data::View> sortedData = viewsData;

    //! sort the views based on screens and edges priorities
    //! views on primary screen have higher priority and
    //! for views in the same screen the priority goes to
    //! Bottom,Left,Top,Right
    for (int i = 0; i < sortedData.size(); ++i) {
        for (int j = 0; j < sortedData.size() - i - 1; ++j) {
            if (atLowerStatePriority(sortedData[j], sortedData[j + 1])
                    || atLowerScreenPriority(sortedData[j], sortedData[j + 1])
                    || (!atLowerScreenPriority(sortedData[j], sortedData[j + 1])
                        && atLowerEdgePriority(sortedData[j], sortedData[j + 1])) ) {
                Latte::Data::View temp = sortedData[j + 1];
                sortedData[j + 1] = sortedData[j];
                sortedData[j] = temp;
            }
        }
    }

    return sortedData;
}

}
}
