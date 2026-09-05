/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "coronahelpers.h"

namespace Latte {

namespace CoronaHelpers {

QString strippedLatteName(const QString &path, const QStringList &extensions)
{
    const QString name = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);

    for (const QString &extension : extensions) {
        if (name.endsWith(extension)) {
            return name.chopped(extension.size());
        }
    }

    return name;
}

bool isLayoutFilePath(const QString &path)
{
    return (path.startsWith(QLatin1String("file:/")) || path.startsWith(QLatin1String("/"))) && path.endsWith(LAYOUTEXTENSION);
}

QString cleanLayoutFilePath(const QString &path)
{
    QString layoutPath = path;

    if (layoutPath.startsWith(QLatin1String("file:///"))) {
        layoutPath = layoutPath.remove(QLatin1String("file://"));
    } else if (layoutPath.startsWith(QLatin1String("file://"))) {
        layoutPath = layoutPath.remove(QLatin1String("file:/"));
    }

    return layoutPath;
}

bool pruneObsoleteContainmentConfig(KConfigGroup &containments,
                                    const QSet<uint> &liveContainmentIds,
                                    const QHash<uint, QSet<uint>> &liveAppletIds)
{
    bool changed = false;

    for (const auto &cId : containments.groupList()) {
        if (!liveContainmentIds.contains(cId.toUInt())) {
            //! cleanup obsolete containments
            containments.group(cId).deleteGroup();
            changed = true;
        } else {
            //! cleanup obsolete applets of surviving containments
            const QSet<uint> liveApplets = liveAppletIds.value(cId.toUInt());
            auto appletsEntries = containments.group(cId).group(QStringLiteral("Applets"));

            for (const auto &appletId : appletsEntries.groupList()) {
                if (!liveApplets.contains(appletId.toUInt())) {
                    appletsEntries.group(appletId).deleteGroup();
                    changed = true;
                }
            }
        }
    }

    return changed;
}

QStringList buildContextMenuData(const ContextMenuInputs &inputs)
{
    QStringList data;

    data << QString::number(inputs.memoryUsage);
    data << inputs.centralLayoutsNames.join(Latte::Data::ContextMenu::FIELDSEPARATOR);
    data << inputs.currentLayoutsNames.join(Latte::Data::ContextMenu::FIELDSEPARATOR);
    data << inputs.alwaysShownActions.join(Latte::Data::ContextMenu::FIELDSEPARATOR);

    data << Latte::Data::ContextMenu::joinLayoutsMenuField(inputs.menuLayouts);
    data << inputs.selectedViewLayoutName;

    QStringList viewtype;
    viewtype << QString::number(inputs.viewType);

    if (inputs.viewIsOriginal) {
        viewtype << QStringLiteral("0");
        viewtype << QString::number(inputs.viewClonesCount);
    } else if (inputs.viewIsCloned) {
        viewtype << QStringLiteral("1");
        viewtype << QStringLiteral("0");
    } else {
        viewtype << QStringLiteral("0");
        viewtype << QStringLiteral("0");
    }

    data << viewtype.join(Latte::Data::ContextMenu::FIELDSEPARATOR);

    return data;
}

WindowIdAndScheme parseWindowIdAndScheme(const QString &windowIdAndScheme)
{
    const int firstDash = windowIdAndScheme.indexOf(QLatin1Char('-'));

    WindowIdAndScheme result;
    result.windowId = windowIdAndScheme.mid(0, firstDash);
    result.scheme = windowIdAndScheme.mid(firstDash + 1);
    return result;
}

int validPageOrFirst(int page, int firstPage, int lastPage)
{
    return (page >= firstPage && page <= lastPage) ? page : firstPage;
}

}

}
