/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SETTINGSNAMEUTILS_H
#define SETTINGSNAMEUTILS_H

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace Latte {
namespace Settings {

// Exact, case-sensitive existence check — matches GenericTable::containsName / Importer::layoutExists.
inline bool nameExists(const QStringList &existingNames, const QString &name)
{
    return existingNames.contains(name);
}

// The shared uniqueness algorithm behind Layouts::uniqueLayoutName and Views::uniqueViewName.
// Strips an existing " - <N>" suffix only when the full name already exists AND pos_ > 0
// (strictly greater-than, so a name that IS " - 3" at position 0 is not stripped).
inline QString uniqueName(QString name, const QStringList &existingNames)
{
    int pos_ = name.lastIndexOf(QRegularExpression(QStringLiteral(" - [0-9]+")));
    if (nameExists(existingNames, name) && pos_ > 0) {
        name = name.left(pos_);
    }
    int i = 2;
    QString namePart = name;
    while (nameExists(existingNames, name)) {
        name = namePart + QStringLiteral(" - ") + QString::number(i);
        i++;
    }
    return name;
}

// First match, -1 on miss — the negative-row contract that callers rely on via `if (row >= 0)`.
inline int rowForValue(const QStringList &columnValues, const QString &needle)
{
    for (int i = 0; i < columnValues.count(); ++i) {
        if (columnValues[i] == needle) {
            return i;
        }
    }
    return -1;
}

// Views::pasteSelectedViews decision helpers.
// Returns true when a clipboard view should be skipped because it is a move-origin
// whose origin layout is the current paste target (it is already here).
inline bool pasteSkipsView(bool isMoveOrigin, const QString &viewOriginLayout, const QString &currentLayoutId)
{
    return isMoveOrigin && (viewOriginLayout == currentLayoutId);
}

// Returns true when a clipboard view is a cut (move-origin), meaning the paste
// should flip its flags to isMoveDestination instead of keeping isMoveOrigin.
inline bool pasteTurnsCutIntoMove(bool isMoveOrigin)
{
    return isMoveOrigin;
}

} // namespace Settings
} // namespace Latte

#endif // SETTINGSNAMEUTILS_H
