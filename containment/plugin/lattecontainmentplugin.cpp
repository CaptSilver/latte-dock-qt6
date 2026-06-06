/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lattecontainmentplugin.h"

// local
#include "layoutmanager.h"
#include "types.h"

// Qt
#include <QtQml>

void LatteContainmentPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("org.kde.latte.private.containment"));
    // Qt6 warns "Invalid QML element name 'Types'" (value-type names want lowercase).
    // Kept uppercase deliberately: the public QML API is LatteContainment.Types, used across
    // the QML tree; renaming for a benign, non-fatal warning is not worth the churn.
    qmlRegisterUncreatableType<Latte::Containment::Types>(uri, 0, 1, "Types", QStringLiteral("Latte Containment Types uncreatable"));
    qmlRegisterType<Latte::Containment::LayoutManager>(uri, 0, 1, "LayoutManager");
}

