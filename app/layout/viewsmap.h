/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LAYOUT_VIEWSMAP_H
#define LAYOUT_VIEWSMAP_H

// Qt
#include <QHash>
#include <QList>
#include <QString>

// Plasma
#include <Plasma/Plasma>

namespace Latte {
namespace Layout {

//! views map structure: SCREEN_NAME -> EDGE -> VIEWID
typedef QHash<QString, QHash<Plasma::Types::Location, QList<uint>>> ViewsMap;

}
}

#endif
