/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "realscreeninfo.h"

// local
#include "screenpool.h"

// Qt
#include <QGuiApplication>
#include <QScreen>

namespace Latte {

RealScreenInfo::RealScreenInfo(ScreenPool *screenPool)
    : m_screenPool(screenPool)
{
}

int RealScreenInfo::numScreens() const
{
    return qGuiApp->screens().count();
}

QRect RealScreenInfo::screenGeometry(int id) const
{
    const auto screens = qGuiApp->screens();
    const QScreen *screen{m_screenPool->primaryScreen()};

    QString screenName;

    if (m_screenPool->hasScreenId(id)) {
        screenName = m_screenPool->connector(id);
    }

    for (const auto scr : screens) {
        if (scr->name() == screenName) {
            screen = scr;
            break;
        }
    }

    if (!screen) {
        return {};
    }

    return screen->geometry();
}

bool RealScreenInfo::hasScreenForId(int id) const
{
    return m_screenPool->screenForId(id) != nullptr;
}

QRect RealScreenInfo::geometryForId(int id) const
{
    const QScreen *screen = m_screenPool->screenForId(id);
    return screen ? screen->geometry() : QRect();
}

QRect RealScreenInfo::availableGeometryForId(int id) const
{
    const QScreen *screen = m_screenPool->screenForId(id);
    return screen ? screen->availableGeometry() : QRect();
}

}
