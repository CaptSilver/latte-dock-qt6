/*
    SPDX-FileCopyrightText: 2016 Smith AR <audoban@openmailbox.org>
    SPDX-FileCopyrightText: 2016 Michail Vourlakos <mvourlakos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lattecoreplugin.h"

// local
#include "dialog.h"
#include "environment.h"
#include "iconitem.h"
#include "quickwindowsystem.h"
#include "tools.h"

#include <types.h>

// Qt
#include <QtQml>


void LatteCorePlugin::registerTypes(const char *uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("org.kde.latte.core"));
    // Qt6 warns "Invalid QML element name 'Types'" (value-type names want lowercase).
    // Kept uppercase deliberately: the public QML API is LatteCore.Types, used in 595+ call
    // sites; renaming for a benign, non-fatal warning is not worth the churn.
    qmlRegisterUncreatableType<Latte::Types>(uri, 0, 2, "Types", QStringLiteral("Latte Types uncreatable"));
    qmlRegisterType<Latte::IconItem>(uri, 0, 2, "IconItem");
    qmlRegisterType<Latte::Quick::Dialog>(uri, 0, 2, "Dialog");
    qmlRegisterSingletonType<Latte::Environment>(uri, 0, 2, "Environment", &Latte::environment_qobject_singletontype_provider);
    qmlRegisterSingletonType<Latte::Tools>(uri, 0, 2, "Tools", &Latte::tools_qobject_singletontype_provider);
    qmlRegisterSingletonType<Latte::QuickWindowSystem>(uri, 0, 2, "WindowSystem", &Latte::windowsystem_qobject_singletontype_provider);
}
