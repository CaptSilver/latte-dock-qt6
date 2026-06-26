/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Source-level guards for three one-token correctness fixes that have no feasible
// headless behavioral repro: each lives behind the full View / Corona / settings
// graph and cannot be constructed offscreen. Mirrors bindingrestoremodetest --
// read the real source via REPO_ROOT, extract the function body by brace match,
// and assert the fixed form so the typo / empty-guard cannot silently return:
//   * VisibilityManager::updateSidebarState   '==' typo for '=' (state never set)
//   * Layouts::modeIsChanged                  missing '>' -> pointer arithmetic +
//                                             infinite self-recursion
//   * ContainmentInterface::updateContainmentConfigProperty  empty guard body
//                                             falls through to a null deref

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QtTest>

class SourceGuardTest : public QObject
{
    Q_OBJECT

private:
    static QString readFile(const QString &rel)
    {
        QFile f(QStringLiteral("%1/%2").arg(QStringLiteral(REPO_ROOT), rel));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }
        return QString::fromUtf8(f.readAll());
    }

    // Brace-matched body (including the outer braces) of the first `sig { ... }`.
    static QString functionBody(const QString &src, const QString &sig)
    {
        const int s = src.indexOf(sig);
        if (s == -1) {
            return QString();
        }
        const int brace = src.indexOf(QLatin1Char('{'), s + sig.size());
        if (brace == -1) {
            return QString();
        }
        int depth = 0;
        int i = brace;
        for (; i < src.size(); ++i) {
            if (src.at(i) == QLatin1Char('{')) {
                ++depth;
            } else if (src.at(i) == QLatin1Char('}') && --depth == 0) {
                ++i;
                break;
            }
        }
        return src.mid(brace, i - brace);
    }

    static QString stripped(const QString &body)
    {
        QString s = body;
        s.remove(QRegularExpression(QStringLiteral("\\s+")));
        return s;
    }

private Q_SLOTS:
    void visibilityManager_updateSidebarState_assignsState();
    void layoutsController_modeIsChanged_delegatesToModel();
    void containmentInterface_updateContainmentConfigProperty_guardReturns();
    void primaryScreen_dereferencesAreNullGuarded();
    void layoutsController_selectedLayoutOriginalData_guardsNegativeRow();
    void synchronizer_switchToLayoutInMultipleMode_guardsEmptyActivities();
    void panelBackground_cornerLoopsUseExclusiveBound();
    void genericLayout_recreateView_usesQPointerAndAlwaysDequeues();
    void addView_constructsViewsThroughFactory();
    void synchronizer_pauseLayout_guardsNullLayout();
    void factory_reload_keepsIdNameListsLockstep();
    void panelBackground_updateShadow_emitsNotify();
    void synchronizer_unloadLayouts_unloadsViewsBeforeContainments();
    void corona_showSettingsWindow_warnsWhenInStartup();
    void deadCompositingBranchesAreCollapsed();
    void synchronizer_runningActivities_usesStatesCache();
    void synchronizer_syncMultipleLayouts_invalidatesStatesCacheOnce();
    void waylandInterface_windowFor_usesIndexFastPath();
    void genericLayout_viewTransitions_useTransitionHelpers();
    void hashLookupsAvoidKeysContains();
    void positioner_dropsDeadAvailableRegionMember();
    void synchronizer_freeActivities_delegatesToHelper();
    void synchronizer_freeRunningActivities_delegatesToHelper();
    void synchronizer_validActivities_delegatesToHelper();
    void iconItem_setSource_routesThroughClassifier();
    void iconItem_setLastValidSourceName_usesFilter();
    void iconItem_isValid_delegatesToClassifier();
    void layoutsController_uniqueLayoutName_delegatesToHelper();
    void layoutsController_rowForId_delegatesToHelper();
    void layoutsController_rowForName_delegatesToHelper();
    void viewsController_uniqueViewName_delegatesToHelper();
    void viewsController_rowForId_delegatesToHelper();
    void viewsController_pasteSelectedViews_delegatesToHelper();
    void storage_newUniqueIdsFile_delegatesToRemapper();
    void windowstracker_predicatesDelegate();
    void abstractWindowInterface_classifiersDelegate();
};

void SourceGuardTest::visibilityManager_updateSidebarState_assignsState()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/view/visibilitymanager.cpp")),
                                            QStringLiteral("void VisibilityManager::updateSidebarState()")));
    QVERIFY2(!s.isEmpty(), "updateSidebarState() not found");
    // Must ASSIGN the freshly computed state before emitting, not compare-and-discard.
    QVERIFY2(s.contains(QStringLiteral("m_isSidebar=cursidebarstate;")),
             "updateSidebarState must assign m_isSidebar (single '='), not compare it");
    QVERIFY2(!s.contains(QStringLiteral("m_isSidebar==cursidebarstate;")),
             "updateSidebarState has a discarded '==' comparison statement");
}

void SourceGuardTest::layoutsController_modeIsChanged_delegatesToModel()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/settingsdialog/layoutscontroller.cpp")),
                                            QStringLiteral("bool Layouts::modeIsChanged() const")));
    QVERIFY2(!s.isEmpty(), "Layouts::modeIsChanged() not found");
    QVERIFY2(s.contains(QStringLiteral("m_model->modeIsChanged()")),
             "modeIsChanged must delegate via m_model->modeIsChanged()");
    QVERIFY2(!s.contains(QStringLiteral("m_model-modeIsChanged")),
             "modeIsChanged has the missing-'>' pointer-arithmetic / self-recursion typo");
}

void SourceGuardTest::containmentInterface_updateContainmentConfigProperty_guardReturns()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/view/containmentinterface.cpp")),
                                            QStringLiteral("void ContainmentInterface::updateContainmentConfigProperty")));
    QVERIFY2(!s.isEmpty(), "updateContainmentConfigProperty() not found");
    // The null/missing-key guard must early-return instead of an empty body that
    // falls through to dereferencing a possibly-null m_configuration.
    QVERIFY2(s.contains(QStringLiteral("contains(key)){return;")),
             "updateContainmentConfigProperty guard must early-return on a null/absent config");
}

void SourceGuardTest::primaryScreen_dereferencesAreNullGuarded()
{
    // qGuiApp->primaryScreen() can be null (all monitors off / transient unplug),
    // so every site that dereferences it must guard first. No headless repro: the
    // offscreen QPA always reports a screen.
    const QString screenPool = stripped(functionBody(readFile(QStringLiteral("app/screenpool.cpp")),
                                       QStringLiteral("int ScreenPool::primaryScreenId() const")));
    QVERIFY2(screenPool.contains(QStringLiteral("if(!primary){returnNOSCREENID;}")),
             "primaryScreenId must null-check primaryScreen() before ->name()");

    const QString screenInfo = stripped(functionBody(readFile(QStringLiteral("app/realscreeninfo.cpp")),
                                   QStringLiteral("QRect RealScreenInfo::screenGeometry(int id) const")));
    QVERIFY2(screenInfo.contains(QStringLiteral("if(!screen){return")),
             "RealScreenInfo::screenGeometry must null-check the resolved screen before ->geometry()");

    const QString watcher = stripped(functionBody(readFile(QStringLiteral("app/primaryoutputwatcher.cpp")),
                                    QStringLiteral("void PrimaryOutputWatcher::setupRegistry()")));
    QVERIFY2(watcher.contains(QStringLiteral("if(QScreen*primary=qGuiApp->primaryScreen())")),
             "setupRegistry must guard qGuiApp->primaryScreen() before ->name()");
}

void SourceGuardTest::layoutsController_selectedLayoutOriginalData_guardsNegativeRow()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/settingsdialog/layoutscontroller.cpp")),
                                            QStringLiteral("const Latte::Data::Layout Layouts::selectedLayoutOriginalData() const")));
    QVERIFY2(!s.isEmpty(), "selectedLayoutOriginalData() not found");
    // Must short-circuit a -1 (no selection) row like its three siblings, rather
    // than building m_proxyModel->index(-1, ...) and reading from it.
    QVERIFY2(s.contains(QStringLiteral("if(selectedRow<0)")),
             "selectedLayoutOriginalData must guard selectedRow < 0");
}

void SourceGuardTest::synchronizer_switchToLayoutInMultipleMode_guardsEmptyActivities()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/layouts/synchronizer.cpp")),
                                            QStringLiteral("bool Synchronizer::switchToLayoutInMultipleMode(QString layoutName)")));
    QVERIFY2(!s.isEmpty(), "switchToLayoutInMultipleMode() not found");
    // appliedActivities can be empty; indexing [0] is an OOB read.
    QVERIFY2(!s.contains(QStringLiteral("appliedActivities[0]")),
             "switchToLayoutInMultipleMode indexes a possibly-empty list");
    QVERIFY2(s.contains(QStringLiteral("appliedActivities.isEmpty()")),
             "switchToLayoutInMultipleMode must guard the empty-activities case");
}

void SourceGuardTest::panelBackground_cornerLoopsUseExclusiveBound()
{
    const QString s = stripped(readFile(QStringLiteral("app/plasma/extended/panelbackgroundscan.cpp")));
    QVERIFY2(!s.isEmpty(), "panelbackgroundscan.cpp not found");
    // scanLine(corner.height()) reads one row past the image buffer.
    QVERIFY2(!s.contains(QStringLiteral("r<=corner.height()")),
             "a corner roundness loop still uses the inclusive r<=corner.height() bound");
}

void SourceGuardTest::genericLayout_recreateView_usesQPointerAndAlwaysDequeues()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/layout/genericlayout.cpp")),
                                            QStringLiteral("void GenericLayout::recreateView(Plasma::Containment *containment, bool delayed)")));
    QVERIFY2(!s.isEmpty(), "recreateView() not found");
    // The deferred chain dereferences the containment ~600ms later, so it must
    // hold a QPointer, not a raw pointer that can dangle.
    QVERIFY2(s.contains(QStringLiteral("QPointer")),
             "recreateView must capture the containment via QPointer");
    // The queue entry must always be removed, not only inside the addView branch.
    QVERIFY2(s.contains(QStringLiteral("addView(containment);}m_viewsToRecreate.removeAll")),
             "recreateView must dequeue m_viewsToRecreate unconditionally, not inside the addView guard");
}

void SourceGuardTest::synchronizer_pauseLayout_guardsNullLayout()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/layouts/synchronizer.cpp")),
                                            QStringLiteral("void Synchronizer::pauseLayout(QString layoutName)")));
    QVERIFY2(!s.isEmpty(), "pauseLayout() not found");
    // centralLayout() can return null; the null check must precede the dereference.
    QVERIFY2(s.contains(QStringLiteral("if(!layout||layout->isOnAllActivities())")),
             "pauseLayout must null-check layout before dereferencing it");
}

void SourceGuardTest::factory_reload_keepsIdNameListsLockstep()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/indicator/factory.cpp")),
                                            QStringLiteral("void Factory::reload(const QString &indicatorPath)")));
    QVERIFY2(!s.isEmpty(), "Factory::reload() not found");
    // The id and name lists are index-parallel, so the name must be inserted under
    // the same id-uniqueness guard, never an independent name-contains guard that
    // desyncs them.
    QVERIFY2(!s.contains(QStringLiteral("m_customPluginNames.contains")),
             "reload gates the name insert independently, desyncing the parallel lists");
}

void SourceGuardTest::panelBackground_updateShadow_emitsNotify()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/plasma/extended/panelbackground.cpp")),
                                            QStringLiteral("void PanelBackground::updateShadow(KSvg::Svg *svg)")));
    QVERIFY2(!s.isEmpty(), "updateShadow() not found");
    // The shadowSize/shadowColor Q_PROPERTYs back reactive QML bindings; without
    // their NOTIFY the drop-shadow goes stale on a theme switch.
    QVERIFY2(s.contains(QStringLiteral("Q_EMITshadowSizeChanged()")),
             "updateShadow must emit shadowSizeChanged when the size changes");
    QVERIFY2(s.contains(QStringLiteral("Q_EMITshadowColorChanged()")),
             "updateShadow must emit shadowColorChanged when the color changes");
}

void SourceGuardTest::synchronizer_unloadLayouts_unloadsViewsBeforeContainments()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/layouts/synchronizer.cpp")),
                                            QStringLiteral("void Synchronizer::unloadLayouts(const QStringList &layoutNames, const QStringList &preloadedLayouts)")));
    QVERIFY2(!s.isEmpty(), "unloadLayouts() not found");
    const int views = s.indexOf(QStringLiteral("unloadLatteViews"));
    const int containments = s.indexOf(QStringLiteral("unloadContainments"));
    QVERIFY2(views >= 0 && containments >= 0, "unloadLayouts must unload both views and containments");
    // Views first is the crash-safe order (containment delete re-enters
    // containmentDestroyed, which tears down views); the reverse races.
    QVERIFY2(views < containments,
             "unloadLayouts must unload views before containments");
}

void SourceGuardTest::corona_showSettingsWindow_warnsWhenInStartup()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/lattecorona.cpp")),
                                            QStringLiteral("void Corona::showSettingsWindow(int page)")));
    QVERIFY2(!s.isEmpty(), "showSettingsWindow() not found");
    // A degraded session where KActivities never reaches Running leaves m_inStartup
    // true forever; the guard must log rather than silently swallow the request.
    QVERIFY2(s.contains(QStringLiteral("if(m_inStartup){qWarning")),
             "showSettingsWindow must warn, not silently return, while startup is blocked");
}

void SourceGuardTest::deadCompositingBranchesAreCollapsed()
{
    // KF6 dropped KWindowSystem::compositingActive(); the migration replaced it with
    // the literal true, leaving if(true)/if(!true) dead branches (Wayland always
    // composites). They obscure which path runs and must be collapsed away.
    const char *files[] = {
        "app/view/effects.cpp",
        "app/view/helpers/screenedgeghostwindow.cpp",
        "app/view/visibilitymanager.cpp",
        "app/view/settings/secondaryconfigview.cpp",
        "app/view/settings/primaryconfigview.cpp",
        "app/view/settings/widgetexplorerview.cpp",
    };
    for (const char *f : files) {
        const QString s = stripped(readFile(QString::fromUtf8(f)));
        QVERIFY2(!s.isEmpty(), qPrintable(QStringLiteral("could not read %1").arg(QString::fromUtf8(f))));
        QVERIFY2(!s.contains(QStringLiteral("if(true)")),
                 qPrintable(QStringLiteral("%1 still has an if(true) dead branch").arg(QString::fromUtf8(f))));
        QVERIFY2(!s.contains(QStringLiteral("!true")),
                 qPrintable(QStringLiteral("%1 still has a !true dead branch").arg(QString::fromUtf8(f))));
    }
}

void SourceGuardTest::addView_constructsViewsThroughFactory()
{
    const QString addView = stripped(functionBody(readFile(QStringLiteral("app/layout/genericlayout.cpp")),
                                     QStringLiteral("void GenericLayout::addView(Plasma::Containment *containment)")));
    QVERIFY2(!addView.isEmpty(), "addView() not found");
    QVERIFY2(!addView.contains(QStringLiteral("newLatte::OriginalView(")) && !addView.contains(QStringLiteral("newLatte::ClonedView(")),
             "addView must not construct views inline; it routes through the view factory");
    QVERIFY2(addView.contains(QStringLiteral("viewFactory()->createView(")),
             "addView must create views via viewFactory()->createView()");

    const QString factory = stripped(functionBody(readFile(QStringLiteral("app/layout/realviewfactory.cpp")),
                                     QStringLiteral("Latte::View *RealViewFactory::createView(GenericLayout *layout, const AddViewRequest &request)")));
    QVERIFY2(factory.contains(QStringLiteral("layout->registerLatteView(")), "factory must register the view (store-before-wire)");
    QVERIFY2(factory.contains(QStringLiteral("->setupWaylandLayerShell();")) && factory.contains(QStringLiteral("->show();")),
             "factory must wire the view (layer shell + show)");
}

void SourceGuardTest::synchronizer_runningActivities_usesStatesCache()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/layouts/synchronizer.cpp")),
                                            QStringLiteral("QStringList Synchronizer::runningActivities()")));
    QVERIFY2(!s.isEmpty(), "Synchronizer::runningActivities() not found");
    QVERIFY2(s.contains(QStringLiteral("m_activityStates.runningActivities()")),
             "runningActivities must read through the memoizing cache");
    QVERIFY2(!s.contains(QStringLiteral("ActivitiesInfo::runningActivities()")),
             "runningActivities must not re-query the activity manager directly");
}

void SourceGuardTest::synchronizer_syncMultipleLayouts_invalidatesStatesCacheOnce()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/layouts/synchronizer.cpp")),
                                            QStringLiteral("void Synchronizer::syncMultipleLayoutsToActivities(QStringList preloadedLayouts)")));
    QVERIFY2(!s.isEmpty(), "syncMultipleLayoutsToActivities() not found");
    QVERIFY2(s.contains(QStringLiteral("m_activityStates.invalidate();")),
             "each sync must refresh the activity-states cache exactly once");
}

void SourceGuardTest::waylandInterface_windowFor_usesIndexFastPath()
{
    const QString src = readFile(QStringLiteral("app/wm/waylandinterface.cpp"));

    const QString wf = stripped(functionBody(src,
                                QStringLiteral("KWayland::Client::PlasmaWindow *WaylandInterface::windowFor(WindowId wid)")));
    QVERIFY2(!wf.isEmpty(), "windowFor() not found");
    QVERIFY2(wf.contains(QStringLiteral("m_windowIndex.lookup(wid)")),
             "windowFor must consult the id index before scanning");

    const QString track = stripped(functionBody(src,
                                QStringLiteral("void WaylandInterface::trackWindow(KWayland::Client::PlasmaWindow *w)")));
    QVERIFY2(!track.isEmpty(), "trackWindow() not found");
    QVERIFY2(track.contains(QStringLiteral("m_windowIndex.insert(idFor(w),w);")),
             "trackWindow must index the window");

    const QString untrack = stripped(functionBody(src,
                                QStringLiteral("void WaylandInterface::untrackWindow(KWayland::Client::PlasmaWindow *w)")));
    QVERIFY2(!untrack.isEmpty(), "untrackWindow() not found");
    QVERIFY2(untrack.contains(QStringLiteral("m_windowIndex.remove(idFor(w));")),
             "untrackWindow must drop the window from the index");
}

void SourceGuardTest::genericLayout_viewTransitions_useTransitionHelpers()
{
    const QString src = readFile(QStringLiteral("app/layout/genericlayout.cpp"));

    const QString dc = stripped(functionBody(src, QStringLiteral("void GenericLayout::destroyedChanged(bool destroyed)")));
    QVERIFY2(!dc.isEmpty(), "destroyedChanged() not found");
    QVERIFY2(dc.contains(QStringLiteral("ViewContainerTransition::moveBetween(m_latteViews,m_waitingLatteViews")),
             "destroyedChanged must move active->waiting via the transition helper");
    QVERIFY2(dc.contains(QStringLiteral("ViewContainerTransition::moveBetween(m_waitingLatteViews,m_latteViews")),
             "destroyedChanged must move waiting->active via the transition helper");

    const QString cd = stripped(functionBody(src, QStringLiteral("void GenericLayout::containmentDestroyed(QObject *cont)")));
    QVERIFY2(!cd.isEmpty(), "containmentDestroyed() not found");
    QVERIFY2(cd.contains(QStringLiteral("ViewContainerTransition::takeFromEither(m_latteViews,m_waitingLatteViews,containment)")),
             "containmentDestroyed must take from either map via the transition helper");
}

void SourceGuardTest::positioner_dropsDeadAvailableRegionMember()
{
    const QString h = readFile(QStringLiteral("app/view/positioner.h"));
    const QString cpp = readFile(QStringLiteral("app/view/positioner.cpp"));
    QVERIFY2(!h.isEmpty() && !cpp.isEmpty(), "positioner sources not found");
    QVERIFY2(!h.contains(QStringLiteral("m_lastAvailableScreenRegion")),
             "the dead m_lastAvailableScreenRegion member must be removed from the header");
    QVERIFY2(!cpp.contains(QStringLiteral("m_lastAvailableScreenRegion")),
             "the dead m_lastAvailableScreenRegion write must be removed from positioner.cpp");
}

void SourceGuardTest::hashLookupsAvoidKeysContains()
{
    const QString bg = readFile(QStringLiteral("app/plasma/extended/backgroundcache.cpp"));
    QVERIFY2(!bg.isEmpty(), "backgroundcache.cpp not found");
    QVERIFY2(!bg.contains(QStringLiteral("keys().contains(")),
             "backgroundcache.cpp must use contains(), not the allocating keys().contains()");
    const QString gl = readFile(QStringLiteral("app/layout/genericlayout.cpp"));
    QVERIFY2(!gl.isEmpty(), "genericlayout.cpp not found");
    QVERIFY2(!gl.contains(QStringLiteral("keys().contains(")),
             "genericlayout.cpp must use contains(), not keys().contains()");
}

void SourceGuardTest::synchronizer_freeActivities_delegatesToHelper()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/layouts/synchronizer.cpp")),
                                            QStringLiteral("QStringList Synchronizer::freeActivities()")));
    QVERIFY2(!s.isEmpty(), "freeActivities() not found");
    QVERIFY2(s.contains(QStringLiteral("ActivitySetAlgebra::freeActivities(activities(),m_assignedLayouts.keys())")),
             "freeActivities must delegate to ActivitySetAlgebra::freeActivities");
}

void SourceGuardTest::synchronizer_freeRunningActivities_delegatesToHelper()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/layouts/synchronizer.cpp")),
                                            QStringLiteral("QStringList Synchronizer::freeRunningActivities()")));
    QVERIFY2(!s.isEmpty(), "freeRunningActivities() not found");
    QVERIFY2(s.contains(QStringLiteral("ActivitySetAlgebra::freeRunningActivities(runningActivities(),m_assignedLayouts.keys())")),
             "freeRunningActivities must delegate to ActivitySetAlgebra::freeRunningActivities");
}

void SourceGuardTest::synchronizer_validActivities_delegatesToHelper()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/layouts/synchronizer.cpp")),
                                            QStringLiteral("QStringList Synchronizer::validActivities(const QStringList &layoutActivities)")));
    QVERIFY2(!s.isEmpty(), "validActivities() not found");
    QVERIFY2(s.contains(QStringLiteral("ActivitySetAlgebra::validActivities(layoutActivities,activities())")),
             "validActivities must delegate to ActivitySetAlgebra::validActivities");
}

void SourceGuardTest::iconItem_setSource_routesThroughClassifier()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("declarativeimports/core/iconitem.cpp")),
                                            QStringLiteral("void IconItem::setSource(const QVariant &source)")));
    QVERIFY2(!s.isEmpty(), "setSource() not found");
    QVERIFY2(s.contains(QStringLiteral("IconSourceClassifier::classify(")),
             "setSource must route through IconSourceClassifier::classify()");
    QVERIFY2(s.contains(QStringLiteral("IconSourceClassifier::sourceName(")),
             "setSource must derive the source string via IconSourceClassifier::sourceName()");
}

void SourceGuardTest::iconItem_setLastValidSourceName_usesFilter()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("declarativeimports/core/iconitem.cpp")),
                                            QStringLiteral("void IconItem::setLastValidSourceName(QString name)")));
    QVERIFY2(!s.isEmpty(), "setLastValidSourceName() not found");
    QVERIFY2(s.contains(QStringLiteral("IconSourceClassifier::isFilteredSourceName(")),
             "setLastValidSourceName must delegate the empty/executable guard to IconSourceClassifier::isFilteredSourceName()");
    QVERIFY2(!s.contains(QStringLiteral("application-x-executable")),
             "setLastValidSourceName must not inline the application-x-executable literal");
}

void SourceGuardTest::iconItem_isValid_delegatesToClassifier()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("declarativeimports/core/iconitem.cpp")),
                                            QStringLiteral("bool IconItem::isValid() const")));
    QVERIFY2(!s.isEmpty(), "isValid() not found");
    QVERIFY2(s.contains(QStringLiteral("IconSourceClassifier::isValid(")),
             "isValid must delegate to IconSourceClassifier::isValid()");
}

void SourceGuardTest::layoutsController_uniqueLayoutName_delegatesToHelper()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/settingsdialog/layoutscontroller.cpp")),
                                            QStringLiteral("QString Layouts::uniqueLayoutName(")));
    QVERIFY2(!s.isEmpty(), "uniqueLayoutName() not found");
    QVERIFY2(s.contains(QStringLiteral("Settings::uniqueName(")),
             "uniqueLayoutName must delegate to Settings::uniqueName()");
}

void SourceGuardTest::layoutsController_rowForId_delegatesToHelper()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/settingsdialog/layoutscontroller.cpp")),
                                            QStringLiteral("int Layouts::rowForId(")));
    QVERIFY2(!s.isEmpty(), "Layouts::rowForId() not found");
    QVERIFY2(s.contains(QStringLiteral("Settings::rowForValue(")),
             "Layouts::rowForId must delegate to Settings::rowForValue()");
}

void SourceGuardTest::layoutsController_rowForName_delegatesToHelper()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/settingsdialog/layoutscontroller.cpp")),
                                            QStringLiteral("int Layouts::rowForName(")));
    QVERIFY2(!s.isEmpty(), "Layouts::rowForName() not found");
    QVERIFY2(s.contains(QStringLiteral("Settings::rowForValue(")),
             "Layouts::rowForName must delegate to Settings::rowForValue()");
}

void SourceGuardTest::viewsController_uniqueViewName_delegatesToHelper()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/viewsdialog/viewscontroller.cpp")),
                                            QStringLiteral("QString Views::uniqueViewName(")));
    QVERIFY2(!s.isEmpty(), "Views::uniqueViewName() not found");
    QVERIFY2(s.contains(QStringLiteral("Settings::uniqueName(")),
             "uniqueViewName must delegate to Settings::uniqueName()");
}

void SourceGuardTest::viewsController_rowForId_delegatesToHelper()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/viewsdialog/viewscontroller.cpp")),
                                            QStringLiteral("int Views::rowForId(")));
    QVERIFY2(!s.isEmpty(), "Views::rowForId() not found");
    QVERIFY2(s.contains(QStringLiteral("Settings::rowForValue(")),
             "Views::rowForId must delegate to Settings::rowForValue()");
}

void SourceGuardTest::viewsController_pasteSelectedViews_delegatesToHelper()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/viewsdialog/viewscontroller.cpp")),
                                            QStringLiteral("void Views::pasteSelectedViews()")));
    QVERIFY2(!s.isEmpty(), "pasteSelectedViews() not found");
    QVERIFY2(s.contains(QStringLiteral("Settings::pasteSkipsView(")),
             "pasteSelectedViews must delegate the skip-decision to Settings::pasteSkipsView()");
}

void SourceGuardTest::storage_newUniqueIdsFile_delegatesToRemapper()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/layouts/storage.cpp")),
                                            QStringLiteral("QString Storage::newUniqueIdsFile(")));
    QVERIFY2(!s.isEmpty(), "newUniqueIdsFile() not found");
    // The assignment algorithm was moved to StorageIdRemapper::remap; the adapter
    // must call through it rather than inline the old loops.
    QVERIFY2(s.contains(QStringLiteral("StorageIdRemapper::remap(")),
             "newUniqueIdsFile must delegate id assignment to StorageIdRemapper::remap()");
    // The old inline assignment loops and 2-cycle fix must not remain.
    QVERIFY2(!s.contains(QStringLiteral("availableId(allIds,assignedIds,12)")),
             "newUniqueIdsFile must not still contain the old inline containment availableId call");
    QVERIFY2(!s.contains(QStringLiteral("PROBLEMAPPEARED")),
             "newUniqueIdsFile must not still contain the old inline PROBLEM APPEARED 2-cycle fix");
}

void SourceGuardTest::windowstracker_predicatesDelegate()
{
    const QString src = readFile(QStringLiteral("app/wm/tracker/windowstracker.cpp"));

    const QString intersectsBody = functionBody(src, QStringLiteral("Windows::intersects"));
    QVERIFY2(intersectsBody.contains(QStringLiteral("WindowTrackingPredicates::")),
             "intersects must delegate to WindowTrackingPredicates");

    const QString isActiveBody = functionBody(src, QStringLiteral("Windows::isActive"));
    QVERIFY2(isActiveBody.contains(QStringLiteral("WindowTrackingPredicates::")),
             "isActive must delegate to WindowTrackingPredicates");

    const QString isActiveScreenBody = functionBody(src, QStringLiteral("Windows::isActiveInViewScreen"));
    QVERIFY2(isActiveScreenBody.contains(QStringLiteral("WindowTrackingPredicates::")),
             "isActiveInViewScreen must delegate to WindowTrackingPredicates");
    QVERIFY2(isActiveScreenBody.contains(QStringLiteral("devicePixelRatio")),
             "isActiveInViewScreen must keep X11 DPR scaling");

    const QString isMaxScreenBody = functionBody(src, QStringLiteral("Windows::isMaximizedInViewScreen"));
    QVERIFY2(isMaxScreenBody.contains(QStringLiteral("WindowTrackingPredicates::")),
             "isMaximizedInViewScreen must delegate to WindowTrackingPredicates");
    QVERIFY2(isMaxScreenBody.contains(QStringLiteral("devicePixelRatio")),
             "isMaximizedInViewScreen must keep X11 DPR scaling");
}

void SourceGuardTest::abstractWindowInterface_classifiersDelegate()
{
    const QString file = readFile(QStringLiteral("app/wm/abstractwindowinterface.cpp"));

    const QString isIgnoredBody = functionBody(file, QStringLiteral("AbstractWindowInterface::isIgnored"));
    QVERIFY2(isIgnoredBody.contains(QStringLiteral("WindowTrackingPredicates::")),
             "isIgnored must delegate to WindowTrackingPredicates");

    const QString plasmaBody = functionBody(file, QStringLiteral("AbstractWindowInterface::isRegisteredPlasmaIgnoredWindow"));
    QVERIFY2(plasmaBody.contains(QStringLiteral("WindowTrackingPredicates::")),
             "isRegisteredPlasmaIgnoredWindow must delegate to WindowTrackingPredicates");

    const QString whiteBody = functionBody(file, QStringLiteral("AbstractWindowInterface::isWhitelistedWindow"));
    QVERIFY2(whiteBody.contains(QStringLiteral("WindowTrackingPredicates::")),
             "isWhitelistedWindow must delegate to WindowTrackingPredicates");

    const QString blockedBody = functionBody(file, QStringLiteral("AbstractWindowInterface::hasBlockedTracking"));
    QVERIFY2(blockedBody.contains(QStringLiteral("WindowTrackingPredicates::")),
             "hasBlockedTracking must delegate to WindowTrackingPredicates");

    const QString registerBody = functionBody(file, QStringLiteral("AbstractWindowInterface::registerIgnoredWindow"));
    QVERIFY2(registerBody.contains(QStringLiteral("Q_EMIT windowChanged")),
             "registerIgnoredWindow must still emit windowChanged");
}

QTEST_GUILESS_MAIN(SourceGuardTest)

#include "sourceguardtest.moc"
