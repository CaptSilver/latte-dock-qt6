/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "storageidremapper.h"

namespace Latte {
namespace Layouts {

const int StorageIdRemapper::CONTAINMENTIDBASE = 12;
const int StorageIdRemapper::APPLETIDBASE = 40;
const int StorageIdRemapper::MAXID = 32000;

// Verbatim port of Storage::availableId (storage.cpp).
// Scans the snapshot lists by value so callers can pass evolving state
// without risk of the helper mutating it.
QString StorageIdRemapper::availableId(QStringList all, QStringList assigned, int base)
{
    bool found = false;

    int i = base;

    while (!found && i < 32000) {
        QString iStr = QString::number(i);

        if (!all.contains(iStr) && !assigned.contains(iStr)) {
            return iStr;
        }

        i++;
    }

    return QStringLiteral("");
}

// Verbatim port of the assignment + 2-cycle-fix section of
// Storage::newUniqueIdsFile (storage.cpp ~:399-455).
// qDebug() lines are omitted (diagnostic only, not behavioral).
IdRemap StorageIdRemapper::remap(const IdRemapInput &input)
{
    QStringList assignedIds;
    QHash<QString, QString> assigned;

    //! Reassign containment ids to unique ones
    for (const auto &contId : input.containmentIds) {
        QString newId;

        if (contId.toInt() >= 12 && !input.usedIds.contains(contId) && !assignedIds.contains(contId)) {
            newId = contId;
        } else {
            newId = availableId(input.usedIds, assignedIds, 12);
        }

        assignedIds << newId;
        assigned[contId] = newId;
    }

    //! Reassign applet ids to unique ones (shares the running assignedIds from above)
    for (const auto &appId : input.appletIds) {
        QString newId;

        if (appId.toInt() >= 40 && !input.usedIds.contains(appId) && !assignedIds.contains(appId)) {
            newId = appId;
        } else {
            newId = availableId(input.usedIds, assignedIds, 40);
        }

        assignedIds << newId;
        assigned[appId] = newId;
    }

    //! "PROBLEM APPEARED" 2-cycle fix — port of storage.cpp ~:429-455.
    //! Collapses a strict 2-cycle (cId -> B -> cId) to identity on both sides.
    for (const auto &cId : input.containmentIds) {
        QString value = assigned[cId];

        if (assigned.contains(value)) {
            QString value2 = assigned[value];

            if (cId != assigned[cId] && !value2.isEmpty() && cId == value2) {
                assigned[cId] = cId;
                assigned[value] = value;
            }
        }
    }

    for (const auto &aId : input.appletIds) {
        QString value = assigned[aId];

        if (assigned.contains(value)) {
            QString value2 = assigned[value];

            if (aId != assigned[aId] && !value2.isEmpty() && aId == value2) {
                assigned[aId] = aId;
                assigned[value] = value;
            }
        }
    }

    IdRemap r;
    r.assigned = assigned;
    return r;
}

} // namespace Layouts
} // namespace Latte
