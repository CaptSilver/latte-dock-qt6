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

bool differentAppletsWithSameId(const LayoutModel &model, const MetadataResolver &resolve, Data::Error &error)
{
    QStringList registeredapplets;
    QStringList conflictedapplets;

    for (const auto &c : model.containments) {
        for (const auto &a : c.applets) {
            if (!registeredapplets.contains(a.id)) {
                registeredapplets << a.id;
            } else if (!conflictedapplets.contains(a.id)) {
                conflictedapplets << a.id;
            }
        }
    }

    for (const auto &c : model.containments) {
        for (const auto &a : c.applets) {
            if (!conflictedapplets.contains(a.id)) {
                continue;
            }

            Data::ErrorInformation errorinfo;
            errorinfo.id = QString::number(error.information.rowCount());
            errorinfo.containment = resolve(c.pluginId);
            errorinfo.containment.storageId = c.id;
            errorinfo.applet = resolve(a.pluginId);
            errorinfo.applet.storageId = a.id;

            error.information << errorinfo;
        }
    }

    return !error.information.isEmpty();
}

bool appletsAndContainmentsWithSameId(const LayoutModel &model, const MetadataResolver &resolve, Data::Warning &warning)
{
    QStringList registeredcontainments;
    QStringList conflicted;

    for (const auto &c : model.containments) {
        if (registeredcontainments.contains(c.id)) {
            continue;
        }
        registeredcontainments << c.id;
    }

    for (const auto &c : model.containments) {
        for (const auto &a : c.applets) {
            if (!registeredcontainments.contains(a.id)) {
                continue;
            } else if (!conflicted.contains(a.id)) {
                conflicted << a.id;
            }
        }
    }

    for (const auto &c : model.containments) {
        if (conflicted.contains(c.id)) {
            Data::WarningInformation warninginfo;
            warninginfo.id = QString::number(warning.information.rowCount());
            warninginfo.containment = resolve(c.pluginId);
            warninginfo.containment.storageId = c.id;
            warning.information << warninginfo;
        }

        for (const auto &a : c.applets) {
            if (!conflicted.contains(a.id)) {
                continue;
            }
            Data::WarningInformation warninginfo;
            warninginfo.id = QString::number(warning.information.rowCount());
            warninginfo.containment = resolve(c.pluginId);
            warninginfo.containment.storageId = c.id;
            warninginfo.applet = resolve(a.pluginId);
            warninginfo.applet.storageId = a.id;
            warning.information << warninginfo;
        }
    }

    return !warning.information.isEmpty();
}

bool orphanedParentApplets(const LayoutModel &model, const MetadataResolver &resolve, Data::Error &error)
{
    QSet<int> presentContainmentIds;
    for (const auto &c : model.containments) {
        presentContainmentIds.insert(c.id.toInt());
    }

    for (const auto &c : model.containments) {
        for (const auto &a : c.applets) {
            const int subid = a.subContainmentId;

            if (subid == IDNULL || presentContainmentIds.contains(subid)) {
                continue;
            }

            Data::ErrorInformation errorinfo;
            errorinfo.id = QString::number(error.information.rowCount());
            errorinfo.containment = resolve(c.pluginId);
            errorinfo.containment.storageId = c.id;
            errorinfo.applet = resolve(a.pluginId);
            errorinfo.applet.storageId = a.id;
            errorinfo.applet.subcontainmentId = QString::number(subid);

            error.information << errorinfo;
        }
    }

    return !error.information.isEmpty();
}

bool orphanedSubcontainments(const LayoutModel &model, const Data::ViewsTable &views, const MetadataResolver &resolve, Data::Warning &warning)
{
    for (const auto &c : model.containments) {
        if (views.hasContainmentId(c.id)) {
            continue;
        }

        Data::WarningInformation warninginfo;
        warninginfo.id = QString::number(warning.information.rowCount());
        warninginfo.containment = resolve(c.pluginId);
        warninginfo.containment.storageId = c.id;
        warning.information << warninginfo;
    }

    return !warning.information.isEmpty();
}

}
}
}
