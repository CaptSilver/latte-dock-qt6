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

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
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

    // Absolute paths of every file matching `glob` under the given REPO_ROOT-relative directories.
    static QStringList sourcesUnder(const QStringList &relDirs, const QString &glob)
    {
        QStringList out;
        for (const QString &rel : relDirs) {
            QDirIterator it(QStringLiteral("%1/%2").arg(QStringLiteral(REPO_ROOT), rel),
                            QStringList() << glob,
                            QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                //! this repo configures in-source, so moc output sits beside the sources it was
                //! generated from; scanning it makes a guard answer for code nobody wrote
                if (path.contains(QStringLiteral("_autogen/")) || path.contains(QStringLiteral("/CMakeFiles/"))) {
                    continue;
                }
                out << path;
            }
        }
        return out;
    }

    static QStringList qmlSourcesUnder(const QStringList &relDirs)
    {
        return sourcesUnder(relDirs, QStringLiteral("*.qml"));
    }

    // Every member name the given ability layers declare: properties (aliases included),
    // signals and functions. Commented-out declarations do not count -- the four dead
    // `//margin.thickness:` lines left behind by the margin split are exactly the trap.
    static QSet<QString> declaredMembers(const QStringList &relFiles)
    {
        // An ability object is an Item, so a read of an inherited member is not a typo.
        static const QSet<QString> itemMembers = {
            QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z"),
            QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("visible"),
            QStringLiteral("opacity"), QStringLiteral("enabled"), QStringLiteral("parent"),
            QStringLiteral("children"), QStringLiteral("data"), QStringLiteral("anchors"),
            QStringLiteral("state"), QStringLiteral("states"), QStringLiteral("transitions"),
            QStringLiteral("clip"), QStringLiteral("focus"), QStringLiteral("scale"),
            QStringLiteral("rotation"), QStringLiteral("objectName")};

        static const QRegularExpression decl(
            QStringLiteral("^\\s*(?:readonly\\s+)?property\\s+(?:alias\\s+)?[A-Za-z_][\\w.<>]*\\s+([A-Za-z_]\\w*)\\s*[:{]"
                           "|^\\s*signal\\s+([A-Za-z_]\\w*)"
                           "|^\\s*function\\s+([A-Za-z_]\\w*)"),
            QRegularExpression::MultilineOption);

        QSet<QString> names = itemMembers;
        for (const QString &rel : relFiles) {
            const QString src = readFile(rel);
            if (src.isEmpty()) {
                return QSet<QString>(); // caller turns an unreadable layer into a failure
            }
            QRegularExpressionMatchIterator it = decl.globalMatch(src);
            while (it.hasNext()) {
                const QRegularExpressionMatch m = it.next();
                for (int g = 1; g <= 3; ++g) {
                    if (!m.captured(g).isEmpty()) {
                        names.insert(m.captured(g));
                    }
                }
            }
        }
        return names;
    }

    // "file:line  expr" for every `<ns>.<member>` read whose member no layer declares.
    static QStringList unresolvedReads(const QString &ns, const QSet<QString> &declared, const QStringList &absFiles)
    {
        const QRegularExpression use(QStringLiteral("\\b%1\\.([A-Za-z_]\\w*)").arg(QRegularExpression::escape(ns)));
        const QString prefix = QStringLiteral("%1/").arg(QStringLiteral(REPO_ROOT));

        QStringList bad;
        for (const QString &abs : absFiles) {
            QFile f(abs);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
            for (int i = 0; i < lines.size(); ++i) {
                if (lines.at(i).trimmed().startsWith(QStringLiteral("//"))) {
                    continue;
                }
                QRegularExpressionMatchIterator it = use.globalMatch(lines.at(i));
                while (it.hasNext()) {
                    const QString member = it.next().captured(1);
                    if (!declared.contains(member)) {
                        QString rel = abs;
                        rel.remove(prefix);
                        bad << QStringLiteral("%1:%2  %3.%4").arg(rel).arg(i + 1).arg(ns, member);
                    }
                }
            }
        }
        return bad;
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
    void positioner_geometryMethodsDelegateToPureUnit();
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
    void windowsTracker_updateExtraViewHints_delegatesToBucketing();
    void windowsTracker_perEventIterationAvoidsKeysCopy();
    void storageValidationDelegatesToValidator();
    void visibilityManager_setViewOnFrontLayer_appliesConfiguredMode();
    void infoView_showEvent_keepsPopupOnTopLayer();
    void canvasConfigView_showEvent_staysAboveTheDock();
    void iconItem_appliesEffectsThroughTheStaticApi();
    void tabLayouts_onRawLayoutDropped_reportsAFailedImport();
    void layoutsController_addLayoutByText_guardsTheTemporaryFile();
    void importer_checksEveryArchiveOpen();
    void abilityMemberReadsResolve();
    void shippedJavaScriptIsImported();
    void eventsSink_mouseCasesShareOneBody();
    void notifyrcEventsMatchTheirEmitters();
    void factory_removeIndicator_reportsAFailedRemoval();
    void dialog_dropsCommentedOutAdjustGeometry();
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

void SourceGuardTest::positioner_geometryMethodsDelegateToPureUnit()
{
    const QString cpp = readFile(QStringLiteral("app/view/positioner.cpp"));
    QVERIFY2(!cpp.isEmpty(), "positioner.cpp not found");

    // updatePosition must call dockPosition
    const QString updatePos = stripped(functionBody(cpp, QStringLiteral("void Positioner::updatePosition")));
    QVERIFY2(updatePos.contains(QStringLiteral("PositionerGeometry::dockPosition(")),
             "updatePosition must delegate to PositionerGeometry::dockPosition");

    // resizeWindow must call windowSize
    const QString resize = stripped(functionBody(cpp, QStringLiteral("void Positioner::resizeWindow")));
    QVERIFY2(resize.contains(QStringLiteral("PositionerGeometry::windowSize(")),
             "resizeWindow must delegate to PositionerGeometry::windowSize");

    // maximumNormalGeometry must call the pure function
    const QString maxNorm = stripped(functionBody(cpp, QStringLiteral("QRect Positioner::maximumNormalGeometry")));
    QVERIFY2(maxNorm.contains(QStringLiteral("PositionerGeometry::maximumNormalGeometry(")),
             "maximumNormalGeometry must delegate to PositionerGeometry::maximumNormalGeometry");

    // updateCanvasGeometry must call canvasGeometry
    const QString canvas = stripped(functionBody(cpp, QStringLiteral("void Positioner::updateCanvasGeometry")));
    QVERIFY2(canvas.contains(QStringLiteral("PositionerGeometry::canvasGeometry(")),
             "updateCanvasGeometry must delegate to PositionerGeometry::canvasGeometry");

    // slideLocation must call slideEdge (via static_cast of PositionerGeometry::slideEdge)
    const QString slide = stripped(functionBody(cpp, QStringLiteral("WindowSystem::AbstractWindowInterface::Slide Positioner::slideLocation")));
    QVERIFY2(slide.contains(QStringLiteral("PositionerGeometry::slideEdge(")),
             "slideLocation must delegate to PositionerGeometry::slideEdge");

    // validateTopBottomBorders must call forcedBorders
    const QString borders = stripped(functionBody(cpp, QStringLiteral("void Positioner::validateTopBottomBorders")));
    QVERIFY2(borders.contains(QStringLiteral("PositionerGeometry::forcedBorders(")),
             "validateTopBottomBorders must delegate to PositionerGeometry::forcedBorders");
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

void SourceGuardTest::windowsTracker_updateExtraViewHints_delegatesToBucketing()
{
    const QString src = functionBody(readFile(QStringLiteral("app/wm/tracker/windowstracker.cpp")),
                                     QStringLiteral("void Windows::updateExtraViewHints()"));
    QVERIFY2(src.contains(QStringLiteral("ExtraViewHints::bucketHorizontalTouchingBusyVertical(")),
             "updateExtraViewHints must delegate to ExtraViewHints::bucketHorizontalTouchingBusyVertical");
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

void SourceGuardTest::windowsTracker_perEventIterationAvoidsKeysCopy()
{
    const QString file = readFile(QStringLiteral("app/wm/tracker/windowstracker.cpp"));

    // m_views.keys() must no longer appear in the file
    QVERIFY2(!file.contains(QStringLiteral("m_views.keys()")),
             "m_views.keys() must not appear — use cbegin/cend iterators");

    // m_layouts.keys() must also not appear (the layouts loop was de-keyed too)
    QVERIFY2(!file.contains(QStringLiteral("m_layouts.keys()")),
             "m_layouts.keys() must not appear — use cbegin/cend iterators");

    // m_windows.keys() must appear exactly once, inside cleanupFaultyWindows, which
    // legitimately snapshots keys before removing entries during iteration.
    const int count = file.count(QStringLiteral("m_windows.keys()"));
    QVERIFY2(count == 1, "m_windows.keys() must appear exactly once in windowstracker.cpp");
    const QString cleanup = stripped(functionBody(file, QStringLiteral("void Windows::cleanupFaultyWindows()")));
    QVERIFY2(!cleanup.isEmpty(), "cleanupFaultyWindows() not found");
    QVERIFY2(cleanup.contains(QStringLiteral("m_windows.keys()")),
             "m_windows.keys() must be inside cleanupFaultyWindows");
}

void SourceGuardTest::storageValidationDelegatesToValidator()
{
    const QString src = readFile(QStringLiteral("app/layouts/storage.cpp"));
    const QString body = functionBody(src, QStringLiteral("Storage::hasDifferentAppletsWithSameId"));
    QVERIFY2(body.contains(QStringLiteral("StorageValidator::differentAppletsWithSameId")),
             "hasDifferentAppletsWithSameId must delegate to the shared validator");
    QVERIFY2(body.contains(QStringLiteral("modelFromLive")) && body.contains(QStringLiteral("modelFromFile")),
             "hasDifferentAppletsWithSameId must build the model from live or file source");

    const QString orphanSub = functionBody(src, QStringLiteral("Storage::hasOrphanedSubContainments"));
    QVERIFY2(orphanSub.contains(QStringLiteral("StorageValidator::orphanedSubcontainments")),
             "hasOrphanedSubContainments inactive branch must delegate to the shared validator");
    QVERIFY2(orphanSub.contains(QStringLiteral("qobject_cast<Plasma::Applet")),
             "hasOrphanedSubContainments active branch must keep its live parent-walk");
}

void SourceGuardTest::visibilityManager_setViewOnFrontLayer_appliesConfiguredMode()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/view/visibilitymanager.cpp")),
                                            QStringLiteral("void VisibilityManager::setViewOnFrontLayer()")));
    QVERIFY2(!s.isEmpty(), "setViewOnFrontLayer() not found");
    // The front-layer path must hand the view's ACTUAL visibility mode to the window system so
    // layerFor(mode) resolves it -- AlwaysVisible/AutoHide/Dodge* -> LayerTop (above windows),
    // WindowsGoBelow -> LayerBottom. Omitting the mode falls back to the setViewExtraFlags default
    // (Types::WindowsGoBelow), which layerFor maps to LayerBottom, dropping an always-visible dock
    // underneath every window (the reported bug).
    QVERIFY2(s.contains(QStringLiteral("setViewExtraFlags(m_latteView,true,m_mode)")),
             "setViewOnFrontLayer must pass the configured m_mode, not rely on the WindowsGoBelow default");
    QVERIFY2(!s.contains(QStringLiteral("setViewExtraFlags(m_latteView,true);")),
             "setViewOnFrontLayer must not call the 2-arg form that defaults the layer to WindowsGoBelow/LayerBottom");
}

void SourceGuardTest::infoView_showEvent_keepsPopupOnTopLayer()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/infoview.cpp")),
                                            QStringLiteral("void InfoView::showEvent(QShowEvent *ev)")));
    QVERIFY2(!s.isEmpty(), "InfoView::showEvent() not found");
    // setupWaylandIntegration() puts the message popup on LayerTop so it is visible above windows.
    // showEvent must not clobber that with the mode-less setViewExtraFlags(this) whose default
    // (WindowsGoBelow) maps to LayerBottom, hiding the popup behind windows. Pass an explicit
    // above-windows mode instead.
    QVERIFY2(!s.contains(QStringLiteral("setViewExtraFlags(this);")),
             "InfoView::showEvent must not call the mode-less setViewExtraFlags(this) that defaults the popup to LayerBottom");
    QVERIFY2(s.contains(QStringLiteral("setViewExtraFlags(this,true,Latte::Types::AlwaysVisible)")),
             "InfoView::showEvent must keep the popup on the top layer via an explicit above-windows mode");
}

void SourceGuardTest::canvasConfigView_showEvent_staysAboveTheDock()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/view/settings/canvasconfigview.cpp")),
                                            QStringLiteral("void CanvasConfigView::showEvent(QShowEvent *ev)")));
    QVERIFY2(!s.isEmpty(), "CanvasConfigView::showEvent() not found");
    // The edit canvas overlays the dock -- its input region is click-through so events reach the widgets
    // beneath -- so it must sit on the top layer, above the dock. The dock now takes LayerTop in edit mode
    // (setViewOnFrontLayer passes the real mode), so the mode-less setViewExtraFlags(this,true) here,
    // defaulting to WindowsGoBelow -> LayerBottom, would leave the canvas stranded behind the dock.
    QVERIFY2(!s.contains(QStringLiteral("setViewExtraFlags(this,true);")),
             "CanvasConfigView::showEvent must not default the canvas to LayerBottom");
    QVERIFY2(s.contains(QStringLiteral("setViewExtraFlags(this,true,Latte::Types::AlwaysVisible)")),
             "CanvasConfigView::showEvent must keep the canvas on the top layer, above the dock");
}

void SourceGuardTest::iconItem_appliesEffectsThroughTheStaticApi()
{
    const QString s = readFile(QStringLiteral("declarativeimports/core/iconitem.cpp"));
    QVERIFY2(!s.isEmpty(), "iconitem.cpp unreadable");
    // KIconLoader::iconEffect() and KIconEffect::apply() went away in KF 6.5. The static
    // helpers mutate the pixmap in place, so a half-converted call site would compile as
    // a copy that is applied to nothing and silently drop the disabled/active rendering.
    QVERIFY2(!s.contains(QStringLiteral("iconEffect()->apply(")),
             "iconitem.cpp must not use the removed KIconLoader::iconEffect() API");
    QVERIFY2(s.contains(QStringLiteral("KIconEffect::toDisabled(")) && s.contains(QStringLiteral("KIconEffect::toActive(")),
             "iconitem.cpp must still apply the disabled and active icon effects");
}

void SourceGuardTest::tabLayouts_onRawLayoutDropped_reportsAFailedImport()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/settingsdialog/tablayoutshandler.cpp")),
                                            QStringLiteral("void TabLayouts::onRawLayoutDropped(const QString &rawLayout)")));
    QVERIFY2(!s.isEmpty(), "TabLayouts::onRawLayoutDropped() not found");
    // addLayoutByText() returns a default-constructed layout when the drop cannot be imported.
    // Announcing that unconditionally produced a green "Layout <b></b> imported successfully".
    QVERIFY2(s.contains(QStringLiteral("importedlayout.isEmpty()")),
             "a failed raw-layout drop must be detected before the success message");
    QVERIFY2(s.contains(QStringLiteral("KMessageWidget::Error")),
             "a failed raw-layout drop must be reported as an error");
}

void SourceGuardTest::layoutsController_addLayoutByText_guardsTheTemporaryFile()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/settingsdialog/layoutscontroller.cpp")),
                                            QStringLiteral("const Latte::Data::Layout Layouts::addLayoutByText(QString rawLayoutText)")));
    QVERIFY2(!s.isEmpty(), "Layouts::addLayoutByText() not found");
    // An unopened QTemporaryFile has an empty fileName(), so carrying on built a CentralLayout
    // over a nonexistent path and pushed a phantom row into the model.
    QVERIFY2(s.contains(QStringLiteral("if(!tempFile.open())")),
             "addLayoutByText must not continue when the temporary file cannot be opened");
}

void SourceGuardTest::importer_checksEveryArchiveOpen()
{
    const QString s = stripped(readFile(QStringLiteral("app/layouts/importer.cpp")));
    QVERIFY2(!s.isEmpty(), "importer.cpp unreadable");
    // KArchive::open() is nodiscard for a reason: every directory() walk below it dereferences
    // a null entry when the archive never opened.
    QVERIFY2(!s.contains(QStringLiteral("archive.open(QIODevice::ReadOnly);")),
             "importer.cpp must not ignore the result of KArchive::open()");
}

void SourceGuardTest::abilityMemberReadsResolve()
{
    // QML answers a read of a member nothing declares with `undefined` instead of an error, so a
    // renamed or mistyped ability member dies silently: the binding keeps its previous value, the
    // animation quietly falls back to a Qt default, or -- worst -- the expression throws mid-call
    // and abandons the rest of the function body. Nothing else catches these; the qmlloadcompile
    // gate only compiles the QML, and undefined member reads are resolved at run time.
    //
    // So resolve them here. For each ability namespace, collect the names its layers declare and
    // require every read across the shipped QML to be one of them.
    const QStringList shippedQml = qmlSourcesUnder({QStringLiteral("containment"),
                                                    QStringLiteral("plasmoid"),
                                                    QStringLiteral("declarativeimports"),
                                                    QStringLiteral("shell")});
    QVERIFY2(shippedQml.size() > 100, "found suspiciously few QML sources to scan");

    struct Namespace
    {
        QString reads;          // how the namespace is spelled at the call sites
        QStringList declaredIn; // every layer that may contribute a member
    };

    const QList<Namespace> namespaces = {
        {QStringLiteral("metrics"),
         {QStringLiteral("declarativeimports/abilities/definition/Metrics.qml"),
          QStringLiteral("declarativeimports/abilities/host/Metrics.qml"),
          QStringLiteral("declarativeimports/abilities/client/Metrics.qml"),
          QStringLiteral("containment/package/contents/ui/abilities/Metrics.qml"),
          QStringLiteral("containment/package/contents/ui/abilities/privates/MetricsPrivate.qml")}},
        {QStringLiteral("metrics.margin"),
         {QStringLiteral("declarativeimports/abilities/definition/metrics/Margin.qml")}},
        {QStringLiteral("metrics.totals"),
         {QStringLiteral("declarativeimports/abilities/definition/metrics/Totals.qml")}},
        {QStringLiteral("animations.duration"),
         {QStringLiteral("declarativeimports/abilities/definition/animations/Duration.qml")}},
        {QStringLiteral("myView.itemShadow"),
         {QStringLiteral("declarativeimports/abilities/definition/myview/ItemShadow.qml")}},
        {QStringLiteral("myView"),
         {QStringLiteral("declarativeimports/abilities/definition/MyView.qml"),
          QStringLiteral("declarativeimports/abilities/host/MyView.qml"),
          QStringLiteral("declarativeimports/abilities/client/MyView.qml"),
          // bridge/MyView.qml is an empty BridgeItem; host/client come from the base
          QStringLiteral("declarativeimports/abilities/bridge/BridgeItem.qml"),
          QStringLiteral("containment/package/contents/ui/abilities/MyView.qml"),
          QStringLiteral("containment/package/contents/ui/abilities/privates/MyViewPrivate.qml")}},
    };

    QStringList unresolved;
    for (const Namespace &ns : namespaces) {
        const QSet<QString> declared = declaredMembers(ns.declaredIn);
        QVERIFY2(!declared.isEmpty(), qPrintable(QStringLiteral("no declarations found for %1").arg(ns.reads)));
        unresolved << unresolvedReads(ns.reads, declared, shippedQml);
    }

    QVERIFY2(unresolved.isEmpty(),
             qPrintable(QStringLiteral("ability member reads that resolve to undefined:\n  %1")
                            .arg(unresolved.join(QStringLiteral("\n  ")))));
}

void SourceGuardTest::shippedJavaScriptIsImported()
{
    // A .js in a shipped package has exactly one entry point: an `import "....js"` from a QML
    // file next to it. Nothing else can reach it -- plasma_install_package ships the directory
    // wholesale, so no build file names the individual scripts, and qmlloadcompile only compiles
    // what the QML actually imports. A script nobody imports is therefore dead the moment its
    // last import goes, yet it keeps getting packaged and keeps reading like live code.
    const QStringList packages = {QStringLiteral("containment"),
                                  QStringLiteral("plasmoid"),
                                  QStringLiteral("declarativeimports"),
                                  QStringLiteral("shell"),
                                  QStringLiteral("indicators")};

    const QStringList scripts = sourcesUnder(packages, QStringLiteral("*.js"));
    QVERIFY2(!scripts.isEmpty(), "found no shipped JavaScript to scan");

    // Resolve each import against the importing file's own directory instead of matching base
    // names, so an import of a same-named script elsewhere in the tree cannot vouch for this one.
    // A leading `//` keeps a commented-out import from counting, which is the whole point.
    static const QRegularExpression jsImport(QStringLiteral("^[ \\t]*import\\s+\"([^\"]+\\.js)\""),
                                             QRegularExpression::MultilineOption);

    QSet<QString> imported;
    for (const QString &qml : qmlSourcesUnder(packages)) {
        QFile f(qml);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString dir = QFileInfo(qml).absolutePath();
        QRegularExpressionMatchIterator it = jsImport.globalMatch(QString::fromUtf8(f.readAll()));
        while (it.hasNext()) {
            imported.insert(QDir::cleanPath(QStringLiteral("%1/%2").arg(dir, it.next().captured(1))));
        }
    }

    const QString prefix = QStringLiteral("%1/").arg(QStringLiteral(REPO_ROOT));
    QStringList orphans;
    for (const QString &js : scripts) {
        if (!imported.contains(QDir::cleanPath(js))) {
            QString rel = js;
            rel.remove(prefix);
            orphans << rel;
        }
    }

    QVERIFY2(orphans.isEmpty(),
             qPrintable(QStringLiteral("shipped JavaScript no QML imports:\n  %1").arg(orphans.join(QStringLiteral("\n  ")))));
}

void SourceGuardTest::eventsSink_mouseCasesShareOneBody()
{
    const QString body = functionBody(readFile(QStringLiteral("app/view/eventssink.cpp")),
                                      QStringLiteral("QEvent *EventsSink::onEvent(QEvent *e)"));
    QVERIFY2(!body.isEmpty(), "EventsSink::onEvent() not found");

    // Move, press and release all clone the mouse event the same way; three copies of that
    // clone is three places for the argument list to drift apart.
    QCOMPARE(body.count(QStringLiteral("new QMouseEvent")), 1);

    const QString s = stripped(body);
    QVERIFY2(s.contains(QStringLiteral("caseQEvent::MouseMove:caseQEvent::MouseButtonPress:caseQEvent::MouseButtonRelease:")),
             "the three mouse cases must fall through to one shared body");

    // The cursor check belongs to moves alone. Applying it to press and release would swallow
    // clicks, and dropping the null test dereferences a positioner that teardown already took.
    QVERIFY2(s.contains(QStringLiteral("(me->type()!=QEvent::MouseMove)||(m_view->positioner()&&m_view->positioner()->isCursorInsideView())")),
             "presses and releases must sink unconditionally; only a move may consult the positioner, and only behind its null check");

    QVERIFY2(!body.contains(QStringLiteral("qDebug")),
             "onEvent must not print on every sunk press");

    // Wheel reads position() rather than scenePosition() and builds a different event, so it
    // stays out of the shared body.
    QCOMPARE(body.count(QStringLiteral("new QWheelEvent")), 1);
    QVERIFY2(s.contains(QStringLiteral("caseQEvent::Wheel:if(autowe=dynamic_cast<QWheelEvent*>(e))")),
             "the wheel case must keep its own cast and body");
}

void SourceGuardTest::notifyrcEventsMatchTheirEmitters()
{
    // The two halves of a notification live in different files and neither one checks the other.
    // A [Event/id] block nobody raises is dead weight that still lists itself in System Settings >
    // Notifications as if the user could configure something, and a KNotification whose id has no
    // block is worse -- KNotification answers a missing id with library defaults rather than an
    // error, so the popup silently stops appearing. Tie the two sides together here.
    const QString notifyrc = readFile(QStringLiteral("app/lattedock.notifyrc"));
    QVERIFY2(!notifyrc.isEmpty(), "app/lattedock.notifyrc not found");

    static const QRegularExpression eventHeader(QStringLiteral("^\\[Event/([^\\]]+)\\]"),
                                                QRegularExpression::MultilineOption);
    QSet<QString> declared;
    QRegularExpressionMatchIterator dit = eventHeader.globalMatch(notifyrc);
    while (dit.hasNext()) {
        declared.insert(dit.next().captured(1));
    }
    QVERIFY2(!declared.isEmpty(), "no [Event/...] blocks parsed out of lattedock.notifyrc");

    //! matches every spelling that reaches KNotification with a literal id: the QStringLiteral
    //! ctor, a bare "..." ctor, and the static KNotification::event()
    static const QRegularExpression emitterId(QStringLiteral("KNotification(?:::event)?\\s*\\(\\s*(?:QStringLiteral\\s*\\(\\s*)?\"([^\"]+)\""));
    QSet<QString> emitted;
    const QStringList cppRoots = {QStringLiteral("app"), QStringLiteral("plasmoid"), QStringLiteral("containment"), QStringLiteral("declarativeimports")};
    for (const QString &cpp : sourcesUnder(cppRoots, QStringLiteral("*.cpp"))) {
        QFile f(cpp);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        QRegularExpressionMatchIterator eit = emitterId.globalMatch(QString::fromUtf8(f.readAll()));
        while (eit.hasNext()) {
            emitted.insert(eit.next().captured(1));
        }
    }
    QVERIFY2(!emitted.isEmpty(), "found no KNotification event ids in any C++ source");

    QStringList unraised(declared.cbegin(), declared.cend());
    unraised.removeIf([&emitted](const QString &id) { return emitted.contains(id); });
    unraised.sort();
    QVERIFY2(unraised.isEmpty(),
             qPrintable(QStringLiteral("lattedock.notifyrc declares events nothing raises: %1").arg(unraised.join(QStringLiteral(", ")))));

    QStringList undeclared(emitted.cbegin(), emitted.cend());
    undeclared.removeIf([&declared](const QString &id) { return declared.contains(id); });
    undeclared.sort();
    QVERIFY2(undeclared.isEmpty(),
             qPrintable(QStringLiteral("KNotification ids with no lattedock.notifyrc block: %1").arg(undeclared.join(QStringLiteral(", ")))));
}

void SourceGuardTest::factory_removeIndicator_reportsAFailedRemoval()
{
    // removeIndicator() shells out to kpackagetool6 behind a modal confirmation, so there is no
    // headless repro. What matters is that both outcomes speak: a bare `if (exitCode == 0)` leaves
    // the user staring at an indicator they just told the dialog to delete, with no message at all.
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/indicator/factory.cpp")),
                                            QStringLiteral("void Factory::removeIndicator(QString id)")));
    QVERIFY2(!s.isEmpty(), "Factory::removeIndicator() not found");
    QVERIFY2(s.contains(QStringLiteral("if(process.exitCode()==0){showRemovedSucceed(pluginName);}else{showRemovedFailed(pluginName);}")),
             "a kpackagetool6 removal that fails must report it, not return silently");
}

void SourceGuardTest::dialog_dropsCommentedOutAdjustGeometry()
{
    const QString h = readFile(QStringLiteral("declarativeimports/core/dialog.h"));
    const QString cpp = readFile(QStringLiteral("declarativeimports/core/dialog.cpp"));
    QVERIFY2(!h.isEmpty() && !cpp.isEmpty(), "dialog sources not found");

    // Popup placement moved to popupPosition() in 2021 and the superseded adjustGeometry() override
    // was left behind commented out. Every fix since landed in popupPosition() only, so the copy had
    // drifted into a decoy: it clamped x to screengeometry.right()-1, pinning a popup's LEFT edge one
    // pixel inside the screen edge and pushing the rest of it off-screen.
    QVERIFY2(!cpp.contains(QStringLiteral("Dialog::adjustGeometry")),
             "the commented-out adjustGeometry copy must stay out of dialog.cpp");
    QVERIFY2(!h.contains(QStringLiteral("adjustGeometry")),
             "the commented-out adjustGeometry declaration must stay out of dialog.h");

    // The arithmetic the dead copy never grew, and the reason resurrecting it would regress placement.
    const QString s = stripped(functionBody(cpp, QStringLiteral("QPoint Dialog::popupPosition(QQuickItem *item, const QSize &size)")));
    QVERIFY2(!s.isEmpty(), "popupPosition() not found");
    QVERIFY2(s.contains(QStringLiteral("screengeometry-=QMargins(0,popupmargin,0,popupmargin)")),
             "popupPosition must inset the screen by the popup margin on the vertical edges");
    QVERIFY2(s.contains(QStringLiteral("screengeometry-=QMargins(popupmargin,0,popupmargin,0)")),
             "popupPosition must inset the screen by the popup margin on the horizontal edges");
    QVERIFY2(s.contains(QStringLiteral("screengeometry.right()-size.width()+1")),
             "popupPosition must clamp x so the popup's right edge stays on screen, not its left");
    QVERIFY2(s.contains(QStringLiteral("screengeometry.bottom()-size.height()+1")),
             "popupPosition must clamp y so the popup's bottom edge stays on screen, not its top");
}

QTEST_GUILESS_MAIN(SourceGuardTest)

#include "sourceguardtest.moc"
