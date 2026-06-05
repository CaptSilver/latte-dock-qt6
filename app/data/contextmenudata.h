/*
    SPDX-FileCopyrightText: 2021 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CONTEXTMENUDATA_H
#define CONTEXTMENUDATA_H

// Qt
#include <QLatin1String>
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
