/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWPRIORITY_H
#define VIEWPRIORITY_H

// local
#include "../data/viewdata.h"

// Qt
#include <QList>

namespace Latte {
namespace Layout {

//! Pure ordering of Latte views from their value data (screen, primary flag, active
//! state, screen edge). Extracted from GenericLayout so it can be unit-tested without
//! a live View/Corona graph.
class ViewPriority
{
public:
    static bool atLowerScreenPriority(const Latte::Data::View &test, const Latte::Data::View &base);
    static bool atLowerStatePriority(const Latte::Data::View &test, const Latte::Data::View &base);
    static bool atLowerEdgePriority(const Latte::Data::View &test, const Latte::Data::View &base);

    static QList<Latte::Data::View> sorted(const QList<Latte::Data::View> &viewsData);
};

}
}

#endif
