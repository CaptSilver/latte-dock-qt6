/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CORONAENGINE_H
#define CORONAENGINE_H

// Qt
#include <QObject>

// KDE
#include <KSharedConfig>

class PanelShadows;

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

private:
    void setupWaylandIntegration();

    Latte::Corona *m_shell{nullptr};

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
