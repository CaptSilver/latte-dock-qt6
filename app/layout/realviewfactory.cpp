/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "realviewfactory.h"

// local
#include "genericlayout.h"
#include "../lattecorona.h"
#include "../view/originalview.h"
#include "../view/clonedview.h"
#include "../view/view.h"

// Qt
#include <QDebug>

// Plasma
#include <Plasma/Containment>

namespace Latte {
namespace Layout {

Latte::View *RealViewFactory::createView(GenericLayout *layout, const AddViewRequest &request)
{
    Plasma::Containment *containment = request.containment;
    QScreen *nextScreen = request.nextScreen;
    const bool byPassWM = request.byPassWM;
    Latte::View *latteView{nullptr};

    if (!request.isCloned) {
        latteView = new Latte::OriginalView(layout->corona(), nextScreen, byPassWM);
    } else {
        if (!request.clonedFrom) {
            qDebug().noquote() << "Adding View:" << request.viewdata.id << "- Clone did not find OriginalView and as such was stopped!!!";
            return nullptr;
        }

        Latte::OriginalView *clonedFrom = request.clonedFrom;
        latteView = new Latte::ClonedView(layout->corona(), clonedFrom, nextScreen, byPassWM);
    }

    qDebug().noquote() << "Adding View:" << request.viewdata.id << "- Passed ALL checks !!!";
    layout->registerLatteView(containment, latteView);

    //! Plasma 6 no longer restores the containment location from its config group,
    //! so apply the stored dock edge before the view initializes; otherwise the
    //! location stays Desktop and the panel geometry and form factor are wrong.
    containment->setLocation(request.viewdata.edge);

    latteView->init(containment);
    latteView->setContainment(containment);
    latteView->setLocation(request.viewdata.edge);
    latteView->setLayout(layout);

    latteView->setupWaylandLayerShell();
    latteView->show();

    return latteView;
}

}
}
