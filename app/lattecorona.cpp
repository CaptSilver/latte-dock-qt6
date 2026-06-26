/*
    SPDX-FileCopyrightText: 2016 Smith AR <audoban@openmaibox.org>
    SPDX-FileCopyrightText: 2016 Michail Vourlakos <mvourlakos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lattecorona.h"

// local
#include <coretypes.h>
#include "alternativeshelper.h"
#include "apptypes.h"
#include "coronaengine.h"
#include "coronahelpers.h"
#include "lattedockadaptor.h"
#include "screengeometrycalculator.h"
#include "screenpool.h"
#include "startuplayoutplanner.h"
#include "data/generictable.h"
#include "data/layouticondata.h"
#include "declarativeimports/interfaces.h"
#include "declarativeimports/contextmenulayerquickitem.h"
#include "indicator/factory.h"
#include "layout/abstractlayout.h"
#include "layout/centrallayout.h"
#include "layout/genericlayout.h"
#include "layouts/importer.h"
#include "layouts/manager.h"
#include "layouts/synchronizer.h"
#include "shortcuts/globalshortcuts.h"
#include "package/lattepackage.h"
#include "plasma/extended/backgroundcache.h"
#include "plasma/extended/backgroundtracker.h"
#include "plasma/extended/screengeometries.h"
#include "plasma/extended/screenpool.h"
#include "plasma/extended/theme.h"
#include "settings/universalsettings.h"
#include "templates/templatesmanager.h"
#include "view/originalview.h"
#include "view/view.h"
#include "view/settings/viewsettingsfactory.h"
#include "view/windowstracker/windowstracker.h"
#include "view/windowstracker/allscreenstracker.h"
#include "view/windowstracker/currentscreentracker.h"
#include "wm/abstractwindowinterface.h"
#include "wm/schemecolors.h"
#include "wm/waylandinterface.h"
#include "wm/tracker/lastactivewindow.h"
#include "wm/tracker/schemes.h"
#include "wm/tracker/windowstracker.h"

// Qt
#include <QAction>
#include <QApplication>
#include <QScreen>
#include <QDBusConnection>
#include <QDebug>
#include <QFile>
#include <QFontDatabase>
#include <QQmlContext>
#include <QProcess>

// Plasma
#include <Plasma/Plasma>
#include <Plasma/Applet>
#include <Plasma/Corona>
#include <Plasma/Containment>
#include <PlasmaQuick/ConfigView>

// KDE
#include <KActionCollection>
#include <KPluginMetaData>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <KAboutData>
#include <PlasmaActivities/Consumer>
#include <PlasmaQuick/SharedQmlEngine>
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

Corona::Corona(bool defaultLayoutOnStartup, QString layoutNameOnStartUp, QString addViewTemplateName, int userSetMemoryUsage, QObject *parent)
    : Plasma::Corona(parent),
      m_defaultLayoutOnStartup(defaultLayoutOnStartup),
      m_userSetMemoryUsage(userSetMemoryUsage),
      m_layoutNameOnStartUp(layoutNameOnStartUp),
      m_startupAddViewTemplateName(addViewTemplateName)
{
    //! The engine owns the collaborators; it hands this shell its m_engine pointer first thing
    //! so accessor forwarding already works while the collaborators construct.
    m_engine = new CoronaEngine(this);

    connect(qApp, &QApplication::aboutToQuit, this, &Corona::onAboutToQuit);

    KPackage::Package package(new Latte::Package(this));

    if (!package.isValid()) {
        qWarning() << staticMetaObject.className()
                   << "the package" << package.metadata().rawData() << "is invalid!";
        return;
    } else {
        qDebug() << staticMetaObject.className()
                 << "the package" << package.metadata().rawData() << "is valid!";
    }

    setKPackage(package);

    //! collaborators load after the package has been set
    m_engine->init();

    qmlRegisterTypes();

    if (m_engine->activitiesConsumer() && (m_engine->activitiesConsumer()->serviceStatus() == KActivities::Consumer::Running)) {
        load();
    }

    connect(m_engine->activitiesConsumer(), &KActivities::Consumer::serviceStatusChanged, this, &Corona::load);

    m_viewsScreenSyncTimer.setSingleShot(true);
    m_viewsScreenSyncTimer.setInterval(m_engine->universalSettings()->screenTrackerInterval());
    connect(&m_viewsScreenSyncTimer, &QTimer::timeout, this, &Corona::syncLatteViewsToScreens);
    connect(m_engine->universalSettings(), &UniversalSettings::screenTrackerIntervalChanged, this, [this]() {
        m_viewsScreenSyncTimer.setInterval(m_engine->universalSettings()->screenTrackerInterval());
    });

    //! Dbus adaptor initialization
    new LatteDockAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/Latte"), this);
}

Corona::~Corona()
{
    m_engine->deleteLater();

    qDebug() << "Latte Corona - deleted...";

    if (!m_importFullConfigurationFile.isEmpty()) {
        //!NOTE: Restart latte to import the new configuration
        qDebug() << "Executing Import Full Configuration command : latte-dock --import-full" << m_importFullConfigurationFile;

        QProcess::startDetached(QStringLiteral("latte-dock"), {QStringLiteral("--import-full"), m_importFullConfigurationFile});
    }
}

void Corona::onAboutToQuit()
{
    m_inQuit = true;

    //! BEGIN: Give the time to slide-out views when closing
    layoutsManager()->synchronizer()->hideAllViews();
    viewSettingsFactory()->deleteLater();

    m_viewsScreenSyncTimer.stop();

    if (layoutsManager()->memoryUsage() == MemoryUsage::SingleLayout) {
        cleanConfig();
    }

    if (layoutsManager()->memoryUsage() == Latte::MemoryUsage::MultipleLayouts) {
        layoutsManager()->importer()->setMultipleLayoutsStatus(Latte::MultipleLayouts::Paused);
    }

    qDebug() << "Latte Corona - unload: containments ...";
    layoutsManager()->unload();
}

void Corona::load()
{
    if (activitiesConsumer() && (activitiesConsumer()->serviceStatus() == KActivities::Consumer::Running) && m_activitiesStarting) {
        m_activitiesStarting = false;

        disconnect(activitiesConsumer(), &KActivities::Consumer::serviceStatusChanged, this, &Corona::load);

        templatesManager()->init();
        layoutsManager()->init();

        //! Plasma 6's base availableScreenRe{ct,gion}Changed take an int screen id; derive it from
        //! the origin view's positioner. Member-function slots (not lambdas) so Qt::UniqueConnection
        //! stays valid — UniqueConnection is illegal with functor/lambda connections and aborts.
        connect(this, &Corona::availableScreenRectChangedFrom, this, &Corona::onAvailableScreenRectChangedFrom, Qt::UniqueConnection);
        connect(this, &Corona::availableScreenRegionChangedFrom, this, &Corona::onAvailableScreenRegionChangedFrom, Qt::UniqueConnection);
        connect(screenPool(), &ScreenPool::primaryScreenChanged, this, &Corona::onScreenCountChanged, Qt::UniqueConnection);

        StartupInputs inputs;
        inputs.userSetMemoryUsage = m_userSetMemoryUsage;
        inputs.currentMemoryUsage = universalSettings()->layoutsMemoryUsage();
        inputs.singleModeLayoutName = universalSettings()->singleModeLayoutName();
        inputs.defaultLayoutOnStartup = m_defaultLayoutOnStartup;
        inputs.layoutNameOnStartUp = m_layoutNameOnStartUp;
        inputs.defaultLayoutTemplateName = i18n(Templates::DEFAULTLAYOUTTEMPLATENAME);

        //! the planner only needs to know whether the chosen single layout and the Default
        //! template already exist; layoutExists() is a side-effect-free query.
        if (layoutsManager()->synchronizer()->layoutExists(inputs.singleModeLayoutName)) {
            inputs.existingLayoutNames << inputs.singleModeLayoutName;
        }
        if (layoutsManager()->synchronizer()->layoutExists(inputs.defaultLayoutTemplateName)) {
            inputs.existingLayoutNames << inputs.defaultLayoutTemplateName;
        }

        const StartupPlan plan = StartupLayoutPlanner::plan(inputs);

        if (plan.memoryUsageToSet.has_value()) {
            universalSettings()->setLayoutsMemoryUsage(plan.memoryUsageToSet.value());
        }

        QString loadLayoutName = plan.loadLayoutName;

        if (plan.createFreshDefaultLayout) {
            //! force loading a NEW default layout even though a default layout may already exist
            const QString newDefaultLayoutPath = templatesManager()->newLayout(QString(), inputs.defaultLayoutTemplateName);
            loadLayoutName = Layout::AbstractLayout::layoutName(newDefaultLayoutPath);
        } else if (plan.ensureDefaultLayoutExists) {
            //! the chosen single layout is gone; create the Default template if it is missing too
            const QString path = templatesManager()->newLayout(QString(), inputs.defaultLayoutTemplateName);
            layoutsManager()->setOnAllActivities(Layout::AbstractLayout::layoutName(path));
        }

        layoutsManager()->loadLayoutOnStartup(loadLayoutName);

        //! load screens signals such screenGeometryChanged in order to support
        //! plasmoid.screenGeometry properly
        for (QScreen *screen : qGuiApp->screens()) {
            onScreenAdded(screen);
        }

        connect(layoutsManager()->synchronizer(), &Layouts::Synchronizer::initializationFinished, [this]() {
            if (!m_startupAddViewTemplateName.isEmpty()) {
                //! user requested through cmd startup to add view from specific view template and we can add it after the startup
                //! sequence has loaded all required layouts properly
                addView(0, m_startupAddViewTemplateName);
                m_startupAddViewTemplateName = QString();
            }
        });

        m_inStartup = false;

        connect(qGuiApp, &QGuiApplication::screenAdded, this, &Corona::onScreenAdded, Qt::UniqueConnection);
        connect(qGuiApp, &QGuiApplication::screenRemoved, this, &Corona::onScreenRemoved, Qt::UniqueConnection);
    }
}

void Corona::unload()
{
    qDebug() << "unload: removing containments...";

    while (!containments().isEmpty()) {
        //deleting a containment will remove it from the list due to QObject::destroyed connect in Corona
        //this form doesn't crash, while qDeleteAll(containments()) does
        delete containments().first();
    }
}

KWayland::Client::PlasmaShell *Corona::waylandCoronaInterface() const
{
    return m_engine->waylandCoronaInterface();
}

void Corona::cleanConfig()
{
    QSet<uint> liveContainmentIds;
    QHash<uint, QSet<uint>> liveAppletIds;

    for (const auto containment : containments()) {
        liveContainmentIds.insert(containment->id());

        QSet<uint> appletIds;
        for (const auto applet : containment->applets()) {
            appletIds.insert(applet->id());
        }
        liveAppletIds.insert(containment->id(), appletIds);
    }

    auto containmentsEntries = config()->group(QStringLiteral("Containments"));

    if (CoronaHelpers::pruneObsoleteContainmentConfig(containmentsEntries, liveContainmentIds, liveAppletIds)) {
        config()->sync();
        qDebug() << "configuration file cleaned...";
    }
}

bool Corona::inQuit() const
{
    return m_inQuit;
}

KActivities::Consumer *Corona::activitiesConsumer() const
{
    return m_engine->activitiesConsumer();
}

PanelShadows *Corona::dialogShadows() const
{
    return m_engine->dialogShadows();
}

GlobalShortcuts *Corona::globalShortcuts() const
{
    return m_engine->globalShortcuts();
}

ScreenPool *Corona::screenPool() const
{
    return m_engine->screenPool();
}

UniversalSettings *Corona::universalSettings() const
{
    return m_engine->universalSettings();
}

ViewSettingsFactory *Corona::viewSettingsFactory() const
{
    return m_engine->viewSettingsFactory();
}

WindowSystem::AbstractWindowInterface *Corona::wm() const
{
    return m_engine->wm();
}

Indicator::Factory *Corona::indicatorFactory() const
{
    return m_engine->indicatorFactory();
}

Layouts::Manager *Corona::layoutsManager() const
{
    return m_engine->layoutsManager();
}

Templates::Manager *Corona::templatesManager() const
{
    return m_engine->templatesManager();
}

PlasmaExtended::ScreenPool *Corona::plasmaScreenPool() const
{
    return m_engine->plasmaScreenPool();
}

PlasmaExtended::Theme *Corona::themeExtended() const
{
    return m_engine->themeExtended();
}

int Corona::numScreens() const
{
    return qGuiApp->screens().count();
}

QRect Corona::screenGeometry(int id) const
{
    const auto screens = qGuiApp->screens();
    const QScreen *screen{screenPool()->primaryScreen()};

    QString screenName;

    if (screenPool()->hasScreenId(id)) {
        screenName = screenPool()->connector(id);
    }

    for(const auto scr : screens) {
        if (scr->name() == screenName) {
            screen = scr;
            break;
        }
    }

    if (!screen) {
        return {};
    }

    return screen->geometry();
}

CentralLayout *Corona::centralLayout(QString name) const
{
    CentralLayout *result{nullptr};

    if (!name.isEmpty()) {
        result = layoutsManager()->synchronizer()->centralLayout(name);
    }

    return result;
}

Layout::GenericLayout *Corona::layout(QString name) const
{
    Layout::GenericLayout *result{nullptr};

    if (!name.isEmpty()) {
        result = layoutsManager()->synchronizer()->layout(name);
    }

    return result;
}

QRegion Corona::availableScreenRegion(int id) const
{   
    //! ignore modes are added in order for notifications to be placed
    //! in better positioning and not overlap with sidebars or usually hidden views
    QList<Types::Visibility> ignoremodes({Latte::Types::AutoHide,
                                          Latte::Types::SidebarOnDemand,
                                          Latte::Types::SidebarAutoHide});


    return availableScreenRegionWithCriteria(id,
                                             QString(),
                                             ignoremodes);
}

QRegion Corona::availableScreenRegionWithCriteria(int id,
                                                  QString activityid,
                                                  QList<Types::Visibility> ignoreModes,
                                                  QList<Plasma::Types::Location> ignoreEdges,
                                                  bool ignoreExternalPanels,
                                                  bool desktopUse) const
{
    const QScreen *screen = screenPool()->screenForId(id);

    if (!screen) {
        return {};
    }

    const QRect startRect = ignoreExternalPanels ? screen->geometry() : screen->availableGeometry();

    return ScreenGeometryCalculator::availableRegion(startRect,
                                                     screen->geometry(),
                                                     viewFootprintsOnScreen(screen, activityid),
                                                     ignoreModes,
                                                     ignoreEdges,
                                                     desktopUse);
}

QRect Corona::availableScreenRect(int id) const
{
    //! ignore modes are added in order for notifications to be placed
    //! in better positioning and not overlap with sidebars or usually hidden views
    QList<Types::Visibility> ignoremodes({Latte::Types::AutoHide,
                                          Latte::Types::SidebarOnDemand,
                                          Latte::Types::SidebarAutoHide});

    return availableScreenRectWithCriteria(id,
                                           QString(),
                                           ignoremodes);
}

QRect Corona::availableScreenRectWithCriteria(int id,
                                              QString activityid,
                                              QList<Types::Visibility> ignoreModes,
                                              QList<Plasma::Types::Location> ignoreEdges,
                                              bool ignoreExternalPanels,
                                              bool desktopUse) const
{
    const QScreen *screen = screenPool()->screenForId(id);

    if (!screen) {
        return {};
    }

    const QRect startRect = ignoreExternalPanels ? screen->geometry() : screen->availableGeometry();

    return ScreenGeometryCalculator::availableRect(startRect,
                                                   screen->geometry(),
                                                   viewFootprintsOnScreen(screen, activityid),
                                                   ignoreModes,
                                                   ignoreEdges,
                                                   desktopUse);
}

QList<ViewFootprint> Corona::viewFootprintsOnScreen(const QScreen *screen, const QString &activityid) const
{
    const bool inCurrentActivity = activityid.isEmpty();

    const QList<Latte::View *> views = layoutsManager()->synchronizer()->viewsBasedOnActivityId(
        inCurrentActivity ? activitiesConsumer()->currentActivity() : activityid);

    QList<ViewFootprint> footprints;

    for (const auto *view : views) {
        if (view && view->containment() && view->screen() == screen) {
            footprints << footprintForView(view);
        }
    }

    return footprints;
}

void Corona::onScreenAdded(QScreen *screen)
{
    Q_ASSERT(screen);

    int id = screenPool()->id(screen->name());

    if (id == -1) {
        screenPool()->insertScreenMapping(screen->name());
    }

    connect(screen, &QScreen::geometryChanged, this, &Corona::onScreenGeometryChanged);

    Q_EMIT availableScreenRectChanged(screenPool()->id(screen->name()));
    Q_EMIT screenAdded(screenPool()->id(screen->name()));

    onScreenCountChanged();
}

void Corona::onScreenRemoved(QScreen *screen)
{
    disconnect(screen, &QScreen::geometryChanged, this, &Corona::onScreenGeometryChanged);
    onScreenCountChanged();
}

void Corona::onScreenCountChanged()
{
    m_viewsScreenSyncTimer.start();
}

void Corona::onScreenGeometryChanged(const QRect &geometry)
{
    Q_UNUSED(geometry);

    QScreen *screen = qobject_cast<QScreen *>(sender());

    if (!screen) {
        return;
    }

    const int id = screenPool()->id(screen->name());

    if (id >= 0) {
        Q_EMIT screenGeometryChanged(id);
        Q_EMIT availableScreenRegionChanged(id);
        Q_EMIT availableScreenRectChanged(id);
    }
}

void Corona::onAvailableScreenRectChangedFrom(Latte::View *origin)
{
    if (origin && origin->positioner()) {
        Q_EMIT availableScreenRectChanged(origin->positioner()->currentScreenId());
    }
}

void Corona::onAvailableScreenRegionChangedFrom(Latte::View *origin)
{
    if (origin && origin->positioner()) {
        Q_EMIT availableScreenRegionChanged(origin->positioner()->currentScreenId());
    }
}

//! the central functions that updates loading/unloading latteviews
//! concerning screen changed (for multi-screen setups mainly)
void Corona::syncLatteViewsToScreens()
{
    layoutsManager()->synchronizer()->syncLatteViewsToScreens();
}

int Corona::primaryScreenId() const
{
    return screenPool()->primaryScreenId();
}

void Corona::quitApplication()
{
    m_inQuit = true;

    //! this code must be called asynchronously because it is called
    //! also from qml (Settings window).
    QTimer::singleShot(300, [this]() {
        layoutsManager()->hideLatteSettingsDialog();
        layoutsManager()->synchronizer()->hideAllViews();
    });

    //! give the time for the views to hide themselves
    QTimer::singleShot(800, [this]() {
        qGuiApp->quit();
    });
}

void Corona::aboutApplication()
{
    if (aboutDialog) {
        aboutDialog->hide();
        aboutDialog->deleteLater();
    }

    aboutDialog = new KAboutApplicationDialog(KAboutData::applicationData());
    connect(aboutDialog.data(), &QDialog::finished, aboutDialog.data(), &QObject::deleteLater);
    wm()->skipTaskBar(*aboutDialog);
    wm()->setKeepAbove(aboutDialog->winId(), true);

    aboutDialog->show();
}

void Corona::loadDefaultLayout()
{
  //disabled
}

int Corona::screenForContainment(const Plasma::Containment *containment) const
{
    //FIXME: indexOf is not a proper way to support multi-screen
    // as for environment to environment the indexes change
    // also there is the following issue triggered
    // from latteView adaptToScreen()
    //
    // in a multi-screen environment that
    // primary screen is not set to 0 it was
    // created an endless showing loop at
    // startup (catch-up race) between
    // screen:0 and primaryScreen

    //case in which this containment is child of an applet, hello systray :)
    if (Plasma::Applet *parentApplet = qobject_cast<Plasma::Applet *>(containment->parent())) {
        if (Plasma::Containment *cont = parentApplet->containment()) {
            return screenForContainment(cont);
        } else {
            return -1;
        }
    }

    Plasma::Containment *c = const_cast<Plasma::Containment *>(containment);
    int scrId = layoutsManager()->synchronizer()->screenForContainment(c);

    if (scrId >= 0) {
        return scrId;
    }

    return containment->lastScreen();
}

void Corona::showAlternativesForApplet(Plasma::Applet *applet)
{
    const QString alternativesQML = kPackage().filePath("appletalternativesui");

    if (alternativesQML.isEmpty()) {
        return;
    }

    Latte::View *latteView =  layoutsManager()->synchronizer()->viewForContainment(applet->containment());

    PlasmaQuick::SharedQmlEngine *qmlObj{nullptr};

    if (latteView) {
        latteView->setAlternativesIsShown(true);
        qmlObj = new PlasmaQuick::SharedQmlEngine(latteView);
    } else {
        qmlObj = new PlasmaQuick::SharedQmlEngine(this);
    }

    qmlObj->setInitializationDelayed(true);
    qmlObj->setSource(QUrl::fromLocalFile(alternativesQML));

    AlternativesHelper *helper = new AlternativesHelper(applet, qmlObj);
    qmlObj->rootContext()->setContextProperty(QStringLiteral("alternativesHelper"), helper);

    m_alternativesObjects << qmlObj;
    qmlObj->completeInitialization();

    //! Alternative dialog signals
    connect(helper, &QObject::destroyed, this, [latteView]() {
        latteView->setAlternativesIsShown(false);
    });

    connect(qmlObj->rootObject(), SIGNAL(visibleChanged(bool)),
            this, SLOT(alternativesVisibilityChanged(bool)));

    connect(applet, &Plasma::Applet::destroyedChanged, this, [this, qmlObj](bool destroyed) {
        if (!destroyed) {
            return;
        }

        QMutableListIterator<PlasmaQuick::SharedQmlEngine *> it(m_alternativesObjects);

        while (it.hasNext()) {
            PlasmaQuick::SharedQmlEngine *obj = it.next();

            if (obj == qmlObj) {
                it.remove();
                obj->deleteLater();
            }
        }
    });
}

void Corona::alternativesVisibilityChanged(bool visible)
{
    if (visible) {
        return;
    }

    QObject *root = sender();

    QMutableListIterator<PlasmaQuick::SharedQmlEngine *> it(m_alternativesObjects);

    while (it.hasNext()) {
        PlasmaQuick::SharedQmlEngine *obj = it.next();

        if (obj->rootObject() == root) {
            it.remove();
            obj->deleteLater();
        }
    }
}

QStringList Corona::containmentsIds()
{
    QStringList ids;

    for(const auto containment : containments()) {
        ids << QString::number(containment->id());
    }

    return ids;
}

QStringList Corona::appletsIds()
{
    QStringList ids;

    for(const auto containment : containments()) {
        auto applets = containment->config().group(QStringLiteral("Applets"));
        ids << applets.groupList();
    }

    return ids;
}

//! Activate launcher menu through dbus interface
void Corona::activateLauncherMenu()
{
    globalShortcuts()->activateLauncherMenu();
}

void Corona::windowColorScheme(QString windowIdAndScheme)
{
    const auto request = CoronaHelpers::parseWindowIdAndScheme(windowIdAndScheme);
    const QString schemeStr = request.scheme;

    if (KWindowSystem::isPlatformWayland()) {
        QTimer::singleShot(200, [this, schemeStr]() {
            //! [Wayland Case] - give the time to be informed correctly for the active window id
            //! otherwise the active window id may not be the same with the one triggered
            //! the color scheme dbus signal
            const QString activeWindowIdStr = wm()->activeWindow().toString();
            wm()->schemesTracker()->setColorSchemeForWindow(activeWindowIdStr.toUInt(), schemeStr);
        });
    } else {
        wm()->schemesTracker()->setColorSchemeForWindow(request.windowId.toUInt(), schemeStr);
    }
}

//! update badge for specific view item
void Corona::updateDockItemBadge(QString identifier, QString value)
{
    globalShortcuts()->updateViewItemBadge(identifier, value);
}

void Corona::setAutostart(const bool &enabled)
{
    universalSettings()->setAutostart(enabled);
}

void Corona::switchToLayout(QString layout)
{
    if (CoronaHelpers::isLayoutFilePath(layout)) {
        importLayoutFile(layout);
    } else {
        layoutsManager()->switchToLayout(layout);
    }
}

void Corona::importLayoutFile(const QString &filepath, const QString &suggestedLayoutName)
{
    if (!CoronaHelpers::isLayoutFilePath(filepath)) {
        qDebug() << i18n("The layout cannot be imported from file :: ") << filepath;
        return;
    }

    //! Import and load runtime a layout through dbus interface
    //! It can be used from external programs that want to update runtime
    //! the Latte shown layout
    QString layoutPath = CoronaHelpers::cleanLayoutFilePath(filepath);

    //! check out layoutpath existence
    if (QFileInfo(layoutPath).exists()) {
        qDebug() << " Layout is going to be imported and loaded from file :: " << layoutPath << " with suggested name :: " << suggestedLayoutName;

        QString importedLayout = layoutsManager()->importer()->importLayout(layoutPath, suggestedLayoutName);

        if (importedLayout.isEmpty()) {
            qDebug() << i18n("The layout cannot be imported from file :: ") << layoutPath;
        } else {
            layoutsManager()->switchToLayout(importedLayout, MemoryUsage::SingleLayout);
        }
    } else {
        qDebug() << " Layout from missing file can not be imported and loaded :: " << layoutPath;
    }
}

void Corona::showSettingsWindow(int page)
{
    if (m_inStartup) {
        qWarning() << "Latte: showSettingsWindow ignored, startup not finished (KActivities service not Running?)";
        return;
    }

    const int validPage = CoronaHelpers::validPageOrFirst(page, Settings::Dialog::LayoutPage, Settings::Dialog::PreferencesPage);

    layoutsManager()->showLatteSettingsDialog(static_cast<Settings::Dialog::ConfigurationPage>(validPage));
}

QStringList Corona::contextMenuData(const uint &containmentId)
{
    auto view = layoutsManager()->synchronizer()->viewForContainment(containmentId);

    CoronaHelpers::ContextMenuInputs inputs;
    inputs.memoryUsage = (int)layoutsManager()->memoryUsage();
    inputs.centralLayoutsNames = layoutsManager()->centralLayoutsNames();
    inputs.currentLayoutsNames = layoutsManager()->synchronizer()->currentLayoutsNames();
    inputs.alwaysShownActions = universalSettings()->contextMenuActionsAlwaysShown();

    for (const auto &layoutName : layoutsManager()->synchronizer()->menuLayouts()) {
        if (layoutsManager()->synchronizer()->centralLayout(layoutName) || layoutsManager()->memoryUsage() == Latte::MemoryUsage::SingleLayout) {
            Data::LayoutIcon layouticon = layoutsManager()->iconForLayout(layoutName);
            CoronaHelpers::ContextMenuLayoutEntry entry;
            entry.name = layoutName;
            entry.isBackgroundFile = layouticon.isBackgroundFile;
            entry.iconName = layouticon.name;
            inputs.menuLayouts << entry;
        }
    }

    inputs.selectedViewLayoutName = view ? view->layout()->name() : QString();
    inputs.viewType = (int)(view ? view->type() : Types::DockView);

    if (view && view->isOriginal()) {
        auto originalview = qobject_cast<Latte::OriginalView *>(view);
        inputs.viewIsOriginal = true;
        inputs.viewClonesCount = originalview->clonesCount();
    } else if (view && view->isCloned()) {
        inputs.viewIsCloned = true;
    }

    return CoronaHelpers::buildContextMenuData(inputs);
}

QStringList Corona::viewTemplatesData()
{
    QStringList data;

    Latte::Data::GenericTable<Data::Generic> viewtemplates = templatesManager()->viewTemplates();

    for(int i=0; i<viewtemplates.rowCount(); ++i) {
        data << viewtemplates[i].name;
        data << viewtemplates[i].id;
    }

    return data;
}

void Corona::addView(const uint &containmentId, const QString &templateId)
{
    if (containmentId <= 0) {
        auto currentlayouts = layoutsManager()->currentLayouts();
        if (currentlayouts.count() > 0) {
            currentlayouts[0]->newView(templateId);
        }
    } else {
        auto view = layoutsManager()->synchronizer()->viewForContainment((int)containmentId);
        if (view) {
            view->newView(templateId);
        }
    }
}

void Corona::duplicateView(const uint &containmentId)
{
    auto view = layoutsManager()->synchronizer()->viewForContainment((int)containmentId);
    if (view) {
        view->duplicateView();
    }
}

void Corona::exportViewTemplate(const uint &containmentId)
{
    auto view = layoutsManager()->synchronizer()->viewForContainment((int)containmentId);
    if (view) {
        view->exportTemplate();
    }
}

void Corona::moveViewToLayout(const uint &containmentId, const QString &layoutName)
{
    auto view = layoutsManager()->synchronizer()->viewForContainment((int)containmentId);
    if (view && !layoutName.isEmpty() && view->layout()->name() != layoutName) {
        Latte::Types::ScreensGroup screensgroup{Latte::Types::SingleScreenGroup};

        if (view->isOriginal()) {
            auto originalview = qobject_cast<Latte::OriginalView *>(view);
            screensgroup = originalview->screensGroup();
        }

        view->positioner()->setNextLocation(layoutName, screensgroup, QString(), Plasma::Types::Floating, Latte::Types::NoneAlignment);
    }
}

void Corona::removeView(const uint &containmentId)
{
    auto view = layoutsManager()->synchronizer()->viewForContainment((int)containmentId);
    if (view) {
        view->removeView();
    }
}

void Corona::addApplet(const uint &containmentId, const QString &pluginId)
{
    auto view = layoutsManager()->synchronizer()->viewForContainment((int)containmentId);
    if (view && view->extendedInterface()) {
        view->extendedInterface()->addApplet(pluginId);
    }
}

void Corona::removeApplet(const uint &containmentId, const uint &appletId)
{
    auto view = layoutsManager()->synchronizer()->viewForContainment((int)containmentId);
    if (view && view->extendedInterface()) {
        view->extendedInterface()->removeApplet((int)appletId);
    }
}

void Corona::triggerAppletAction(const uint &containmentId, const uint &appletId, const QString &actionName)
{
    auto view = layoutsManager()->synchronizer()->viewForContainment((int)containmentId);
    if (!view || !view->containment()) {
        return;
    }

    const auto applets = view->containment()->applets();
    for (auto *applet : applets) {
        if (applet->id() == appletId) {
            if (QAction *act = applet->internalAction(actionName)) {
                act->trigger();
            }
            return;
        }
    }
}

QList<uint> Corona::appletIds(const uint &containmentId)
{
    QList<uint> ids;
    auto view = layoutsManager()->synchronizer()->viewForContainment((int)containmentId);
    if (view && view->containment()) {
        const auto applets = view->containment()->applets();
        for (auto *applet : applets) {
            ids << applet->id();
        }
    }
    return ids;
}

QList<uint> Corona::containmentIds()
{
    QList<uint> ids;
    const auto views = layoutsManager()->synchronizer()->currentViews();
    for (auto *view : views) {
        if (view && view->containment()) {
            ids << view->containment()->id();
        }
    }
    return ids;
}

void Corona::setBackgroundFromBroadcast(QString activity, QString screenName, QString filename)
{
    if (filename.startsWith(QLatin1String("file://"))) {
        filename = filename.remove(0,7);
    }

    PlasmaExtended::BackgroundCache::self()->setBackgroundFromBroadcast(activity, screenName, filename);
}

void Corona::setBroadcastedBackgroundsEnabled(QString activity, QString screenName, bool enabled)
{
    PlasmaExtended::BackgroundCache::self()->setBroadcastedBackgroundsEnabled(activity, screenName, enabled);
}

void Corona::toggleHiddenState(QString layoutName, QString viewName, QString screenName, int screenEdge)
{
    if (layoutName.isEmpty()) {
        for(auto layout : layoutsManager()->currentLayouts()) {
            layout->toggleHiddenState(viewName, screenName, (Plasma::Types::Location)screenEdge);
        }
    } else {
        Layout::GenericLayout *gLayout = layout(layoutName);

        if (gLayout) {
            gLayout->toggleHiddenState(viewName, screenName, (Plasma::Types::Location)screenEdge);
        }
    }
}

void Corona::importFullConfiguration(const QString &file)
{
    m_importFullConfigurationFile = file;
    quitApplication();
}

inline void Corona::qmlRegisterTypes() const
{   
    qmlRegisterUncreatableMetaObject(Latte::Settings::staticMetaObject,
                                     "org.kde.latte.private.app",          // import statement
                                     0, 1,                                 // major and minor version of the import
                                     "Settings",                           // name in QML
                                     QStringLiteral("Error: only enums of latte app settings"));

    qmlRegisterType<Latte::BackgroundTracker>("org.kde.latte.private.app", 0, 1, "BackgroundTracker");
    qmlRegisterType<Latte::Interfaces>("org.kde.latte.private.app", 0, 1, "Interfaces");
    qmlRegisterType<Latte::ContextMenuLayerQuickItem>("org.kde.latte.private.app", 0, 1, "ContextMenuLayer");
    qmlRegisterAnonymousType<QScreen>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::View>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::ViewPart::WindowsTracker>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::ViewPart::TrackerPart::CurrentScreenTracker>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::ViewPart::TrackerPart::AllScreensTracker>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::WindowSystem::SchemeColors>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::WindowSystem::Tracker::LastActiveWindow>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::Types>("latte-dock", 1);

}

}
