/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FAKESCREENINFO_H
#define FAKESCREENINFO_H

#include "../app/iscreeninfo.h"

#include <QHash>
#include <QRect>

//! Canned screen data so the engine's geometry math runs without a live screen graph.
class FakeScreenInfo : public Latte::IScreenInfo
{
public:
    int count = 1;
    QHash<int, QRect> geometries;   //! id -> geometry (drives geometry() and the *ForId() trio)

    int numScreens() const override { return count; }
    QRect screenGeometry(int id) const override { return geometries.value(id); }
    bool hasScreenForId(int id) const override { return geometries.contains(id); }
    QRect geometryForId(int id) const override { return geometries.value(id); }
    QRect availableGeometryForId(int id) const override { return geometries.value(id); }
};

#endif
