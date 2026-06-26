/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef REALSCREENINFO_H
#define REALSCREENINFO_H

// local
#include "iscreeninfo.h"

namespace Latte {

class ScreenPool;

//! Production IScreenInfo: the live qApp->screens() + ScreenPool reads Corona used to do inline.
class RealScreenInfo : public IScreenInfo
{
public:
    explicit RealScreenInfo(ScreenPool *screenPool);

    int numScreens() const override;
    QRect screenGeometry(int id) const override;
    bool hasScreenForId(int id) const override;
    QRect geometryForId(int id) const override;
    QRect availableGeometryForId(int id) const override;

private:
    ScreenPool *m_screenPool{nullptr};
};

}

#endif
