/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace Latte {
namespace Layouts {

struct IdRemapInput {
    QStringList usedIds;        // ids already taken (corona containment+applet ids, or file-derived)
    QStringList containmentIds; // toInvestigateContainmentIds, in groupList() order
    QStringList appletIds;      // toInvestigateAppletIds, in order
};

struct IdRemap {
    QHash<QString, QString> assigned; // old id -> new id

    // Pass-through for unknown keys (unknown old id → returned as-is).
    QString mapped(const QString &oldId) const { return assigned.value(oldId, oldId); }
};

// Pure port of the id-assignment algorithm from Storage::newUniqueIdsFile.
// Lifts just the assignment math (no KConfig I/O, no Corona graph) so it
// can be unit-tested and reasoned about in isolation.
class StorageIdRemapper
{
public:
    static const int CONTAINMENTIDBASE; // 12
    static const int APPLETIDBASE;      // 40
    static const int MAXID;             // 32000 (exhaustion cap)

    // Exact port of Storage::availableId — lowest i>=base (i<MAXID) whose
    // QString::number(i) is in NEITHER `all` NOR `assigned`; "" on exhaustion.
    // Both lists are passed by value, matching the original signature.
    static QString availableId(QStringList all, QStringList assigned, int base);

    // The full assignment: walk containmentIds at CONTAINMENTIDBASE then
    // appletIds at APPLETIDBASE, keeping ONE running assigned-set across both
    // passes; then the "PROBLEM APPEARED" 2-cycle fix. Returns old->new map.
    static IdRemap remap(const IdRemapInput &input);
};

} // namespace Layouts
} // namespace Latte
