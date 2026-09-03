/*
    SPDX-FileCopyrightText: 2021 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CONTEXTMENUDATA_H
#define CONTEXTMENUDATA_H

// Qt
#include <QLatin1String>
#include <QList>
#include <QString>
#include <QStringList>

namespace Latte {
namespace Data {
namespace ContextMenu {

static const char ADDVIEWACTION[]= "_add_view";
static const char ADDWIDGETSACTION[] = "_add_latte_widgets";
static const char DUPLICATEVIEWACTION[] = "_duplicate_view"; /*used inside add view submenu*/
static const char EDITVIEWACTION[] = "_edit_view";
static const char EXPORTVIEWTEMPLATEACTION[] = "_export_view";
static const char LAYOUTSACTION[] = "_layouts";
static const char MOVEVIEWACTION[] = "_move_view";
static const char PRINTACTION[] = "_print";
static const char PREFERENCESACTION[] = "_preferences";
static const char REMOVEVIEWACTION[] = "_remove_view";
static const char QUITLATTEACTION[] = "_quit_latte";
static const char SECTIONACTION[]= "_latte_section";
static const char SEPARATOR1ACTION[] = "_separator1";

//! The context-menu payload crosses D-Bus as a positional QStringList whose fields
//! are ";;"-joined lists, and whose layouts field holds "name**isBackgroundFile**icon"
//! triples. Both separators and both halves of that encoding live here so the writer
//! in the app and the reader in the containmentactions plugin cannot drift apart --
//! the plugin gets a generated copy of this header, so it may include Qt only.
inline const QString FIELDSEPARATOR = QStringLiteral(";;");
inline const QString ENTRYSEPARATOR = QStringLiteral("**");

//! One entry of the layouts submenu: a layout name plus its icon descriptor.
struct ContextMenuLayoutEntry
{
    QString name;
    bool isBackgroundFile{false};
    QString iconName;
};

inline QString joinLayoutsMenuField(const QList<ContextMenuLayoutEntry> &entries)
{
    QStringList joined;

    for (const auto &entry : entries) {
        QStringList entrydata;
        entrydata << entry.name;
        entrydata << (entry.isBackgroundFile ? QStringLiteral("1") : QStringLiteral("0"));
        entrydata << entry.iconName;
        joined << entrydata.join(ENTRYSEPARATOR);
    }

    return joined.join(FIELDSEPARATOR);
}

inline QList<ContextMenuLayoutEntry> parseLayoutsMenuField(const QString &field)
{
    QList<ContextMenuLayoutEntry> entries;
    const QStringList layoutsdata = field.split(FIELDSEPARATOR);

    for (int i=0; i<layoutsdata.count(); ++i) {
        const QStringList cdata = layoutsdata[i].split(ENTRYSEPARATOR);

        //! A layout name may contain the field separator, which splits its entry in
        //! two and leaves a piece carrying no triple. Drop it rather than render a
        //! blank row -- an empty field also arrives here as one empty piece.
        if (cdata.count() < 3) {
            continue;
        }

        ContextMenuLayoutEntry entry;
        entry.name = cdata.at(0);
        entry.isBackgroundFile = cdata.at(1).toInt();
        entry.iconName = cdata.at(2);

        entries << entry;
    }

    return entries;
}

static QStringList ACTIONSEDITORDER = {QLatin1String(LAYOUTSACTION),
                                       QLatin1String(PREFERENCESACTION),
                                       QLatin1String(QUITLATTEACTION),
                                       QLatin1String(SEPARATOR1ACTION),
                                       QLatin1String(ADDWIDGETSACTION),
                                       QLatin1String(ADDVIEWACTION),
                                       QLatin1String(MOVEVIEWACTION),
                                       QLatin1String(EXPORTVIEWTEMPLATEACTION),
                                       QLatin1String(REMOVEVIEWACTION)};

static QStringList ACTIONSALWAYSVISIBLE = {QLatin1String(LAYOUTSACTION),
                                           QLatin1String(PREFERENCESACTION),
                                           QLatin1String(QUITLATTEACTION),
                                           QLatin1String(SEPARATOR1ACTION),
                                           QLatin1String(ADDWIDGETSACTION),
                                           QLatin1String(ADDVIEWACTION)};

static QStringList ACTIONSALWAYSHIDDEN = {QLatin1String(PRINTACTION)};

static QStringList ACTIONSVISIBLEONLYINEDIT = {QLatin1String(MOVEVIEWACTION),
                                               QLatin1String(EXPORTVIEWTEMPLATEACTION),
                                               QLatin1String(REMOVEVIEWACTION)};

static QStringList ACTIONSSPECIAL = {QLatin1String(SECTIONACTION),
                                     QLatin1String(EDITVIEWACTION)};

}
}
}

#endif
