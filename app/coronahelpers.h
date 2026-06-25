/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CORONAHELPERS_H
#define CORONAHELPERS_H

// Qt
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// KDE
#include <KConfigGroup>

namespace Latte {

//! Pure logic lifted out of Latte::Corona so it can be unit-tested without a
//! live Corona/View graph.
namespace CoronaHelpers {

//! The two parts of the "<windowId>-<colorScheme>" payload of the window color
//! scheme dbus signal.
struct WindowIdAndScheme
{
    QString windowId;
    QString scheme;
};

//! One entry of the layouts submenu: a layout name plus its icon descriptor.
struct ContextMenuLayoutEntry
{
    QString name;
    bool isBackgroundFile{false};
    QString iconName;
};

//! Everything buildContextMenuData needs, gathered by Corona from the live
//! layout managers and the selected view.
struct ContextMenuInputs
{
    int memoryUsage{0};
    QStringList centralLayoutsNames;
    QStringList currentLayoutsNames;
    QStringList alwaysShownActions;
    QList<ContextMenuLayoutEntry> menuLayouts;
    QString selectedViewLayoutName;
    int viewType{0};
    bool viewIsOriginal{false};
    bool viewIsCloned{false};
    int viewClonesCount{0};
};

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

//! Marshal the context-menu payload that the menu QML parses: memory usage,
//! the active/current layout name lists, the always-shown actions, the layouts
//! submenu, the selected view's layout, and the view-type triple. Fields are
//! ";;"-joined lists; each submenu entry is "name**isBackgroundFile**icon".
QStringList buildContextMenuData(const ContextMenuInputs &inputs);

//! Split a "<windowId>-<colorScheme>" string on its first dash. With no dash
//! both fields hold the whole string (mirrors the original indexOf(-1) path).
WindowIdAndScheme parseWindowIdAndScheme(const QString &windowIdAndScheme);

//! Return page when it lies within [firstPage, lastPage], otherwise firstPage.
//! Used to validate a requested settings-dialog page against its enum range.
int validPageOrFirst(int page, int firstPage, int lastPage);

}

}

#endif // CORONAHELPERS_H
