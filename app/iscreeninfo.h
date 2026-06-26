/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ISCREENINFO_H
#define ISCREENINFO_H

// Qt
#include <QRect>

namespace Latte {

//! Seam over the live screen set + ScreenPool so geometry math is controllable in tests.
//! Returns geometry VALUES, not QScreen* — a fake cannot construct a real QScreen, and the
//! geometry math only needs the rects. screenGeometry() mirrors Corona::screenGeometry's
//! connector + primary-fallback resolution; the *ForId() trio mirrors the screenForId path
//! the availableScreen* methods use.
class IScreenInfo
{
public:
    virtual ~IScreenInfo() = default;

    virtual int numScreens() const = 0;                       //! qApp->screens().count()
    virtual QRect screenGeometry(int id) const = 0;           //! Corona::screenGeometry override logic
    virtual bool hasScreenForId(int id) const = 0;            //! ScreenPool::screenForId(id) != nullptr
    virtual QRect geometryForId(int id) const = 0;            //! screenForId(id)->geometry()
    virtual QRect availableGeometryForId(int id) const = 0;   //! screenForId(id)->availableGeometry()
};

}

#endif
