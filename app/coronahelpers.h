/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CORONAHELPERS_H
#define CORONAHELPERS_H

// Qt
#include <QHash>
#include <QSet>
#include <QString>

// KDE
#include <KConfigGroup>

namespace Latte {

//! Pure logic lifted out of Latte::Corona so it can be unit-tested without a
//! live Corona/View graph.
namespace CoronaHelpers {

//! True when path points to a Latte .layout.latte file, given either as an
//! absolute filesystem path or a file: URL.
bool isLayoutFilePath(const QString &path);

//! Strip a file:// / file:/// scheme prefix from a layout file path, leaving an
//! absolute filesystem path. Paths without the scheme are returned unchanged.
QString cleanLayoutFilePath(const QString &path);

//! Delete obsolete groups from a Corona "Containments" config group: any
//! containment id not in liveContainmentIds, and within a surviving containment,
//! any applet id not in liveAppletIds[containmentId]. Returns true if anything
//! was deleted (so the caller knows whether to sync).
bool pruneObsoleteContainmentConfig(KConfigGroup &containments,
                                    const QSet<uint> &liveContainmentIds,
                                    const QHash<uint, QSet<uint>> &liveAppletIds);

}

}

#endif // CORONAHELPERS_H
