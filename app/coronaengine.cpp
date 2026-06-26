/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "coronaengine.h"

// local
#include "lattecorona.h"
#include "iscreeninfo.h"
#include "realscreeninfo.h"
#include "screenpool.h"
#include "indicator/factory.h"
#include "layouts/manager.h"
#include "layouts/synchronizer.h"
#include "view/view.h"
#include "plasma/extended/screengeometries.h"
#include "plasma/extended/screenpool.h"
#include "plasma/extended/theme.h"
#include "settings/universalsettings.h"
#include "shortcuts/globalshortcuts.h"
#include "templates/templatesmanager.h"
#include "view/panelshadows_p.h"
#include "view/settings/viewsettingsfactory.h"
#include "wm/abstractwindowinterface.h"
#include "wm/waylandinterface.h"

// Qt
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>

// KDE
#include <PlasmaActivities/Consumer>
#include <KWindowSystem>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/plasmawindowmanagement.h>

namespace Latte {

namespace {

//! Capture the View properties that the available-screen geometry math reads,
//! so the calculation can run against plain values instead of the live View.
ViewFootprint footprintForView(const View *view)
{
    ViewFootprint footprint;
    footprint.location = view->location();
    footprint.formFactor = view->formFactor();
    footprint.alignment = static_cast<Types::Alignment>(view->alignment());
    footprint.hasVisibility = (view->visibility() != nullptr);
    footprint.visibilityMode = footprint.hasVisibility ? view->visibility()->mode() : Types::None;
    footprint.isOffScreen = view->positioner() && view->positioner()->isOffScreen();
    footprint.behaveAsPlasmaPanel = view->behaveAsPlasmaPanel();
    footprint.normalThickness = view->normalThickness();
    footprint.screenEdgeMargin = view->screenEdgeMargin();
    footprint.maxLength = view->maxLength();
    footprint.offset = view->offset();
    footprint.geometry = view->geometry();
    return footprint;
}

}

CoronaEngine::CoronaEngine(Latte::Corona *shell)
    : CoronaEngine(shell, Deps{})
{
}

CoronaEngine::CoronaEngine(Latte::Corona *shell, const Deps &deps)
    : QObject(nullptr),
      m_shell(shell)
{
    //! Hand the shell its engine pointer BEFORE any collaborator is constructed, so that a
    //! collaborator whose ctor reaches back through corona()->someAccessor() (GlobalShortcuts
    //! resolves universalSettings() while connecting) sees a live engine, matching the old
    //! direct-member reads.
    if (m_shell) {
        m_shell->m_engine = this;
    }

    KSharedConfig::Ptr config = deps.config ? deps.config : KSharedConfig::openConfig();

    m_activitiesConsumer = new KActivities::Consumer(this);
    m_screenPool = new ScreenPool(config, this);
    m_screenInfo = deps.screenInfo ? deps.screenInfo : new RealScreenInfo(m_screenPool);
    m_ownsScreenInfo = (deps.screenInfo == nullptr);
    m_indicatorFactory = new Indicator::Factory(this);
    m_universalSettings = new UniversalSettings(config, m_shell, this);
    m_globalShortcuts = new GlobalShortcuts(m_shell, this);
    m_plasmaScreenPool = new PlasmaExtended::ScreenPool(this);
    m_themeExtended = new PlasmaExtended::Theme(config, this);
    m_viewSettingsFactory = new ViewSettingsFactory(this);
    m_templatesManager = new Templates::Manager(m_shell, this);
    m_layoutsManager = new Layouts::Manager(m_shell, this);
    m_plasmaGeometries = new PlasmaExtended::ScreenGeometries(m_shell, this);
    m_dialogShadows = new PanelShadows(this, QStringLiteral("dialogs/background"));

    //! Wayland-only: the X11 backend was removed in the Plasma 6 port.
    m_wm = deps.wm ? deps.wm : new WindowSystem::WaylandInterface(this);
}

CoronaEngine::~CoronaEngine()
{
    m_plasmaGeometries->deleteLater();
    m_wm->deleteLater();
    m_dialogShadows->deleteLater();
    m_globalShortcuts->deleteLater();
    m_layoutsManager->deleteLater();
    m_screenPool->deleteLater();
    m_universalSettings->deleteLater();
    m_plasmaScreenPool->deleteLater();
    m_themeExtended->deleteLater();
    m_indicatorFactory->deleteLater();

    delete m_activitiesConsumer;

    //! IScreenInfo is a plain (non-QObject) helper; an injected one belongs to the caller.
    if (m_ownsScreenInfo) {
        delete m_screenInfo;
    }

    qDebug() << "Latte Corona engine - deleted...";
}

void CoronaEngine::init()
{
    setupWaylandIntegration();

    m_screenPool->load();

    //! universal settings / extendedtheme must be loaded after the package has been set
    m_universalSettings->load();
    m_themeExtended->load();
}

void CoronaEngine::setupWaylandIntegration()
{
    if (!KWindowSystem::isPlatformWayland()) {
        return;
    }

    using namespace KWayland::Client;

    auto connection = ConnectionThread::fromApplication(this);

    if (!connection) {
        return;
    }

    Registry *registry{new Registry(this)};
    registry->create(connection);

    connect(registry, &Registry::plasmaShellAnnounced, this
            , [this, registry](quint32 name, quint32 version) {
        m_waylandCorona = registry->createPlasmaShell(name, version, this);
    });

    QObject::connect(registry, &KWayland::Client::Registry::plasmaWindowManagementAnnounced,
                     [this, registry](quint32 name, quint32 version) {
        KWayland::Client::PlasmaWindowManagement *pwm = registry->createPlasmaWindowManagement(name, version, this);

        WindowSystem::WaylandInterface *wI = qobject_cast<WindowSystem::WaylandInterface *>(m_wm);

        if (wI) {
            wI->initWindowManagement(pwm);
        }
    });


    QObject::connect(registry, &KWayland::Client::Registry::plasmaVirtualDesktopManagementAnnounced,
                     [this, registry] (quint32 name, quint32 version) {
        KWayland::Client::PlasmaVirtualDesktopManagement *vdm = registry->createPlasmaVirtualDesktopManagement(name, version, this);

        WindowSystem::WaylandInterface *wI = qobject_cast<WindowSystem::WaylandInterface *>(m_wm);

        if (wI) {
            wI->initVirtualDesktopManagement(vdm);
        }
    });


    registry->setup();
    connection->roundtrip();
}

KActivities::Consumer *CoronaEngine::activitiesConsumer() const
{
    return m_activitiesConsumer;
}

ScreenPool *CoronaEngine::screenPool() const
{
    return m_screenPool;
}

Indicator::Factory *CoronaEngine::indicatorFactory() const
{
    return m_indicatorFactory;
}

UniversalSettings *CoronaEngine::universalSettings() const
{
    return m_universalSettings;
}

GlobalShortcuts *CoronaEngine::globalShortcuts() const
{
    return m_globalShortcuts;
}

PlasmaExtended::ScreenPool *CoronaEngine::plasmaScreenPool() const
{
    return m_plasmaScreenPool;
}

PlasmaExtended::Theme *CoronaEngine::themeExtended() const
{
    return m_themeExtended;
}

ViewSettingsFactory *CoronaEngine::viewSettingsFactory() const
{
    return m_viewSettingsFactory;
}

Templates::Manager *CoronaEngine::templatesManager() const
{
    return m_templatesManager;
}

Layouts::Manager *CoronaEngine::layoutsManager() const
{
    return m_layoutsManager;
}

WindowSystem::AbstractWindowInterface *CoronaEngine::wm() const
{
    return m_wm;
}

PanelShadows *CoronaEngine::dialogShadows() const
{
    return m_dialogShadows;
}

KWayland::Client::PlasmaShell *CoronaEngine::waylandCoronaInterface() const
{
    return m_waylandCorona;
}

int CoronaEngine::numScreens() const
{
    return m_screenInfo->numScreens();
}

QRect CoronaEngine::screenGeometry(int id) const
{
    return m_screenInfo->screenGeometry(id);
}

QRegion CoronaEngine::availableScreenRegion(int id) const
{
    //! ignore modes are added in order for notifications to be placed
    //! in better positioning and not overlap with sidebars or usually hidden views
    QList<Types::Visibility> ignoremodes({Latte::Types::AutoHide,
                                          Latte::Types::SidebarOnDemand,
                                          Latte::Types::SidebarAutoHide});

    return availableScreenRegionWithCriteria(id, QString(), ignoremodes);
}

QRegion CoronaEngine::availableScreenRegionWithCriteria(int id,
                                                        QString activityid,
                                                        QList<Types::Visibility> ignoreModes,
                                                        QList<Plasma::Types::Location> ignoreEdges,
                                                        bool ignoreExternalPanels,
                                                        bool desktopUse) const
{
    if (!m_screenInfo->hasScreenForId(id)) {
        return {};
    }

    const QRect screenGeom = m_screenInfo->geometryForId(id);
    const QRect startRect = ignoreExternalPanels ? screenGeom : m_screenInfo->availableGeometryForId(id);

    return ScreenGeometryCalculator::availableRegion(startRect,
                                                     screenGeom,
                                                     viewFootprintsOnScreen(id, activityid),
                                                     ignoreModes,
                                                     ignoreEdges,
                                                     desktopUse);
}

QRect CoronaEngine::availableScreenRect(int id) const
{
    //! ignore modes are added in order for notifications to be placed
    //! in better positioning and not overlap with sidebars or usually hidden views
    QList<Types::Visibility> ignoremodes({Latte::Types::AutoHide,
                                          Latte::Types::SidebarOnDemand,
                                          Latte::Types::SidebarAutoHide});

    return availableScreenRectWithCriteria(id, QString(), ignoremodes);
}

QRect CoronaEngine::availableScreenRectWithCriteria(int id,
                                                    QString activityid,
                                                    QList<Types::Visibility> ignoreModes,
                                                    QList<Plasma::Types::Location> ignoreEdges,
                                                    bool ignoreExternalPanels,
                                                    bool desktopUse) const
{
    if (!m_screenInfo->hasScreenForId(id)) {
        return {};
    }

    const QRect screenGeom = m_screenInfo->geometryForId(id);
    const QRect startRect = ignoreExternalPanels ? screenGeom : m_screenInfo->availableGeometryForId(id);

    return ScreenGeometryCalculator::availableRect(startRect,
                                                   screenGeom,
                                                   viewFootprintsOnScreen(id, activityid),
                                                   ignoreModes,
                                                   ignoreEdges,
                                                   desktopUse);
}

QList<ViewFootprint> CoronaEngine::viewFootprintsOnScreen(int id, const QString &activityid) const
{
    //! resolving the live QScreen is the footprint-source's integration boundary: under a
    //! headless test the pool has no mappings, so this returns no footprints and the math
    //! runs against the bare screen rect.
    const QScreen *screen = m_screenPool->screenForId(id);

    if (!screen) {
        return {};
    }

    const bool inCurrentActivity = activityid.isEmpty();

    const QList<Latte::View *> views = m_layoutsManager->synchronizer()->viewsBasedOnActivityId(
        inCurrentActivity ? m_activitiesConsumer->currentActivity() : activityid);

    QList<ViewFootprint> footprints;

    for (const auto *view : views) {
        if (view && view->containment() && view->screen() == screen) {
            footprints << footprintForView(view);
        }
    }

    return footprints;
}

}
