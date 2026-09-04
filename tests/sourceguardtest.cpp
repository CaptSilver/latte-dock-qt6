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
    static QString readAbsolute(const QString &abs)
    {
        QFile f(abs);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }
        return QString::fromUtf8(f.readAll());
    }

    static QString readFile(const QString &rel)
    {
        return readAbsolute(QStringLiteral("%1/%2").arg(QStringLiteral(REPO_ROOT), rel));
    }

    // Absolute paths make a failure message unreadable and machine-specific; the tree-walking
    // guards report what a reader can paste into an editor.
    static QString relativeToRepo(const QString &abs)
    {
        QString rel = abs;
        rel.remove(QStringLiteral("%1/").arg(QStringLiteral(REPO_ROOT)));
        return rel;
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

    // Comment-free copy of a C++ source, with string and char literals left alone so a `//` inside
    // a URL cannot eat the rest of its line. A name that survives only in prose is not a use:
    // generictoolstest.cpp writes ICONMARGIN three times in comments and never once in code, which
    // is exactly the direction a raw text count reads backwards.
    static QString withoutComments(const QString &src)
    {
        QString out;
        out.reserve(src.size());

        for (int i = 0; i < src.size();) {
            const QChar c = src.at(i);

            if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                const QChar quote = c;
                out += c;
                ++i;
                while (i < src.size()) {
                    if (src.at(i) == QLatin1Char('\\')) {
                        out += QLatin1String("  ");
                        i += 2;
                        continue;
                    }
                    out += src.at(i);
                    const bool closing = src.at(i) == quote;
                    ++i;
                    if (closing) {
                        break;
                    }
                }
                continue;
            }

            if (c == QLatin1Char('/') && i + 1 < src.size() && src.at(i + 1) == QLatin1Char('/')) {
                while (i < src.size() && src.at(i) != QLatin1Char('\n')) {
                    ++i;
                }
                continue;
            }

            if (c == QLatin1Char('/') && i + 1 < src.size() && src.at(i + 1) == QLatin1Char('*')) {
                i += 2;
                while (i < src.size() && !(src.at(i) == QLatin1Char('*') && i + 1 < src.size() && src.at(i + 1) == QLatin1Char('/'))) {
                    //! newlines are kept so the line-anchored declaration scan still sees line starts
                    if (src.at(i) == QLatin1Char('\n')) {
                        out += src.at(i);
                    }
                    ++i;
                }
                i += 2;
                continue;
            }

            out += c;
            ++i;
        }

        return out;
    }

    // Emptied string and char literals, on top of withoutComments. A macro name inside a
    // literal is not a use of the macro: settingsdialog.cpp prints "VERSION :::: " and would
    // otherwise vouch for a #cmakedefine no translation unit reads.
    static QString withoutStringBodies(const QString &commentFree)
    {
        // withoutComments has already flattened every escape, so a literal cannot contain
        // its own delimiter by the time this runs.
        static const QRegularExpression literal(QStringLiteral("\"[^\"]*\"|'[^']*'"));
        QString out = commentFree;
        out.remove(literal);
        return out;
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
    void containmentInterface_appletExpansionTrackedThroughOneHelper();
    void containmentInterface_trackAppletExpansion_guardsBeforeConnecting();
    void wm_skipTaskBarNoOpIsGone();
    void corona_dropsTheOrphanAboutDialog();
    void dataTables_dropDeadCopyAndRedundantEarlyOuts();
    void alignmentStateSelfReadsResolve();
    void commonTools_standardPath_dropsTheDeadReverseSearch();
    void orphanHeaderDeclarationsAreGone();
    void editModeLogSinkExistsOnlyInDebugOutput();
    void namespaceConstantsNobodyReadsAreDeleted();
    void delegatePaintDropsItsUnreadLocals();
    void stdNamespaceIsNotReopened();
    void configuredHeaderMacrosAreAllRead();
    void qmlSignalHandlersDeclareTheirParameters();
    void parabolicRelaysDropTheirUnreadScales();
    void latteQmlModulesShipNoUnreachableFiles();
    void qmldirExportsResolveToTheirFiles();
    void comboBoxDropsItsDeadMobileTextMachinery();
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

void SourceGuardTest::containmentInterface_appletExpansionTrackedThroughOneHelper()
{
    const QString cpp = readFile(QStringLiteral("app/view/containmentinterface.cpp"));
    QVERIFY2(!cpp.isEmpty(), "containmentinterface.cpp not found");

    const QString body = functionBody(cpp, QStringLiteral("void ContainmentInterface::onAppletAdded"));
    QVERIFY2(!body.isEmpty(), "onAppletAdded() not found");

    // The sub-containment host, each of its internal applets and the plain-applet arm all register
    // the same pair of connections. Three verbatim pastes is three places for one to drift.
    QCOMPARE(body.count(QStringLiteral("trackAppletExpansion(")), 3);
    QVERIFY2(!body.contains(QStringLiteral("expandedChanged")),
             "onAppletAdded must not wire expandedChanged itself; that belongs to trackAppletExpansion");

    const QString s = stripped(body);
    // Tasks applets must stay out of expansion tracking. appletIsExpandable() does not exclude them,
    // so calling the helper above these two tests would flip hasExpandedApplet on every task popup
    // and repaint the panel background -- with nothing headless to catch it.
    QVERIFY2(s.contains(QStringLiteral("org.kde.latte.plasmoid")) && s.contains(QStringLiteral("org.kde.plasma.multitasking")),
             "the tasks branches must keep claiming their applets before expansion tracking");
    QVERIFY2(s.contains(QStringLiteral("}else{trackAppletExpansion(ai);}")),
             "the plain-applet registration must stay the else arm of the tasks chain");
    QVERIFY2(s.lastIndexOf(QStringLiteral("trackAppletExpansion(")) > s.indexOf(QStringLiteral("org.kde.plasma.multitasking")),
             "expansion tracking must be reached only after both tasks tests have failed");
}

void SourceGuardTest::containmentInterface_trackAppletExpansion_guardsBeforeConnecting()
{
    const QString h = readFile(QStringLiteral("app/view/containmentinterface.h"));
    const QString cpp = readFile(QStringLiteral("app/view/containmentinterface.cpp"));
    QVERIFY2(!h.isEmpty() && !cpp.isEmpty(), "containmentinterface sources not found");

    const QString s = stripped(functionBody(cpp, QStringLiteral("void ContainmentInterface::trackAppletExpansion")));
    QVERIFY2(!s.isEmpty(), "trackAppletExpansion() not found");

    // updateAppletsTracking() replays onAppletAdded over every applet on a timer, so the membership
    // guard is the only thing stopping the connects from stacking up.
    QVERIFY2(s.contains(QStringLiteral("m_expansionTrackedApplets.contains(appletQuickItem)")),
             "trackAppletExpansion must skip an applet it already tracks");
    QVERIFY2(s.contains(QStringLiteral("{return;}")),
             "the already-tracked guard must early-return");
    QVERIFY2(s.contains(QStringLiteral("m_expansionTrackedApplets.insert(appletQuickItem);")),
             "an applet must join the tracked set, or the guard above can never fire");
    QVERIFY2(s.contains(QStringLiteral("AppletQuickItem::expandedChanged")) && s.contains(QStringLiteral("QObject::destroyed")),
             "trackAppletExpansion must make both the expandedChanged and the destroyed connection");
    QVERIFY2(!s.contains(QStringLiteral("[&,")),
             "the destroyed lambda reads nothing by reference; capture this and the pointer by value");

    // Membership is all the code ever asked of the container: the stored QMetaObject::Connection was
    // written at three sites, read at none, and no disconnect() ever consumed it.
    QVERIFY2(!stripped(h).contains(QStringLiteral("QHash<PlasmaQuick::AppletQuickItem*,QMetaObject::Connection>")),
             "the write-only connection handle must be gone from the header");
    QVERIFY2(stripped(h).contains(QStringLiteral("QSet<PlasmaQuick::AppletQuickItem*>m_expansionTrackedApplets;")),
             "the tracked applets must be a plain QSet");
}

void SourceGuardTest::wm_skipTaskBarNoOpIsGone()
{
    // skipTaskBar's only implementation was an empty Q_UNUSED body plus a TODO, so the
    // About dialog never skipped anything. The virtual, both overrides and the single
    // call site have to go together -- any half of that edit is ill-formed.
    const QStringList files = {QStringLiteral("app/wm/abstractwindowinterface.h"),
                               QStringLiteral("app/wm/waylandinterface.h"),
                               QStringLiteral("app/wm/waylandinterface.cpp"),
                               QStringLiteral("app/lattecorona.cpp"),
                               QStringLiteral("tests/lastactivewindowtest.cpp")};

    for (const QString &rel : files) {
        const QString src = readFile(rel);
        QVERIFY2(!src.isEmpty(), qPrintable(QStringLiteral("%1 is unreadable").arg(rel)));
        QVERIFY2(!src.contains(QStringLiteral("skipTaskBar")),
                 qPrintable(QStringLiteral("%1 still mentions skipTaskBar").arg(rel)));
    }

    // Losing the QDialog parameter leaves five includes in a header 47 translation units deep
    // that nothing below it reads.
    const QString h = stripped(readFile(QStringLiteral("app/wm/abstractwindowinterface.h")));
    QVERIFY2(!h.isEmpty(), "abstractwindowinterface.h is unreadable");
    const QStringList dead = {QStringLiteral("#include<unordered_map>"),
                              QStringLiteral("#include<list>"),
                              QStringLiteral("#include<QDialog>"),
                              QStringLiteral("#include<QMap>"),
                              QStringLiteral("#include<QScreen>")};

    for (const QString &inc : dead) {
        QVERIFY2(!h.contains(inc),
                 qPrintable(QStringLiteral("abstractwindowinterface.h still carries %1").arg(inc)));
    }

    // currentScreenGeometries() dereferences QScreen and used to get the type from its own
    // header; pin the include so the removal above cannot silently break it.
    QVERIFY2(stripped(readFile(QStringLiteral("app/wm/abstractwindowinterface.cpp"))).contains(QStringLiteral("#include<QScreen>")),
             "abstractwindowinterface.cpp must include <QScreen> for its own use");
}

void SourceGuardTest::corona_dropsTheOrphanAboutDialog()
{
    // Corona::aboutApplication and Layouts::Manager::showAboutDialog are leftovers from the
    // switch to KHelpMenu: the QAction that reached them was deleted, so nothing called either
    // one. Their only cost was dragging <KAboutApplicationDialog> through every translation
    // unit that includes lattecorona.h.
    const QString coronaHeader = readFile(QStringLiteral("app/lattecorona.h"));
    QVERIFY2(!coronaHeader.isEmpty(), "lattecorona.h is unreadable");
    QVERIFY2(!coronaHeader.contains(QStringLiteral("KAboutApplicationDialog")),
             "lattecorona.h must not name KAboutApplicationDialog");
    QVERIFY2(!coronaHeader.contains(QStringLiteral("aboutApplication")),
             "lattecorona.h must not declare aboutApplication");

    const QString coronaSource = readFile(QStringLiteral("app/lattecorona.cpp"));
    QVERIFY2(!coronaSource.isEmpty(), "lattecorona.cpp is unreadable");
    QVERIFY2(!coronaSource.contains(QStringLiteral("aboutDialog")),
             "lattecorona.cpp must not keep the About dialog member alive");
    QVERIFY2(!coronaSource.contains(QStringLiteral("KAboutData")),
             "lattecorona.cpp must drop <KAboutData> once its only user is gone");

    for (const QString &rel : {QStringLiteral("app/layouts/manager.h"), QStringLiteral("app/layouts/manager.cpp")}) {
        const QString src = readFile(rel);
        QVERIFY2(!src.isEmpty(), qPrintable(QStringLiteral("%1 is unreadable").arg(rel)));
        QVERIFY2(!src.contains(QStringLiteral("showAboutDialog")),
                 qPrintable(QStringLiteral("%1 still forwards to the orphaned About dialog").arg(rel)));
    }

    // The settings window's Help menu is the About surface users actually reach, which is why
    // the code above is dead rather than missing. Anyone tempted to "restore" it should see
    // this first: KHelpMenu already ships an About Latte entry, and nothing hides it.
    const QString settings = readFile(QStringLiteral("app/settings/settingsdialog/settingsdialog.cpp"));
    QVERIFY2(!settings.isEmpty(), "settingsdialog.cpp is unreadable");
    QVERIFY2(settings.contains(QStringLiteral("new KHelpMenu(")),
             "the settings window must still build a KHelpMenu");
    QVERIFY2(!settings.contains(QStringLiteral("KHelpMenu::menuAboutApp")),
             "nothing may hide the KHelpMenu About entry, it is the only About surface left");
}

void SourceGuardTest::dataTables_dropDeadCopyAndRedundantEarlyOuts()
{
    const QString views = readFile(QStringLiteral("app/data/viewstable.cpp"));
    QVERIFY2(!views.isEmpty(), "viewstable.cpp is unreadable");

    const QString equality = stripped(functionBody(views, QStringLiteral("bool ViewsTable::operator==(const ViewsTable &rhs) const")));
    QVERIFY2(!equality.isEmpty(), "ViewsTable::operator== not found");
    QVERIFY2(!equality.contains(QStringLiteral("tempView")),
             "ViewsTable::operator== must not build a table copy it never reads");
    //! a C-style cast to a non-reference class type slices a whole temporary into existence on
    //! each side; the base operator takes rhs by reference, so name it instead
    QVERIFY2(!equality.contains(QStringLiteral("(GenericTable<View>)")),
             "ViewsTable::operator== must not slice both sides through C-style casts");
    QVERIFY2(equality.contains(QStringLiteral("GenericTable<View>::operator==(rhs)")),
             "ViewsTable::operator== must qualify the base comparison it hides");
    //! isInitialized is the whole of Views::hasChangedData(), and layoutscontroller is its only
    //! writer -- trimming this term leaves a dialog that has changes reporting none, silently
    QVERIFY2(equality.contains(QStringLiteral("isInitialized==rhs.isInitialized")),
             "ViewsTable::operator== must keep comparing isInitialized");

    //! when the two sides compare equal every id is in rhs, so the loop below the early-out
    //! appends nothing and returns the same empty table the early-out returned
    const QString viewsSubtracted = stripped(functionBody(views, QStringLiteral("ViewsTable ViewsTable::subtracted(const ViewsTable &rhs) const")));
    QVERIFY2(!viewsSubtracted.isEmpty(), "ViewsTable::subtracted not found");
    QVERIFY2(!viewsSubtracted.contains(QStringLiteral("(*this)==rhs")),
             "ViewsTable::subtracted must not keep an early-out that cannot change its result");

    const QString layouts = readFile(QStringLiteral("app/data/layoutstable.cpp"));
    QVERIFY2(!layouts.isEmpty(), "layoutstable.cpp is unreadable");
    const QString layoutsSubtracted = stripped(functionBody(layouts, QStringLiteral("LayoutsTable LayoutsTable::subtracted(const LayoutsTable &rhs) const")));
    QVERIFY2(!layoutsSubtracted.isEmpty(), "LayoutsTable::subtracted not found");
    QVERIFY2(!layoutsSubtracted.contains(QStringLiteral("(*this)==rhs")),
             "LayoutsTable::subtracted must not keep an early-out that cannot change its result");
    QVERIFY2(!stripped(layouts).contains(QStringLiteral("#include<QDebug>")),
             "layoutstable.cpp must drop <QDebug>, nothing in it logs");
}

void SourceGuardTest::alignmentStateSelfReadsResolve()
{
    // Same silent-undefined class as abilityMemberReadsResolve, one scope down: these two
    // alignment state machines address their own root by id, so a member the file no longer
    // declares still parses and still binds. `lastMargin` was exactly that -- deleted in 2019,
    // read by both files for years afterwards. It never warned, because QQuickAnchors gives
    // every side margin a RESET, so an undefined result resets the margin instead of failing
    // the assignment. ScrollableList.qml is a copy of AppletsContainer.qml, so anything that
    // rots in one rots in both.
    struct RootScope
    {
        QString file;
        QString id;
    };

    const QList<RootScope> scopes = {
        {QStringLiteral("containment/package/contents/ui/layouts/AppletsContainer.qml"), QStringLiteral("appletsContainer")},
        {QStringLiteral("plasmoid/package/contents/ui/taskslayout/ScrollableList.qml"), QStringLiteral("flickableContainer")}};

    QStringList unresolved;
    for (const RootScope &scope : scopes) {
        const QSet<QString> declared = declaredMembers({scope.file});
        QVERIFY2(!declared.isEmpty(), qPrintable(QStringLiteral("no declarations found in %1").arg(scope.file)));
        unresolved << unresolvedReads(scope.id, declared, {QStringLiteral("%1/%2").arg(QStringLiteral(REPO_ROOT), scope.file)});
    }

    QVERIFY2(unresolved.isEmpty(),
             qPrintable(QStringLiteral("alignment-state reads that resolve to undefined:\n  %1")
                            .arg(unresolved.join(QStringLiteral("\n  ")))));
}

void SourceGuardTest::commonTools_standardPath_dropsTheDeadReverseSearch()
{
    // Latte::standardPath took a localfirst bool that selected between a forward and a
    // reverse walk of GenericDataLocation. Every caller in the tree took the default, so
    // the reverse walk could never run -- and it is the kind of dead branch that reads
    // like a supported mode, so the next caller reaches for a flag that was never used.
    // Nothing behavioural can catch its return, hence the source-level guard.
    const QString src = readFile(QStringLiteral("app/tools/commontools.cpp"));
    QVERIFY2(!src.isEmpty(), "commontools.cpp is unreadable");

    const QString s = stripped(functionBody(src, QStringLiteral("QString standardPath(QString subPath)")));
    QVERIFY2(!s.isEmpty(), "standardPath(QString subPath) not found -- it must take the subPath alone");
    QVERIFY2(!s.contains(QStringLiteral("localfirst")),
             "standardPath must not keep a branch on localfirst, no caller ever passes it");
    QVERIFY2(!s.contains(QStringLiteral("}else{")),
             "standardPath must not keep the unreachable reverse-order search");

    const QString header = stripped(readFile(QStringLiteral("app/tools/commontools.h")));
    QVERIFY2(!header.isEmpty(), "commontools.h is unreadable");
    QVERIFY2(header.contains(QStringLiteral("QStringstandardPath(QStringsubPath);")),
             "commontools.h must declare standardPath without the localFirst parameter");
}

void SourceGuardTest::orphanHeaderDeclarationsAreGone()
{
    // Declarations left behind by the port: no definition anywhere, no caller anywhere.
    // The private member functions cost nothing but a reader's time -- the linker would
    // have caught anyone calling them. The signals are the reason this guard exists: moc
    // defines them, so a stale signal keeps a meta-object slot alive and reads as a live
    // notification that nothing ever emits.
    //
    // This list is a known set, NOT an inventory of the tree. Finding these by "the name
    // occurs once tree-wide" structurally cannot see an orphan whose name collides with a
    // live symbol on another class, which is how the second half of this table stayed
    // hidden through two passes -- GlobalShortcuts::modifiersChanged behind
    // ModifierTracker's, View::containmentById behind ContextMenuLayerQuickItem's. Search
    // for `<Class>::<name>(` instead, and remember a declared-but-undefined member only
    // link-errors if something calls it.
    struct Orphan
    {
        const char *header;
        const char *name;
    };

    static const Orphan orphans[] = {
        {"app/lattecorona.h", "configurationShown"},
        {"app/wm/tracker/windowstracker.h", "enabledChangedForLayout"},
        {"app/layouts/synchronizer.h", "runningActicitiesChanged"},
        {"app/settings/universalsettings.h", "downloadWindowSizeChanged"},
        {"app/settings/universalsettings.h", "layoutsColumnWidthsChanged"},
        {"app/settings/universalsettings.h", "layoutsWindowSizeChanged"},
        {"app/view/effects.h", "backgroundRadiusIsEnabled"},
        {"app/plasma/extended/theme.h", "loadCompositingRoundness"},
        {"app/layout/centrallayout.h", "importLocalLayout"},
        {"app/layouts/manager.h", "setMenuLayouts"},
        {"app/settings/exporttemplatedialog/exporttemplatehandler.h", "loadViewApplets"},
        {"app/settings/settingsdialog/settingsdialog.h", "setCurrentFreeActivitiesLayout"},
        {"app/shortcuts/globalshortcuts.h", "initModifiers"},
        {"containment/plugin/layoutmanager.h", "saveOption"},
        {"declarativeimports/core/environment.h", "loadPlasmaDesktopVersion"},
        {"plasmoid/plugin/smartlauncherbackend.h", "setupApplicationJobs"},
        //! found only by class-qualified search -- each of these shares its name with a live
        //! member of an unrelated class, so a bare grep reports the sibling and moves on
        {"app/shortcuts/globalshortcuts.h", "modifiersChanged"},
        {"app/settings/settingsdialog/settingsdialog.h", "initLayoutMenu"},
        {"app/layout/abstractlayout.h", "setTextColor"},
        {"app/view/view.h", "initSignalingForLocationChangeSliding"},
        {"app/view/view.h", "updateAppletContainsMethod"},
        {"app/view/view.h", "containmentById"},
        {"app/settings/viewsdialog/viewscontroller.h", "sortByColumn"},
    };

    for (const Orphan &orphan : orphans) {
        const QString rel = QString::fromUtf8(orphan.header);
        const QString name = QString::fromUtf8(orphan.name);
        const QString src = readFile(rel);
        QVERIFY2(!src.isEmpty(), qPrintable(QStringLiteral("%1 is unreadable").arg(rel)));
        // Word-bounded, not contains(): saveOption is a prefix of the live saveOptions()
        // sitting a few lines above it in the same header.
        QVERIFY2(!src.contains(QRegularExpression(QStringLiteral("\\b%1\\b").arg(name))),
                 qPrintable(QStringLiteral("%1 still declares the orphan %2").arg(rel, name)));
    }

    // configurationShown was the only thing in lattecorona.h naming a PlasmaQuick type, and
    // that header reaches most of app/ -- put the include back and every one of those
    // translation units pays for QQmlEngine and QQuickWindow again.
    QVERIFY2(!readFile(QStringLiteral("app/lattecorona.h")).contains(QStringLiteral("PlasmaQuick/ConfigView")),
             "lattecorona.h must not include <PlasmaQuick/ConfigView>, nothing in it needs the type");
}

void SourceGuardTest::editModeLogSinkExistsOnlyInDebugOutput()
{
    // View::debugLog and Interfaces::debugLog each carried a private copy of the same
    // O_NOFOLLOW/fdopen sink, so the process held two never-closed handles on one file.
    // Both must now forward to the single implementation in debugoutput.cpp.
    const QString sink = readFile(QStringLiteral("app/debugoutput.cpp"));
    QVERIFY2(sink.contains(QStringLiteral("O_NOFOLLOW")) && sink.contains(QStringLiteral("fdopen")),
             "the edit-mode log sink must live in debugoutput.cpp");

    // One handle for the process lifetime is the whole point of the merge, and no behavioural
    // test can see it: O_APPEND plus a per-line fflush makes N handles byte-identical to one.
    // Drop the static and every call leaks a fresh descriptor, which is worse than the two this
    // replaced -- so pin it here.
    QVERIFY2(stripped(functionBody(sink, QStringLiteral("void editModeLog(const QString &msg)"))).contains(QStringLiteral("staticFILE*logfile")),
             "the sink must be a single process-lifetime handle, not one per call");

    struct Forwarder
    {
        const char *file;
        const char *signature;
    };

    static const Forwarder forwarders[] = {
        {"app/view/view.cpp", "void View::debugLog(const QString &msg) const"},
        {"app/declarativeimports/interfaces.cpp", "void Interfaces::debugLog(const QString &msg) const"},
    };

    for (const Forwarder &forwarder : forwarders) {
        const QString rel = QString::fromUtf8(forwarder.file);
        const QString src = readFile(rel);
        QVERIFY2(!src.isEmpty(), qPrintable(QStringLiteral("%1 is unreadable").arg(rel)));
        QVERIFY2(!src.contains(QStringLiteral("O_NOFOLLOW")) && !src.contains(QStringLiteral("fdopen")),
                 qPrintable(QStringLiteral("%1 has grown its own copy of the edit-mode log sink").arg(rel)));

        const QString body = stripped(functionBody(src, QString::fromUtf8(forwarder.signature)));
        QVERIFY2(body.contains(QStringLiteral("DebugOutput::editModeLog(msg)")),
                 qPrintable(QStringLiteral("%1 must forward debugLog to DebugOutput::editModeLog").arg(rel)));
    }
}

void SourceGuardTest::namespaceConstantsNobodyReadsAreDeleted()
{
    // A const at namespace scope has internal linkage, so a SHOUTY name that appears exactly once
    // in its own .cpp -- the definition -- cannot be read from anywhere at all. Nothing in the
    // build says so: app/ compiles with -Wextra and no -Wall, and GCC leaves C++ out of
    // -Wunused-const-variable regardless, which is how eleven copy-pasted drawing margins piled up
    // unnoticed. Being dead was the lesser problem -- layoutnamedelegate.cpp declared
    // INDICATORCHANGESMARGIN = 2 beside the 5 that generictools.cpp actually draws that indicator
    // with, so reading the delegate told you the wrong geometry.
    //
    // Headers are deliberately out of scope: a constant declared there exists to be read from
    // other translation units, so the appears-once rule means nothing.
    static const QRegularExpression declaration(QStringLiteral("^(?:static\\s+)?const(?:expr)?\\s+[\\w:]+(?:\\s*<[^>]*>)?\\s*[*&]?\\s*([A-Z][A-Z0-9_]{2,})\\s*(?:=|\\{)"));

    const QStringList cppRoots = {QStringLiteral("app"), QStringLiteral("containment"), QStringLiteral("containmentactions"), QStringLiteral("declarativeimports"), QStringLiteral("plasmoid")};
    const QStringList sources = sourcesUnder(cppRoots, QStringLiteral("*.cpp"));
    QVERIFY2(sources.size() > 100, qPrintable(QStringLiteral("only %1 C++ sources walked, the roots are wrong").arg(sources.size())));

    int declarations = 0;
    QStringList unread;

    for (const QString &path : sources) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        const QString src = withoutComments(QString::fromUtf8(f.readAll()));
        const QStringList lines = src.split(QLatin1Char('\n'));

        for (const QString &line : lines) {
            const QRegularExpressionMatch m = declaration.match(line);
            if (!m.hasMatch()) {
                continue;
            }
            ++declarations;

            const QString name = m.captured(1);
            int uses = 0;
            QRegularExpressionMatchIterator it = QRegularExpression(QStringLiteral("\\b%1\\b").arg(name)).globalMatch(src);
            while (it.hasNext()) {
                it.next();
                ++uses;
            }

            if (uses == 1) {
                unread << QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), name);
            }
        }
    }

    //! a regex that has quietly stopped matching would report a spotless tree forever
    QVERIFY2(declarations > 20, qPrintable(QStringLiteral("only %1 namespace-scope constants recognised, the declaration pattern has drifted").arg(declarations)));

    unread.sort();
    QVERIFY2(unread.isEmpty(),
             qPrintable(QStringLiteral("namespace-scope constants nothing reads: %1").arg(unread.join(QStringLiteral(", ")))));
}

void SourceGuardTest::delegatePaintDropsItsUnreadLocals()
{
    // paint() runs per cell per repaint, and both of these threw their result away.
    // backgrounddelegate copied a whole QStyleOptionViewItem and then handed `option` to the draw
    // helpers anyway; layoutnamedelegate paid for a virtual model data() call and a QVariant
    // convert for a flag it never branched on. -Wunused-variable is off in this build, so a
    // reader is the only thing that catches them coming back.
    struct DeadLocal
    {
        const char *file;
        const char *signature;
        const char *local;
    };

    static const DeadLocal deadLocals[] = {
        {"app/settings/settingsdialog/delegates/backgrounddelegate.cpp",
         "void BackgroundDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const",
         "myOptions"},
        {"app/settings/settingsdialog/delegates/layoutnamedelegate.cpp",
         "void LayoutName::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const",
         "inMultiple"},
    };

    for (const DeadLocal &dead : deadLocals) {
        const QString rel = QString::fromUtf8(dead.file);
        const QString body = functionBody(withoutComments(readFile(rel)), QString::fromUtf8(dead.signature));
        QVERIFY2(!body.isEmpty(), qPrintable(QStringLiteral("%1 paint() not found").arg(rel)));

        const QString local = QString::fromUtf8(dead.local);
        QVERIFY2(!QRegularExpression(QStringLiteral("\\b%1\\b").arg(local)).match(body).hasMatch(),
                 qPrintable(QStringLiteral("%1 paint() computes %2 and never reads it").arg(rel, local)));
    }
}

void SourceGuardTest::stdNamespaceIsNotReopened()
{
    // Adding an overload to the standard library's own namespace is undefined behaviour, and
    // for make_unique it is a plain redefinition against any C++14-or-later libstdc++. The one
    // that lived in extras.h was reachable only through a build flag no supported entry point
    // set, so it sat there as a landmine for whoever first turned it on.
    const QStringList cppRoots = {QStringLiteral("app"), QStringLiteral("containment"), QStringLiteral("containmentactions"),
                                  QStringLiteral("declarativeimports"), QStringLiteral("plasmoid"), QStringLiteral("shell"),
                                  QStringLiteral("tests")};

    //! written with escapes, so this file does not match its own guard and needs no exemption --
    //! an exemption here would blind the guard to the one file most likely to grow a copy
    static const QRegularExpression reopened(QStringLiteral("\\bnamespace\\s+std\\s*\\{"));

    QStringList sources = sourcesUnder(cppRoots, QStringLiteral("*.cpp"));
    sources << sourcesUnder(cppRoots, QStringLiteral("*.h"));
    QVERIFY2(sources.size() > 100, qPrintable(QStringLiteral("only %1 C++ sources walked, the roots are wrong").arg(sources.size())));

    for (const QString &path : std::as_const(sources)) {
        QVERIFY2(!reopened.match(withoutComments(readAbsolute(path))).hasMatch(),
                 qPrintable(QStringLiteral("%1 reopens the standard library namespace").arg(relativeToRepo(path))));
    }
}

void SourceGuardTest::configuredHeaderMacrosAreAllRead()
{
    // A #cmakedefine nobody reads keeps its whole chain alive: the CMake variable behind it,
    // whatever computes that variable, and the include path that lets the generated header
    // resolve. KF6_VERSION_MINOR cost two string(REGEX) calls and a status line at configure
    // time for a macro that appeared in no translation unit at all.
    const QStringList configRoots = {QStringLiteral("app"), QStringLiteral("declarativeimports")};
    const QStringList templates = sourcesUnder(configRoots, QStringLiteral("*.h.cmake"));
    QVERIFY2(!templates.isEmpty(), "no configured header templates found");

    // This tree configures in-source, so each generated header sits beside its own template and
    // #defines every name in it. Scanning those would let the question answer itself.
    QSet<QString> generated;
    for (const QString &tmpl : templates) {
        generated.insert(tmpl.chopped(QStringLiteral(".cmake").size()));
    }

    const QStringList cppRoots = {QStringLiteral("app"), QStringLiteral("containment"), QStringLiteral("containmentactions"),
                                  QStringLiteral("declarativeimports"), QStringLiteral("plasmoid"), QStringLiteral("tests")};
    QStringList sources = sourcesUnder(cppRoots, QStringLiteral("*.cpp"));
    sources << sourcesUnder(cppRoots, QStringLiteral("*.h"));
    QVERIFY2(sources.size() > 100, qPrintable(QStringLiteral("only %1 C++ sources walked, the roots are wrong").arg(sources.size())));

    QString corpus;
    for (const QString &path : std::as_const(sources)) {
        if (generated.contains(path)) {
            continue;
        }
        corpus += withoutStringBodies(withoutComments(readAbsolute(path)));
    }

    static const QRegularExpression cmakedefine(QStringLiteral("^#cmakedefine(?:01)?\\s+(\\w+)"), QRegularExpression::MultilineOption);
    for (const QString &tmpl : templates) {
        QRegularExpressionMatchIterator it = cmakedefine.globalMatch(readAbsolute(tmpl));
        int names = 0;
        while (it.hasNext()) {
            const QString name = it.next().captured(1);
            ++names;
            QVERIFY2(corpus.contains(QRegularExpression(QStringLiteral("\\b%1\\b").arg(name))),
                     qPrintable(QStringLiteral("%1 configures %2 and no C++ source reads it").arg(relativeToRepo(tmpl), name)));
        }
        QVERIFY2(names > 0, qPrintable(QStringLiteral("%1 parsed as having no #cmakedefine at all").arg(relativeToRepo(tmpl))));
    }
}

void SourceGuardTest::qmlSignalHandlersDeclareTheirParameters()
{
    // `onFoo: { ... mouseX ... }` only sees mouseX because Qt injects the signal's parameters into
    // the handler's scope, which Qt 6 deprecated and will eventually drop. The day it goes, the
    // read does not throw -- it resolves to undefined, so the hover anchor silently pins to 0 and
    // the parabolic effect stops following the mouse. Nothing compiles or lints this: the handler
    // stays valid QML either way. So require the arrow/function form, which names its parameters.
    //
    // Only the brace-bodied shape is injection-prone. An expression handler like
    // `onPressedChanged: button.pressedChanged(pressed)` reads its own object's property and is
    // not what this is after.
    const QStringList shippedQml = qmlSourcesUnder({QStringLiteral("containment"),
                                                    QStringLiteral("plasmoid"),
                                                    QStringLiteral("declarativeimports"),
                                                    QStringLiteral("shell"),
                                                    QStringLiteral("indicators")});
    QVERIFY2(shippedQml.size() > 100, "found suspiciously few QML sources to scan");

    static const QRegularExpression signalDecl(QStringLiteral("^\\s*signal\\s+([A-Za-z_]\\w*)\\s*\\(\\s*[^)\\s][^)]*\\)"),
                                               QRegularExpression::MultilineOption);

    QStringList injected;
    for (const QString &abs : shippedQml) {
        const QString src = withoutComments(readAbsolute(abs));
        QSet<QString> handlers;
        QRegularExpressionMatchIterator it = signalDecl.globalMatch(src);
        while (it.hasNext()) {
            const QString name = it.next().captured(1);
            handlers.insert(QStringLiteral("on%1%2").arg(name.left(1).toUpper(), name.mid(1)));
        }
        if (handlers.isEmpty()) {
            continue;
        }

        const QStringList lines = src.split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i) {
            for (const QString &handler : std::as_const(handlers)) {
                if (lines.at(i).contains(QRegularExpression(QStringLiteral("^\\s*%1\\s*:\\s*\\{\\s*$").arg(handler)))) {
                    injected << QStringLiteral("%1:%2  %3").arg(relativeToRepo(abs)).arg(i + 1).arg(handler);
                }
            }
        }
    }

    QVERIFY2(injected.isEmpty(),
             qPrintable(QStringLiteral("signal handlers relying on deprecated parameter injection:\n  %1")
                            .arg(injected.join(QStringLiteral("\n  ")))));
}

void SourceGuardTest::parabolicRelaysDropTheirUnreadScales()
{
    // Both scale relays banked applyParabolicEffect's return in a `var scales` nothing ever read.
    // The trap is that the call is not a getter: it emits sglUpdateLower/HigherItemScale and IS the
    // relay, so "delete the dead assignment" reads as "delete the line" and quietly reduces the
    // parabolic effect to zooming the hovered item alone. Pin both halves -- the call stays, the
    // local does not come back.
    static const char *relays[] = {"containment/package/contents/ui/applet/ParabolicArea.qml",
                                   "declarativeimports/abilities/items/basicitem/ParabolicEventsArea.qml"};

    for (const char *relay : relays) {
        const QString rel = QString::fromUtf8(relay);
        const QString src = withoutComments(readFile(rel));
        QVERIFY2(!src.isEmpty(), qPrintable(QStringLiteral("%1 unreadable").arg(rel)));

        QVERIFY2(src.contains(QRegularExpression(QStringLiteral("\\bapplyParabolicEffect\\s*\\("))),
                 qPrintable(QStringLiteral("%1 no longer relays through applyParabolicEffect").arg(rel)));
        QVERIFY2(!src.contains(QRegularExpression(QStringLiteral("[^=!<>]=\\s*[^=;\\n]*\\bapplyParabolicEffect\\s*\\("))),
                 qPrintable(QStringLiteral("%1 assigns applyParabolicEffect's return to a local nothing reads").arg(rel)));
    }
}

void SourceGuardTest::latteQmlModulesShipNoUnreachableFiles()
{
    // A .qml inside an installed QML module has exactly two ways in: a qmldir export, which makes it
    // a type any third-party indicator can import, or an instantiation by name from other QML. The
    // module is installed with install(DIRECTORY), so no build file names the individual files and
    // nothing notices when the last reader of one goes away -- three files under components/private/
    // were reachable only from commented-out Plasma 5 code and still shipped to every user's import
    // path, reading like live code the whole time.
    const QStringList packages = {QStringLiteral("containment"),
                                  QStringLiteral("plasmoid"),
                                  QStringLiteral("declarativeimports"),
                                  QStringLiteral("shell"),
                                  QStringLiteral("indicators")};

    const QStringList sources = qmlSourcesUnder(packages);
    QVERIFY2(sources.size() > 50, qPrintable(QStringLiteral("only %1 QML files walked, the roots are wrong").arg(sources.size())));

    // A name surviving only in a comment is not a reader: every ref below is checked against the
    // comment-free body, which is the whole reason the dead private/ files looked used.
    QHash<QString, QString> readable;
    for (const QString &path : sources) {
        readable.insert(path, withoutComments(readAbsolute(path)));
    }

    QStringList unreachable;
    int checked = 0;

    for (const QString &path : sources) {
        // Only declarativeimports/ installs as QML modules. A package file under shell/ or
        // containment/ is reached by path from its own package, so the rule does not apply.
        if (!path.contains(QStringLiteral("/declarativeimports/"))) {
            continue;
        }
        ++checked;

        const QFileInfo info(path);

        bool exported = false;
        const QStringList qmldirLines = readAbsolute(info.absolutePath() + QStringLiteral("/qmldir")).split(QLatin1Char('\n'));
        for (const QString &line : qmldirLines) {
            if (line.simplified().split(QLatin1Char(' ')).contains(info.fileName())) {
                exported = true;
                break;
            }
        }
        if (exported) {
            continue;
        }

        // Base names are unique among the files that are not exported, so a whole-word match tells
        // "somebody instantiates this" from "nobody does" without parsing QML.
        const QRegularExpression use(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(info.completeBaseName())));
        bool referenced = false;
        for (auto it = readable.constBegin(); it != readable.constEnd(); ++it) {
            if (it.key() == path) {
                continue;
            }
            if (it.value().contains(use)) {
                referenced = true;
                break;
            }
        }

        if (!referenced) {
            unreachable << relativeToRepo(path);
        }
    }

    QVERIFY2(checked > 40, qPrintable(QStringLiteral("only %1 module QML files checked, the roots are wrong").arg(checked)));
    unreachable.sort();
    QVERIFY2(unreachable.isEmpty(),
             qPrintable(QStringLiteral("installed QML modules ship files nothing can reach -- neither exported in their qmldir nor instantiated anywhere: %1").arg(unreachable.join(QStringLiteral(", ")))));
}

void SourceGuardTest::qmldirExportsResolveToTheirFiles()
{
    // A qmldir line is the module's public declaration of a type and the only thing that makes it
    // importable from outside this tree. Delete the .qml without the line and the installed module
    // keeps offering a type whose file is gone -- an error that surfaces in whichever third-party
    // applet imports it, never here. qmlloadcompile cannot catch it either: it enumerates the files
    // that exist, so an export pointing at nothing is invisible to it by construction.
    const QStringList qmldirs = sourcesUnder({QStringLiteral("containment"), QStringLiteral("plasmoid"), QStringLiteral("declarativeimports")},
                                             QStringLiteral("qmldir"));
    QVERIFY2(qmldirs.size() >= 8, qPrintable(QStringLiteral("only %1 qmldir files walked, the roots are wrong").arg(qmldirs.size())));

    static const QRegularExpression exportLine(QStringLiteral("^\\s*(?:singleton\\s+)?\\w+\\s+[\\d.]+\\s+(\\S+\\.qml)\\s*$"));

    QStringList dangling;
    int exports = 0;

    for (const QString &path : qmldirs) {
        const QString dir = QFileInfo(path).absolutePath();
        const QStringList lines = readAbsolute(path).split(QLatin1Char('\n'));

        for (const QString &line : lines) {
            const QRegularExpressionMatch m = exportLine.match(line);
            if (!m.hasMatch()) {
                continue;
            }
            ++exports;

            if (!QFileInfo::exists(QStringLiteral("%1/%2").arg(dir, m.captured(1)))) {
                dangling << QStringLiteral("%1 exports a missing %2").arg(relativeToRepo(path), m.captured(1));
            }
        }
    }

    QVERIFY2(exports > 50, qPrintable(QStringLiteral("only %1 qmldir exports parsed, the line pattern is wrong").arg(exports)));
    QVERIFY2(dangling.isEmpty(), qPrintable(dangling.join(QStringLiteral("; "))));
}

void SourceGuardTest::comboBoxDropsItsDeadMobileTextMachinery()
{
    const QString raw = readFile(QStringLiteral("declarativeimports/components/ComboBox.qml"));
    QVERIFY2(!raw.isEmpty(), "ComboBox.qml not found");

    // The editable/tablet-mode TextField path was commented out well before the Qt6 port and never
    // came back -- nothing in the tree sets `editable`. It kept two cursor delegates, a mobile
    // selection toolbar and an `undefinedCursor` Component alive on paper, all of them named only
    // from inside the comment, plus a `theme.buttonTextColor` that stopped existing in Plasma 6.
    const QStringList dead = {QStringLiteral("MobileCursor"),
                              QStringLiteral("MobileTextActionsToolBar"),
                              QStringLiteral("undefinedCursor"),
                              QStringLiteral("T.TextField"),
                              QStringLiteral("theme.buttonTextColor"),
                              QStringLiteral("console.log"),
                              // Screen.devicePixelRatio inside that block was the only thing this import was for.
                              QStringLiteral("import QtQuick.Window")};
    for (const QString &name : dead) {
        QVERIFY2(!raw.contains(name), qPrintable(QStringLiteral("ComboBox.qml still carries the dead %1").arg(name)));
    }

    // The trap: TextFieldFocus reads exactly as dead as the rest -- it is only ever visible when
    // `editable` is set, which nothing does -- but the background's opacity binding reads its id, so
    // removing it turns that binding into a ReferenceError and the transparent button stops hiding.
    const QString live = stripped(withoutComments(raw));
    QVERIFY2(live.contains(QStringLiteral("Private.TextFieldFocus{id:textFieldPrivate")),
             "ComboBox.qml must keep Private.TextFieldFocus: its id is read by the background opacity binding");
    QVERIFY2(live.contains(QStringLiteral("textFieldPrivate.state!==\"hover\"")),
             "the background opacity binding must still read textFieldPrivate.state");
    QVERIFY2(live.contains(QStringLiteral("Private.ButtonShadow{")),
             "ComboBox.qml must keep the live Private.ButtonShadow");

    // ItemDelegate's only tie to private/ was a commented-out background line. The import has to go
    // with it, or the file keeps importing a directory it no longer takes anything from.
    const QString delegate = readFile(QStringLiteral("declarativeimports/components/ItemDelegate.qml"));
    QVERIFY2(!delegate.isEmpty(), "ItemDelegate.qml not found");
    QVERIFY2(!delegate.contains(QStringLiteral("DefaultListItemBackground")),
             "ItemDelegate.qml still names the deleted DefaultListItemBackground");
    QVERIFY2(!delegate.contains(QStringLiteral("import \"private\"")),
             "ItemDelegate.qml imports private/ but instantiates nothing from it");

    // Slider is the other live consumer of private/, and the reason the directory has to survive.
    QVERIFY2(withoutComments(readFile(QStringLiteral("declarativeimports/components/Slider.qml"))).contains(QStringLiteral("Private.RoundShadow")),
             "Slider.qml must keep the live Private.RoundShadow");
}

QTEST_GUILESS_MAIN(SourceGuardTest)

#include "sourceguardtest.moc"
