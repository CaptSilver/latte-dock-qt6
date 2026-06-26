/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "addviewdecision.h"

namespace Latte {
namespace Layout {

AddViewDecision AddViewDecisionMaker::decide(const AddViewInputs &in)
{
    AddViewDecision decision;

    //! the always-visible family ignores the byPassWM config and never bypasses the WM
    if (in.visibilityMode == Latte::Types::AlwaysVisible
            || in.visibilityMode == Latte::Types::WindowsGoBelow
            || in.visibilityMode == Latte::Types::WindowsCanCover
            || in.visibilityMode == Latte::Types::WindowsAlwaysCover) {
        decision.byPassWM = false;
    } else {
        decision.byPassWM = in.configByPassWM;
    }

    //! a non-primary view tied to a valid screen needs that screen active, else it is skipped
    if (!in.onPrimary && in.screenIdValid) {
        if (in.screenActive) {
            decision.useExplicitScreen = true;
        } else {
            decision.reject = true;
        }
    }

    return decision;
}

}
}
