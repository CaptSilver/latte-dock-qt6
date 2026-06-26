/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef STARTUPLAYOUTPLANNER_H
#define STARTUPLAYOUTPLANNER_H

// local
#include "apptypes.h"

// C++
#include <optional>

// Qt
#include <QString>
#include <QStringList>

namespace Latte {

struct StartupInputs
{
    int userSetMemoryUsage = -1;                                         //! CLI override; -1 means unset
    MemoryUsage::LayoutsMemory currentMemoryUsage = MemoryUsage::SingleLayout;
    QString singleModeLayoutName;
    bool defaultLayoutOnStartup = false;
    QString layoutNameOnStartUp;
    QString defaultLayoutTemplateName;                                  //! i18n(Templates::DEFAULTLAYOUTTEMPLATENAME)
    QStringList existingLayoutNames;                                    //! names the synchronizer already knows
};

struct StartupPlan
{
    std::optional<MemoryUsage::LayoutsMemory> memoryUsageToSet;         //! persist via setLayoutsMemoryUsage when set
    bool createFreshDefaultLayout = false;                             //! force a brand-new Default (defaultLayoutOnStartup)
    bool ensureDefaultLayoutExists = false;                           //! create Default only if missing, with setOnAllActivities
    QString loadLayoutName;                                            //! empty => multiple-layouts mode; else the name to load
    bool loadResolvedAfterCreate = false;                            //! loadLayoutName comes from the freshly created path
};

class StartupLayoutPlanner
{
public:
    static StartupPlan plan(const StartupInputs &in);
};

}

#endif
