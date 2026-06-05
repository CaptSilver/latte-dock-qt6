/*
    SPDX-FileCopyrightText: 2016 Smith AR <audoban@openmailbox.org>
    SPDX-FileCopyrightText: 2016 Michail Vourlakos <mvourlakos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "quickwindowsystem.h"

// Qt
#include <QDebug>

// X11
#include <KWindowSystem>

namespace Latte {

QuickWindowSystem::QuickWindowSystem(QObject *parent)
    : QObject(parent)
{
    //! Wayland always composites; X11 compositing API removed in KF6
    m_compositing = true;
}

QuickWindowSystem::~QuickWindowSystem()
{
    qDebug() << staticMetaObject.className() << "destructed";
}

bool QuickWindowSystem::compositingActive() const
{
    return m_compositing;
}

bool QuickWindowSystem::isPlatformWayland() const
{
    return KWindowSystem::isPlatformWayland();
}

bool QuickWindowSystem::isPlatformX11() const
{
    return KWindowSystem::isPlatformX11();
}

} //end of namespace
