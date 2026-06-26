/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CORONAENGINE_H
#define CORONAENGINE_H

// local
#include "screengeometrycalculator.h"

// Qt
#include <QObject>
#include <QRect>
#include <QRegion>

// KDE
#include <KSharedConfig>

class PanelShadows;
class QScreen;

namespace KActivities {
class Consumer;
}
namespace KWayland {
namespace Client {
class PlasmaShell;
}
}

namespace Latte {
class Corona;
class IScreenInfo;
class ScreenPool;
class UniversalSettings;
class ViewSettingsFactory;
class GlobalShortcuts;
namespace Indicator {
class Factory;
}
namespace Layouts {
class Manager;
}
namespace PlasmaExtended {
class ScreenGeometries;
class ScreenPool;
class Theme;
}
namespace Templates {
class Manager;
}
namespace WindowSystem {
class AbstractWindowInterface;
}

//! The dock kernel: owns Corona's collaborators and runs their loading, so the
//! Plasma::Corona shell stays a thin forwarding object. The ctor only constructs
//! the collaborators (no live-infra side effects), so the engine can be built in a
//! headless test; init() performs the side-effecting startup loads.
class CoronaEngine : public QObject
{
    Q_OBJECT

public:
    //! Injection points for headless tests. A null field means the engine builds the
    //! real collaborator; production passes nothing.
    struct Deps
    {
        WindowSystem::AbstractWindowInterface *wm = nullptr;   //! null => new WaylandInterface
        IScreenInfo *screenInfo = nullptr;                     //! null => RealScreenInfo(screenPool)
        KSharedConfig::Ptr config;                             //! null => KSharedConfig::openConfig()
    };

    explicit CoronaEngine(Latte::Corona *shell);
    CoronaEngine(Latte::Corona *shell, const Deps &deps);
    ~CoronaEngine() override;

    //! Side-effecting startup that needs live infra; not called by headless unit tests.
    void init();

    KActivities::Consumer *activitiesConsumer() const;
    ScreenPool *screenPool() const;
    Indicator::Factory *indicatorFactory() const;
    UniversalSettings *universalSettings() const;
    GlobalShortcuts *globalShortcuts() const;
    PlasmaExtended::ScreenPool *plasmaScreenPool() const;
    PlasmaExtended::Theme *themeExtended() const;
    ViewSettingsFactory *viewSettingsFactory() const;
    Templates::Manager *templatesManager() const;
    Layouts::Manager *layoutsManager() const;
    WindowSystem::AbstractWindowInterface *wm() const;
    PanelShadows *dialogShadows() const;
    KWayland::Client::PlasmaShell *waylandCoronaInterface() const;

    //! Available-screen geometry. The math runs against IScreenInfo rects and view
    //! footprints, so it can be exercised without a live screen / View graph.
    int numScreens() const;
    QRect screenGeometry(int id) const;
    QRegion availableScreenRegion(int id) const;
    QRect availableScreenRect(int id) const;
    QRegion availableScreenRegionWithCriteria(int id,
                                              QString activityid = QString(),
                                              QList<Types::Visibility> ignoreModes = QList<Types::Visibility>(),
                                              QList<Plasma::Types::Location> ignoreEdges = QList<Plasma::Types::Location>(),
                                              bool ignoreExternalPanels = true,
                                              bool desktopUse = false) const;
    QRect availableScreenRectWithCriteria(int id,
                                          QString activityid = QString(),
                                          QList<Types::Visibility> ignoreModes = QList<Types::Visibility>(),
                                          QList<Plasma::Types::Location> ignoreEdges = QList<Plasma::Types::Location>(),
                                          bool ignoreExternalPanels = true,
                                          bool desktopUse = false) const;

private:
    void setupWaylandIntegration();

    //! Snapshot the views living on a screen as plain footprints, so the
    //! available-screen geometry math can run without the live View graph.
    QList<ViewFootprint> viewFootprintsOnScreen(int id, const QString &activityid) const;

    Latte::Corona *m_shell{nullptr};
    IScreenInfo *m_screenInfo{nullptr};
    bool m_ownsScreenInfo{false};

    KActivities::Consumer *m_activitiesConsumer{nullptr};
    ScreenPool *m_screenPool{nullptr};
    Indicator::Factory *m_indicatorFactory{nullptr};
    UniversalSettings *m_universalSettings{nullptr};
    GlobalShortcuts *m_globalShortcuts{nullptr};
    PlasmaExtended::ScreenPool *m_plasmaScreenPool{nullptr};
    PlasmaExtended::Theme *m_themeExtended{nullptr};
    ViewSettingsFactory *m_viewSettingsFactory{nullptr};
    Templates::Manager *m_templatesManager{nullptr};
    Layouts::Manager *m_layoutsManager{nullptr};
    PlasmaExtended::ScreenGeometries *m_plasmaGeometries{nullptr};
    PanelShadows *m_dialogShadows{nullptr};
    WindowSystem::AbstractWindowInterface *m_wm{nullptr};
    KWayland::Client::PlasmaShell *m_waylandCorona{nullptr};
};

}

#endif
