/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "windowinfowrap.h"

#include <QList>
#include <QRect>

namespace Latte {
namespace WindowSystem {
namespace WindowTrackingPredicates {

inline bool intersects(const WindowInfoWrap &winfo, const QRect &viewAbsoluteGeometry)
{
    return (!winfo.isMinimized() && !winfo.isShaded() && winfo.geometry().intersects(viewAbsoluteGeometry));
}

inline bool isActive(const WindowInfoWrap &winfo)
{
    return (winfo.isValid() && winfo.isActive() && !winfo.isMinimized());
}

inline bool isActiveInViewScreen(const WindowInfoWrap &winfo, const QRect &screenGeometry)
{
    return (winfo.isValid() && winfo.isActive() && !winfo.isMinimized() && screenGeometry.intersects(winfo.geometry()));
}

inline bool isMaximizedInViewScreen(const WindowInfoWrap &winfo, const QRect &screenGeometry)
{
    return (winfo.isValid() && !winfo.isMinimized() && !winfo.isShaded() && winfo.isMaximized() && screenGeometry.intersects(winfo.geometry()));
}

inline bool isIgnored(const QList<WindowId> &ignoredWindows, const WindowId &wid)
{
    return ignoredWindows.contains(wid);
}

inline bool isRegisteredPlasmaIgnored(const QList<WindowId> &plasmaIgnoredWindows, const WindowId &wid)
{
    return plasmaIgnoredWindows.contains(wid);
}

inline bool isWhitelisted(const QList<WindowId> &whitelistedWindows, const WindowId &wid)
{
    return whitelistedWindows.contains(wid);
}

inline bool hasBlockedTracking(const QList<WindowId> &ignoredWindows,
                                const QList<WindowId> &plasmaIgnoredWindows,
                                const QList<WindowId> &whitelistedWindows,
                                const WindowId &wid)
{
    return (!isWhitelisted(whitelistedWindows, wid) &&
            (isRegisteredPlasmaIgnored(plasmaIgnoredWindows, wid) || isIgnored(ignoredWindows, wid)));
}

inline bool shouldRegister(const QList<WindowId> &existingWindows, const WindowId &wid)
{
    return (!wid.isNull() && !existingWindows.contains(wid));
}

} // namespace WindowTrackingPredicates
} // namespace WindowSystem
} // namespace Latte
