/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IVIEWFACTORY_H
#define IVIEWFACTORY_H

// local
#include "../data/viewdata.h"

class QScreen;

namespace Plasma {
class Containment;
}

namespace Latte {
class View;
class OriginalView;
namespace Layout {
class GenericLayout;

//! Everything the factory needs to build and wire one view, gathered by GenericLayout::addView.
struct AddViewRequest
{
    Plasma::Containment *containment{nullptr};
    Latte::Data::View viewdata;
    QScreen *nextScreen{nullptr};
    bool byPassWM{false};
    bool isCloned{false};
    Latte::OriginalView *clonedFrom{nullptr};   //! valid when isCloned
};

//! Seam over View construction so GenericLayout's add-view orchestration is decoupled from the
//! QML/Wayland-backed View objects. createView builds the View, registers it on the layout
//! (store-before-wire) and wires it; returns nullptr if it cannot be created.
class IViewFactory
{
public:
    virtual ~IViewFactory() = default;
    virtual Latte::View *createView(GenericLayout *layout, const AddViewRequest &request) = 0;
};

}
}

#endif
