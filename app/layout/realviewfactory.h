/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef REALVIEWFACTORY_H
#define REALVIEWFACTORY_H

// local
#include "iviewfactory.h"

namespace Latte {
namespace Layout {

//! Production IViewFactory: constructs OriginalView/ClonedView and wires them, exactly as
//! GenericLayout::addView used to inline.
class RealViewFactory : public IViewFactory
{
public:
    Latte::View *createView(GenericLayout *layout, const AddViewRequest &request) override;
};

}
}

#endif
