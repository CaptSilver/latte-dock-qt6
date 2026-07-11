/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-link unit test for Latte::Layouts::Storage. storage.cpp pulls in
// Corona/View/Importer/Manager through its includes, so rather than recompile
// half the app this links the prebuilt latte-dock application objects (minus
// main.cpp) the same way universalsettingstest does, and drives the singleton
// Storage::self() against temp .latte KConfig files. Every method exercised
// here is one that operates on a file path or a KConfigGroup without needing a
// live Corona: the containment/applet enumeration, the view() deserializer,
// updateView() serializer, subcontainment detection, clone handling and the
// null-corona screen-id branch. storageroundtriptest.cpp MIRRORS this logic
// over hand-rolled KConfig; this test runs the SAME assertions through the real
// compiled Storage so a divergence between the mirror and production surfaces.

#include "layouts/storage.h"
#include "layout/centrallayout.h"
#include "data/appletdata.h"
#include "data/viewdata.h"
#include "data/viewstable.h"
#include "data/generictable.h"
#include "data/genericdata.h"

#include <coretypes.h>

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>

using Latte::Layouts::Storage;

class StorageTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    // Writes a layout file with one Latte containment (id 1) carrying a
    // plasmoid applet (id 2) and a systray applet (id 3) whose Configuration
    // points at subcontainment 99, plus a non-Latte containment (id 5) and the
    // subcontainment itself (id 99). Returns the file path.
    QString writeLayout(const QString &name)
    {
        const QString path = m_dir.filePath(name);
        KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
        KConfigGroup conts(ptr, QStringLiteral("Containments"));

        KConfigGroup c1 = conts.group(QStringLiteral("1"));
        c1.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
        c1.writeEntry(QStringLiteral("name"), QStringLiteral("My Dock"));
        c1.writeEntry(QStringLiteral("location"), 6); // Plasma::Types::LeftEdge
        c1.writeEntry(QStringLiteral("onPrimary"), false);
        c1.writeEntry(QStringLiteral("lastScreen"), 12);
        c1.writeEntry(QStringLiteral("screensGroup"), 2); // AllSecondaryScreensGroup
        c1.writeEntry(QStringLiteral("isClonedFrom"), 5);
        c1.group(QStringLiteral("General")).writeEntry(QStringLiteral("maxLength"), (float)80.0);
        c1.group(QStringLiteral("General")).writeEntry(QStringLiteral("alignment"), 3);
        c1.group(QStringLiteral("General")).writeEntry(QStringLiteral("screenEdgeMargin"), 7);

        KConfigGroup applets = c1.group(QStringLiteral("Applets"));
        KConfigGroup a2 = applets.group(QStringLiteral("2"));
        a2.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.plasmoid"));
        KConfigGroup a3 = applets.group(QStringLiteral("3"));
        a3.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.plasma.private.systemtray"));
        a3.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("SystrayContainmentId"), 99);

        KConfigGroup c5 = conts.group(QStringLiteral("5"));
        c5.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.desktopcontainment"));

        KConfigGroup c99 = conts.group(QStringLiteral("99"));
        c99.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.plasma.private.systemtray"));

        ptr->sync();
        return path;
    }

private Q_SLOTS:
    void initTestCase();
    void validityHelpers();
    void isLatteContainmentReadsPlugin();
    void subContainmentIdAndDetection();
    void appletGroupIsValidRejectsPreloadShell();
    void viewDeserializesContainmentGroup();
    void viewRejectsNonLatteContainment();
    void updateViewRoundTripsThroughKConfig();
    void viewsEnumeratesOnlyLatteContainments();
    void subcontainmentsFromGroup();
    void containsViewAndHasContainment();
    void clonedViewDetection();
    void removeContainmentDeletesGroup();
    void removeViewDropsViewAndSubs();
    void expectedViewScreenIdNullCorona();
    void errorsReportDuplicateAppletIds();
    void warningsReportOrphanedSubcontainment();
    void exportTemplateStripsUnapprovedApplets();
    void pluginsFromFileListsAppletMetadata();
    void importContainmentsCopiesGroups();
    void metadataFallsBackToPluginId();
    void removeAllClonedViewsDropsClones();
    void newUniqueIdsFileRemapsInactiveLayout();
    void storedViewInactiveWritesTempFile();
    void viewsFromInactiveLayoutDelegatesToFile();
};

void StorageTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    QVERIFY(Storage::self() != nullptr);
}

void StorageTest::validityHelpers()
{
    QCOMPARE(Storage::IDNULL, -1);
    QCOMPARE(Storage::IDBASE, 0);
    QVERIFY(!Storage::isValid(-1));
    QVERIFY(Storage::isValid(0));
    QVERIFY(Storage::isValid(99));
}

void StorageTest::isLatteContainmentReadsPlugin()
{
    KConfig cfg(m_dir.filePath(QStringLiteral("plugincheck.latte")));
    KConfigGroup latte = cfg.group(QStringLiteral("latte"));
    latte.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
    QVERIFY(Storage::self()->isLatteContainment(latte));

    KConfigGroup other = cfg.group(QStringLiteral("other"));
    other.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.plasma.desktop"));
    QVERIFY(!Storage::self()->isLatteContainment(other));

    KConfigGroup empty = cfg.group(QStringLiteral("empty"));
    QVERIFY(!Storage::self()->isLatteContainment(empty));
}

void StorageTest::subContainmentIdAndDetection()
{
    const QString path = writeLayout(QStringLiteral("subs.latte"));
    KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
    KConfigGroup applets = KConfigGroup(ptr, QStringLiteral("Containments")).group(QStringLiteral("1")).group(QStringLiteral("Applets"));

    // The systray applet declares SystrayContainmentId 99.
    KConfigGroup systray = applets.group(QStringLiteral("3"));
    QCOMPARE(Storage::self()->subContainmentId(systray), 99);

    // A plain plasmoid is not a subcontainment.
    KConfigGroup plasmoid = applets.group(QStringLiteral("2"));
    QCOMPARE(Storage::self()->subContainmentId(plasmoid), Storage::IDNULL);

    // ContainmentId is the second recognised identity.
    KConfig cfg(m_dir.filePath(QStringLiteral("groupapplet.latte")));
    KConfigGroup grp = cfg.group(QStringLiteral("g"));
    grp.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("ContainmentId"), 42);
    QCOMPARE(Storage::self()->subContainmentId(grp), 42);
}

void StorageTest::appletGroupIsValidRejectsPreloadShell()
{
    KConfig cfg(m_dir.filePath(QStringLiteral("validity.latte")));

    // No own keys, only [Configuration]PreloadWeight -> removed-applet shell.
    KConfigGroup shell = cfg.group(QStringLiteral("shell"));
    shell.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("PreloadWeight"), 42);
    QVERIFY(!Storage::appletGroupIsValid(shell));

    // A real applet has a plugin key.
    KConfigGroup real = cfg.group(QStringLiteral("real"));
    real.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.plasmoid"));
    real.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("PreloadWeight"), 42);
    QVERIFY(Storage::appletGroupIsValid(real));
}

void StorageTest::viewDeserializesContainmentGroup()
{
    const QString path = writeLayout(QStringLiteral("viewread.latte"));
    KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
    KConfigGroup c1 = KConfigGroup(ptr, QStringLiteral("Containments")).group(QStringLiteral("1"));

    Latte::Data::View v = Storage::self()->view(c1);

    QVERIFY(v.isValid());
    QCOMPARE(v.id, QStringLiteral("1"));
    QCOMPARE(v.name, QStringLiteral("My Dock"));
    QCOMPARE(v.onPrimary, false);
    QCOMPARE(v.screen, 12);
    QCOMPARE(v.isClonedFrom, 5);
    QCOMPARE(v.screenEdgeMargin, 7);
    QCOMPARE((int)v.screensGroup, (int)Latte::Types::AllSecondaryScreensGroup);
    QCOMPARE((int)v.edge, 6); // Plasma::Types::LeftEdge
    QCOMPARE(v.maxLength, (float)80.0);
    QCOMPARE((int)v.alignment, 3);

    // The systray applet under this view is reported as a subcontainment.
    QCOMPARE(v.subcontainments.rowCount(), 1);
    QCOMPARE(v.subcontainments[(uint)0].id, QStringLiteral("99"));
}

void StorageTest::viewRejectsNonLatteContainment()
{
    KConfig cfg(m_dir.filePath(QStringLiteral("nonlatte.latte")));
    KConfigGroup g = cfg.group(QStringLiteral("Containments")).group(QStringLiteral("5"));
    g.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.desktopcontainment"));
    g.writeEntry(QStringLiteral("name"), QStringLiteral("desktop"));

    Latte::Data::View v = Storage::self()->view(g);
    QVERIFY(!v.isValid());
    QVERIFY(v.name.isEmpty());
}

void StorageTest::updateViewRoundTripsThroughKConfig()
{
    const QString path = m_dir.filePath(QStringLiteral("update.latte"));
    {
        KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
        KConfigGroup g = KConfigGroup(ptr, QStringLiteral("Containments")).group(QStringLiteral("7"));
        g.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
        g.sync();

        Latte::Data::View nv;
        nv.name = QStringLiteral("Written");
        nv.screensGroup = Latte::Types::AllScreensGroup;
        nv.onPrimary = false;
        nv.isClonedFrom = 4;
        nv.screen = 13;
        nv.screenEdgeMargin = 9;
        nv.edge = Plasma::Types::TopEdge;
        nv.maxLength = (float)55.0;
        nv.alignment = Latte::Types::Justify;
        Storage::self()->updateView(g, nv);
    }

    // Read back from a fresh handle: a real on-disk round-trip through view().
    KConfig fresh(path);
    KConfigGroup g = fresh.group(QStringLiteral("Containments")).group(QStringLiteral("7"));
    Latte::Data::View r = Storage::self()->view(g);

    QCOMPARE(r.name, QStringLiteral("Written"));
    QCOMPARE((int)r.screensGroup, (int)Latte::Types::AllScreensGroup);
    QCOMPARE(r.onPrimary, false);
    QCOMPARE(r.isClonedFrom, 4);
    QCOMPARE(r.screen, 13);
    QCOMPARE(r.screenEdgeMargin, 9);
    QCOMPARE((int)r.edge, (int)Plasma::Types::TopEdge);
    QCOMPARE(r.maxLength, (float)55.0);
    QCOMPARE((int)r.alignment, (int)Latte::Types::Justify);

    // maxLength must serialize under [General], not at the containment level.
    QCOMPARE(g.readEntry(QStringLiteral("maxLength"), (float)-1.0), (float)-1.0);
    QCOMPARE(g.group(QStringLiteral("General")).readEntry(QStringLiteral("maxLength"), (float)-1.0), (float)55.0);
}

void StorageTest::viewsEnumeratesOnlyLatteContainments()
{
    const QString path = writeLayout(QStringLiteral("viewsfile.latte"));
    Latte::Data::ViewsTable table = Storage::self()->views(path);

    // Only containment 1 is org.kde.latte.containment; 5 and 99 are not views.
    QCOMPARE(table.rowCount(), 1);
    QVERIFY(table.containsId(QStringLiteral("1")));
    QVERIFY(!table.containsId(QStringLiteral("5")));
    QVERIFY(!table.containsId(QStringLiteral("99")));
    QCOMPARE(table[(uint)0].name, QStringLiteral("My Dock"));

    // 99 is reachable only as view 1's subcontainment, not as its own view.
    QVERIFY(table.hasContainmentId(QStringLiteral("1")));
    QVERIFY(table.hasContainmentId(QStringLiteral("99")));
    QVERIFY(!table.hasContainmentId(QStringLiteral("5")));
}

void StorageTest::subcontainmentsFromGroup()
{
    const QString path = writeLayout(QStringLiteral("subsfromgroup.latte"));
    KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
    KConfigGroup c1 = KConfigGroup(ptr, QStringLiteral("Containments")).group(QStringLiteral("1"));

    Latte::Data::GenericTable<Latte::Data::Generic> subs = Storage::self()->subcontainments(c1);
    QCOMPARE(subs.rowCount(), 1);
    QCOMPARE(subs[(uint)0].id, QStringLiteral("99"));

    // A non-Latte containment group yields no subcontainments.
    KConfigGroup c5 = KConfigGroup(ptr, QStringLiteral("Containments")).group(QStringLiteral("5"));
    QCOMPARE(Storage::self()->subcontainments(c5).rowCount(), 0);
}

void StorageTest::containsViewAndHasContainment()
{
    const QString path = writeLayout(QStringLiteral("contains.latte"));

    // containsView is true only for an existing Latte containment id.
    QVERIFY(Storage::self()->containsView(path, 1));
    QVERIFY(!Storage::self()->containsView(path, 5));   // exists but not Latte
    QVERIFY(!Storage::self()->containsView(path, 99));  // exists but not Latte
    QVERIFY(!Storage::self()->containsView(path, 1234)); // missing
}

void StorageTest::clonedViewDetection()
{
    KConfig cfg(m_dir.filePath(QStringLiteral("clones.latte")));

    KConfigGroup cloned = cfg.group(QStringLiteral("Containments")).group(QStringLiteral("10"));
    cloned.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
    cloned.writeEntry(QStringLiteral("isClonedFrom"), 3);
    QVERIFY(Storage::self()->isClonedView(cloned));

    KConfigGroup notcloned = cfg.group(QStringLiteral("Containments")).group(QStringLiteral("11"));
    notcloned.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
    QVERIFY(!Storage::self()->isClonedView(notcloned)); // default isClonedFrom == ISCLONEDNULL

    // A non-Latte containment is never a cloned view even with isClonedFrom set.
    KConfigGroup nonlatte = cfg.group(QStringLiteral("Containments")).group(QStringLiteral("12"));
    nonlatte.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.desktopcontainment"));
    nonlatte.writeEntry(QStringLiteral("isClonedFrom"), 3);
    QVERIFY(!Storage::self()->isClonedView(nonlatte));
}

void StorageTest::removeContainmentDeletesGroup()
{
    const QString path = writeLayout(QStringLiteral("removecont.latte"));

    Storage::self()->removeContainment(path, QStringLiteral("5"));

    KConfig fresh(path);
    KConfigGroup conts = fresh.group(QStringLiteral("Containments"));
    QVERIFY(!conts.hasGroup(QStringLiteral("5")));
    QVERIFY(conts.hasGroup(QStringLiteral("1")));

    // An empty id is a no-op and must not throw or wipe the file.
    Storage::self()->removeContainment(path, QString());
    QVERIFY(KConfig(path).group(QStringLiteral("Containments")).hasGroup(QStringLiteral("1")));
}

void StorageTest::removeViewDropsViewAndSubs()
{
    const QString path = writeLayout(QStringLiteral("removeview.latte"));

    // Build the view (carrying its subcontainment 99) and remove it; both the
    // view containment and its subcontainment group must be gone.
    KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
    KConfigGroup c1 = KConfigGroup(ptr, QStringLiteral("Containments")).group(QStringLiteral("1"));
    Latte::Data::View v = Storage::self()->view(c1);
    QCOMPARE(v.subcontainments.rowCount(), 1);

    Storage::self()->removeView(path, v);

    KConfig fresh(path);
    KConfigGroup conts = fresh.group(QStringLiteral("Containments"));
    QVERIFY(!conts.hasGroup(QStringLiteral("1")));
    QVERIFY(!conts.hasGroup(QStringLiteral("99")));
    QVERIFY(conts.hasGroup(QStringLiteral("5"))); // untouched

    // An invalid view is a no-op.
    Latte::Data::View invalid;
    Storage::self()->removeView(path, invalid);
    QVERIFY(KConfig(path).group(QStringLiteral("Containments")).hasGroup(QStringLiteral("5")));
}

void StorageTest::expectedViewScreenIdNullCorona()
{
    // The Corona overload short-circuits to NOSCREENID when corona is null,
    // regardless of the view payload.
    Latte::Data::View v;
    v.setState(Latte::Data::View::IsCreated);
    v.onPrimary = true;
    QCOMPARE(Storage::self()->expectedViewScreenId((Latte::Corona *)nullptr, v),
             Latte::ScreenPool::NOSCREENID);
}

void StorageTest::errorsReportDuplicateAppletIds()
{
    // Two Latte containments each carry applet id "7" -> APPLETSWITHSAMEID error.
    const QString path = m_dir.filePath(QStringLiteral("dupapplets.latte"));
    {
        KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
        KConfigGroup conts(ptr, QStringLiteral("Containments"));
        for (const QString &cid : {QStringLiteral("1"), QStringLiteral("2")}) {
            KConfigGroup c = conts.group(cid);
            c.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
            KConfigGroup a = c.group(QStringLiteral("Applets")).group(QStringLiteral("7"));
            a.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.plasmoid"));
        }
        ptr->sync();
    }

    Latte::CentralLayout layout(nullptr, path, QStringLiteral("dupapplets"));
    QVERIFY(!layout.isActive()); // no corona -> inactive branch

    Latte::Data::ErrorsList errs = Storage::self()->errors(&layout);

    bool sawSameId = false;
    for (const auto &e : errs) {
        if (e.id == QString(QLatin1String(Latte::Data::Error::APPLETSWITHSAMEID))) {
            sawSameId = true;
            QCOMPARE(e.information.rowCount(), 2); // both occurrences of "7"
        }
    }
    QVERIFY(sawSameId);
}

void StorageTest::warningsReportOrphanedSubcontainment()
{
    // A non-Latte containment "5" reachable from no view -> ORPHANEDSUBCONTAINMENT.
    const QString path = m_dir.filePath(QStringLiteral("orphansub.latte"));
    {
        KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
        KConfigGroup conts(ptr, QStringLiteral("Containments"));

        KConfigGroup c1 = conts.group(QStringLiteral("1"));
        c1.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));

        KConfigGroup c5 = conts.group(QStringLiteral("5"));
        c5.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.plasma.private.systemtray"));
        ptr->sync();
    }

    Latte::CentralLayout layout(nullptr, path, QStringLiteral("orphansub"));
    QVERIFY(!layout.isActive());

    Latte::Data::WarningsList warns = Storage::self()->warnings(&layout);

    bool sawOrphan = false;
    for (const auto &w : warns) {
        if (w.id == QString(QLatin1String(Latte::Data::Warning::ORPHANEDSUBCONTAINMENT))) {
            sawOrphan = true;
        }
    }
    QVERIFY(sawOrphan);
}

void StorageTest::exportTemplateStripsUnapprovedApplets()
{
    // writeLayout marks containment 1 with isClonedFrom=5, so exportTemplate's
    // removeAllClonedViews pass would remove it. Use a dedicated non-clone fixture.
    // Both applets carry a [Configuration] subgroup so the strip is observable:
    // exportTemplate deletes the config subgroups of UNAPPROVED, non-subcontainment
    // applets, leaving the applet's own plugin key and the approved applet intact.
    const QString origin = m_dir.filePath(QStringLiteral("exportsrc.latte"));
    {
        KSharedConfigPtr ptr = KSharedConfig::openConfig(origin);
        KConfigGroup conts(ptr, QStringLiteral("Containments"));
        KConfigGroup c1 = conts.group(QStringLiteral("1"));
        c1.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
        c1.writeEntry(QStringLiteral("layoutId"), QStringLiteral("SomeLayout"));
        KConfigGroup applets = c1.group(QStringLiteral("Applets"));

        KConfigGroup a2 = applets.group(QStringLiteral("2"));
        a2.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.plasmoid"));
        a2.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("keep"), QStringLiteral("yes"));

        KConfigGroup a3 = applets.group(QStringLiteral("3"));
        a3.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.plasma.private.systemtray"));
        a3.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("gone"), QStringLiteral("soon"));
        ptr->sync();
    }
    const QString dest = m_dir.filePath(QStringLiteral("exported.latte"));

    // Approve only the plasmoid (applet "2"); the systray applet "3" is not approved.
    Latte::Data::AppletsTable approved;
    Latte::Data::Applet a;
    a.id = QStringLiteral("org.kde.latte.plasmoid");
    approved << a;

    QVERIFY(Storage::self()->exportTemplate(origin, dest, approved));
    QVERIFY(QFile(dest).exists());

    KConfig cfg(dest);
    KConfigGroup exportedApplets =
        cfg.group(QStringLiteral("Containments")).group(QStringLiteral("1")).group(QStringLiteral("Applets"));

    // Unapproved applet 3: its configuration subgroup is stripped, but the applet's
    // own plugin key survives (exportTemplate deletes config subgroups, not the applet).
    QVERIFY(exportedApplets.group(QStringLiteral("3")).groupList().isEmpty());
    QCOMPARE(exportedApplets.group(QStringLiteral("3")).readEntry(QStringLiteral("plugin"), QString()),
             QStringLiteral("org.kde.plasma.private.systemtray"));

    // Approved applet 2: its configuration subgroup and plugin survive untouched.
    QVERIFY(exportedApplets.group(QStringLiteral("2")).hasGroup(QStringLiteral("Configuration")));
    QCOMPARE(exportedApplets.group(QStringLiteral("2")).group(QStringLiteral("Configuration")).readEntry(QStringLiteral("keep"), QString()),
             QStringLiteral("yes"));

    // layoutId is cleared on every containment.
    QCOMPARE(cfg.group(QStringLiteral("Containments")).group(QStringLiteral("1")).readEntry(QStringLiteral("layoutId"), QStringLiteral("x")),
             QString());
}

void StorageTest::pluginsFromFileListsAppletMetadata()
{
    const QString path = writeLayout(QStringLiteral("pluginsfile.latte"));
    // -1 (IDNULL) means "all containments": every applet plugin id is gathered.
    Latte::Data::AppletsTable table = Storage::self()->plugins(path, -1);
    QVERIFY(table.rowCount() >= 1);
}

void StorageTest::importContainmentsCopiesGroups()
{
    const QString origin = writeLayout(QStringLiteral("impsrc.latte"));
    const QString dest = m_dir.filePath(QStringLiteral("impdst.latte"));

    Storage::self()->importContainments(origin, dest);

    KConfig cfg(dest);
    KConfigGroup conts = cfg.group(QStringLiteral("Containments"));
    QVERIFY(conts.hasGroup(QStringLiteral("1")));
    QVERIFY(conts.hasGroup(QStringLiteral("5")));

    // Empty paths are a no-op and must not throw.
    Storage::self()->importContainments(QString(), dest);
}

void StorageTest::metadataFallsBackToPluginId()
{
    // An unknown plugin id yields data whose name is the id itself.
    Latte::Data::Applet data = Storage::self()->metadata(QStringLiteral("org.kde.nonexistent.applet.xyz"));
    QCOMPARE(data.id, QStringLiteral("org.kde.nonexistent.applet.xyz"));
    QCOMPARE(data.name, QStringLiteral("org.kde.nonexistent.applet.xyz"));
}

void StorageTest::removeAllClonedViewsDropsClones()
{
    const QString path = m_dir.filePath(QStringLiteral("clonesremove.latte"));
    {
        KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
        KConfigGroup conts(ptr, QStringLiteral("Containments"));
        KConfigGroup c1 = conts.group(QStringLiteral("1"));
        c1.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
        KConfigGroup c10 = conts.group(QStringLiteral("10"));
        c10.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
        c10.writeEntry(QStringLiteral("isClonedFrom"), 1);
        ptr->sync();
    }

    Storage::self()->removeAllClonedViews(path);

    KConfig fresh(path);
    KConfigGroup conts = fresh.group(QStringLiteral("Containments"));
    QVERIFY(conts.hasGroup(QStringLiteral("1")));   // original kept
    QVERIFY(!conts.hasGroup(QStringLiteral("10"))); // clone removed
}

void StorageTest::newUniqueIdsFileRemapsInactiveLayout()
{
    // Destination inactive layout already owns id "1"; the origin template also uses "1".
    // newView() calls newUniqueIdsFile() internally and imports the remapped containments
    // into the destination file. The returned view must carry a fresh id (not "1"),
    // and the destination file must now contain a second Latte containment with that id.
    const QString destPath = writeLayout(QStringLiteral("dest.latte"));
    Latte::CentralLayout dest(nullptr, destPath, QStringLiteral("dest"));
    QVERIFY(!dest.isActive());

    const QString originPath = writeLayout(QStringLiteral("origin.latte"));

    Latte::Data::View nextViewData;
    nextViewData.setState(Latte::Data::View::IsCreated, originPath);

    Latte::Data::View added = Storage::self()->newView(&dest, nextViewData);
    QVERIFY(added.isValid());
    QVERIFY(!added.id.isEmpty());
    QVERIFY(added.id != QStringLiteral("1")); // remapped away from the collision

    // The destination file now holds two Latte views (the original "1" and the remapped one).
    Latte::Data::ViewsTable table = Storage::self()->views(destPath);
    QCOMPARE(table.rowCount(), 2);
    QVERIFY(table.containsId(QStringLiteral("1")));
    QVERIFY(table.containsId(added.id));
}

void StorageTest::storedViewInactiveWritesTempFile()
{
    const QString path = writeLayout(QStringLiteral("storedview.latte"));
    Latte::CentralLayout layout(nullptr, path, QStringLiteral("storedview"));
    QVERIFY(!layout.isActive());

    const QString stored = Storage::self()->storedView(&layout, 1);
    QVERIFY(!stored.isEmpty());
    QVERIFY(QFile(stored).exists());

    // The stored file carries the view containment (1) and its subcontainment (99).
    KConfig cfg(stored);
    KConfigGroup conts = cfg.group(QStringLiteral("Containments"));
    QVERIFY(conts.hasGroup(QStringLiteral("1")));
    QVERIFY(conts.hasGroup(QStringLiteral("99")));

    // A non-existent containment id yields an empty path.
    QVERIFY(Storage::self()->storedView(&layout, 4242).isEmpty());
}

void StorageTest::viewsFromInactiveLayoutDelegatesToFile()
{
    const QString path = writeLayout(QStringLiteral("viewslayout.latte"));
    Latte::CentralLayout layout(nullptr, path, QStringLiteral("viewslayout"));
    QVERIFY(!layout.isActive());

    Latte::Data::ViewsTable table = Storage::self()->views(&layout);
    QCOMPARE(table.rowCount(), 1);
    QVERIFY(table.containsId(QStringLiteral("1")));
}

QTEST_MAIN(StorageTest)

#include "storagetest.moc"
