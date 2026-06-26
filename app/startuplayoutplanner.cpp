/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "startuplayoutplanner.h"

namespace Latte {

StartupPlan StartupLayoutPlanner::plan(const StartupInputs &in)
{
    StartupPlan plan;

    //! the CLI override, when present, is persisted and also decides the effective mode below
    const MemoryUsage::LayoutsMemory effectiveMemory = (in.userSetMemoryUsage != -1)
        ? static_cast<MemoryUsage::LayoutsMemory>(in.userSetMemoryUsage)
        : in.currentMemoryUsage;

    if (in.userSetMemoryUsage != -1) {
        plan.memoryUsageToSet = effectiveMemory;
    }

    if (!in.defaultLayoutOnStartup && in.layoutNameOnStartUp.isEmpty()) {
        if (effectiveMemory == MemoryUsage::MultipleLayouts) {
            plan.loadLayoutName = QString();
        } else {
            if (in.existingLayoutNames.contains(in.singleModeLayoutName)) {
                plan.loadLayoutName = in.singleModeLayoutName;
            } else {
                //! chosen single layout is gone; fall back to the Default template, creating it if absent
                plan.loadLayoutName = in.defaultLayoutTemplateName;
                plan.ensureDefaultLayoutExists = !in.existingLayoutNames.contains(in.defaultLayoutTemplateName);
            }
        }
    } else if (in.defaultLayoutOnStartup) {
        //! force a fresh Default even if one already exists; load name comes from the created path
        plan.createFreshDefaultLayout = true;
        plan.loadResolvedAfterCreate = true;
        plan.memoryUsageToSet = MemoryUsage::SingleLayout;
    } else {
        plan.loadLayoutName = in.layoutNameOnStartUp;
        plan.memoryUsageToSet = MemoryUsage::SingleLayout;
    }

    return plan;
}

}
