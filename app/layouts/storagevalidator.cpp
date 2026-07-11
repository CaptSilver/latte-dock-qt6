/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "storagevalidator.h"

#include "../data/errorinformationdata.h"

#include <QLatin1String>
#include <QSet>
#include <QStringList>

#include <KConfigGroup>

namespace Latte {
namespace Layouts {
namespace StorageValidator {

LayoutModel buildFromConfig(const KConfigGroup &containments,
                            const std::function<int(const KConfigGroup &)> &subIdOfApplet)
{
    LayoutModel model;

    for (const auto &cId : containments.groupList()) {
        KConfigGroup cGroup = containments.group(cId);

        ContainmentModel cm;
        cm.id = cId;
        cm.pluginId = cGroup.readEntry(QStringLiteral("plugin"), QString());
        cm.isLatte = (cm.pluginId == QLatin1String("org.kde.latte.containment"));

        KConfigGroup appletsGroup = cGroup.group(QStringLiteral("Applets"));
        for (const auto &aId : appletsGroup.groupList()) {
            AppletModel am;
            am.id = aId;
            am.pluginId = appletsGroup.group(aId).readEntry(QStringLiteral("plugin"), QString());
            am.subContainmentId = subIdOfApplet(appletsGroup.group(aId));
            cm.applets << am;
        }

        model.containments << cm;
    }

    return model;
}

}
}
}
