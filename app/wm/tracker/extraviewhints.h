/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <functional>

#include <QHash>
#include <QList>
#include <QRect>

#include <Plasma/Plasma>

namespace Latte {
namespace WindowSystem {
namespace Tracker {

struct TrackedViewGeometry {
    int viewKey{0};
    bool enabled{false};
    bool trackingCurrentActivity{false};
    bool isHorizontal{false};
    bool isVertical{false};
    int screenId{-1};
    Plasma::Types::Location location{Plasma::Types::Floating};
    bool isTouchingTopViewAndIsBusy{false};
    bool isTouchingBottomViewAndIsBusy{false};
    QRect absoluteGeometry;
};

namespace ExtraViewHints {

// Returns horKey -> bool: does this horizontal enabled/tracking view touch a busy vertical on same screen?
// O(views): bucket busy verticals by screenId first, then one pass over horizontals.
inline QHash<int,bool> bucketHorizontalTouchingBusyVertical(
    const QList<TrackedViewGeometry> &views,
    const std::function<bool(const TrackedViewGeometry &hor, const TrackedViewGeometry &ver)> &hasEdgeTouch)
{
    // Bucket enabled+tracking vertical views by screenId
    QHash<int, QList<int>> verIndicesByScreen; // screenId -> indices into views
    for (int i = 0; i < views.size(); ++i) {
        const auto &v = views.at(i);
        if (v.isVertical && v.enabled && v.trackingCurrentActivity) {
            verIndicesByScreen[v.screenId].append(i);
        }
    }

    QHash<int,bool> result;
    for (int i = 0; i < views.size(); ++i) {
        const auto &hor = views.at(i);
        if (!hor.isHorizontal || !hor.enabled || !hor.trackingCurrentActivity) {
            continue;
        }
        bool touching = false;
        const auto &verIndices = verIndicesByScreen.value(hor.screenId);
        for (int j : verIndices) {
            const auto &ver = views.at(j);
            bool topTouch = (hor.location == Plasma::Types::TopEdge) && ver.isTouchingTopViewAndIsBusy && hasEdgeTouch(hor, ver);
            bool bottomTouch = (hor.location == Plasma::Types::BottomEdge) && ver.isTouchingBottomViewAndIsBusy && hasEdgeTouch(hor, ver);
            if (topTouch || bottomTouch) {
                touching = true;
                break;
            }
        }
        result[hor.viewKey] = touching;
    }
    return result;
}

} // namespace ExtraViewHints
} // namespace Tracker
} // namespace WindowSystem
} // namespace Latte
