/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ADDVIEWDECISION_H
#define ADDVIEWDECISION_H

// local
#include <coretypes.h>

namespace Latte {
namespace Layout {

struct AddViewInputs
{
    bool onPrimary{true};
    int screenId{0};
    bool screenIdValid{false};                              //! Layouts::Storage::isValid(screenId)
    bool screenActive{false};                               //! screenPool->isScreenActive(screenId)
    Latte::Types::Visibility visibilityMode{Latte::Types::DodgeActive};
    bool configByPassWM{false};                             //! containment config "byPassWM"
};

struct AddViewDecision
{
    bool reject{false};            //! non-primary view on an inactive valid screen -> skip
    bool useExplicitScreen{false}; //! true => caller uses screenForId(screenId); false => primary
    bool byPassWM{false};
};

//! Pure add-view decision: explicit-screen resolution / reject, and the visibility-mode ->
//! byPassWM mapping. Extracted from GenericLayout::addView.
class AddViewDecisionMaker
{
public:
    static AddViewDecision decide(const AddViewInputs &in);
};

}
}

#endif
