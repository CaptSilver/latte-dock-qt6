/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-object behavioral test for the static parser/path helpers of
// app/layouts/importer.cpp (Latte::Layouts::Importer). The header drags in
// manager.h/lattecorona.h transitively, so importer.cpp is driven through the
// prebuilt latte-dock application objects (the glob-link), not recompiled here.
//
// This test calls the REAL compiled statics rather than re-implementing the
// suffix/extension string logic in the test itself:
//   fileVersion() (incl. the .latterc KTar archive branch a mirror can't reach),
//   nameOfConfigFile(), layoutUserDir()/layoutUserFilePath(), layoutExists(),
//   uniqueLayoutName(), availableLayouts(), systemShellDataPath(),
//   layoutTemplateSystemFilePath(), standardPaths()/standardPathsFor() ordering,
//   and the multipleLayoutsStatus() KConfig round-trip.
// The directories the statics read come from XDG_CONFIG_HOME / XDG_DATA_*, all
// pointed at throwaway temp dirs so the host config is never touched.

#include "layouts/importer.h"
#include "layout/abstractlayout.h"
#include "apptypes.h"

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KArchiveDirectory>
#include <KTar>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using Latte::Layouts::Importer;

class ImporterLogicTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_configHome; // XDG_CONFIG_HOME -> Latte::configPath()
    QTemporaryDir m_dataDir;    // XDG_DATA_DIRS    -> system data root
    QTemporaryDir m_userData;   // XDG_DATA_HOME    -> user data root

    QString configPath() const { return m_configHome.path(); }
    QString latteDir() const { return m_configHome.path() + QStringLiteral("/latte"); }

    // Write a .layout.latte KConfig with the given LayoutSettings/version.
    QString writeLayoutFile(const QString &path, int version)
    {
        KConfig c(path);
        c.group(QStringLiteral("LayoutSettings")).writeEntry(QStringLiteral("version"), version);
        c.sync();
        return path;
    }

    // Build a .latterc tar archive holding the named member files. Each pair is
    // (archive member name -> a KConfig group/key/value triplet rendered to text),
    // matching what fileVersion() peeks at: lattedockrc[UniversalSettings/version]
    // and lattedock-appletsrc[LayoutSettings/version].
    QString writeArchive(const QString &path,
                         int rcVersion,      // -1 = omit lattedockrc
                         int appletsVersion, // -1 = omit lattedock-appletsrc
                         bool withLatteDir)
    {
        KTar archive(path, QStringLiteral("application/x-tar"));
        if (!archive.open(QIODevice::WriteOnly)) {
            return QString();
        }

        if (rcVersion >= 0) {
            const QByteArray rc = QStringLiteral("[UniversalSettings]\nversion=%1\n")
                                      .arg(rcVersion)
                                      .toUtf8();
            archive.writeFile(QStringLiteral("lattedockrc"), rc);
        }
        if (appletsVersion >= 0) {
            const QByteArray applets = QStringLiteral("[LayoutSettings]\nversion=%1\n")
                                           .arg(appletsVersion)
                                           .toUtf8();
            archive.writeFile(QStringLiteral("lattedock-appletsrc"), applets);
        }
        if (withLatteDir) {
            // A directory entry; copyTo() recreates it so QDir(latte).exists() is true.
            archive.writeFile(QStringLiteral("latte/dummy"), QByteArray("x"));
        }

        archive.close();
        return path;
    }

    // Build an old-style lattedock-appletsrc: one Latte containment for the given
    // session (DefaultSession=0 / AlternativeSession=1) whose applet references a
    // systray containment, the systray containment itself, and the per-session
    // global launchers. No LayoutSettings/version, so it reads as pre-v2 and is
    // eligible for old-format import.
    QString writeOldAppletsrc(const QString &path, int session, int systrayId,
                              const QStringList &defaultLaunchers,
                              const QStringList &alternativeLaunchers)
    {
        KConfig config(path);
        KConfigGroup containments = config.group(QStringLiteral("Containments"));

        KConfigGroup latteContainment = containments.group(QStringLiteral("1"));
        latteContainment.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
        latteContainment.writeEntry(QStringLiteral("session"), session);

        KConfigGroup appletConfig = latteContainment.group(QStringLiteral("Applets"))
                                        .group(QStringLiteral("10"))
                                        .group(QStringLiteral("Configuration"));
        appletConfig.writeEntry(QStringLiteral("SystrayContainmentId"), QString::number(systrayId));

        if (systrayId != -1) {
            KConfigGroup systray = containments.group(QString::number(systrayId));
            systray.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.plasma.private.systemtray"));
        }

        KConfigGroup general = config.group(QStringLiteral("General"));
        general.writeEntry(QStringLiteral("globalLaunchers_default"), defaultLaunchers);
        general.writeEntry(QStringLiteral("globalLaunchers_alternative"), alternativeLaunchers);

        config.sync();
        return path;
    }

private Q_SLOTS:
    void initTestCase();

    void fileVersionLayoutFiles();
    void fileVersionMissingAndForeign();
    void fileVersionArchiveConfigV1();
    void fileVersionArchiveConfigV2();
    void fileVersionArchiveUnknown();
    void nameOfConfigFile_data();
    void nameOfConfigFile();
    void layoutPaths();
    void layoutExistsAndAvailable();
    void uniqueLayoutNameDedups();
    void systemPaths();
    void standardPathsOrdering();
    void multipleLayoutsStatusRoundTrip();

    void importLayoutHelperCopiesV2();
    void importLayoutHelperRejectsNonV2();
    void importLayoutHelperDedupsName();
    void availableTemplatesScanAndDedup();
    void autostartEnableDisableRoundTrip();
    void checkRepairMovesLinkedContainments();
    void importLayoutInstanceEmitsSignal();
    void storageTmpDirExists();
    void exportFullConfigurationArchivesTree();
    void importOldLayoutDefaultSession();
    void importOldLayoutAlternativeSession();
    void importOldLayoutRejectsWhenNoLatteContainment();
    void importOldConfigurationExtractsAndImports();
    void importOldConfigurationRejectsMissingAndWrongFormat();
    void importOldConfigurationDerivesNameFromArchive();
    void importHelperExtractsConfigArchive();
};

void ImporterLogicTest::initTestCase()
{
    QVERIFY(m_configHome.isValid());
    QVERIFY(m_dataDir.isValid());
    QVERIFY(m_userData.isValid());

    // configPath() reads ConfigLocation (XDG_CONFIG_HOME); layout dir lives under it.
    qputenv("XDG_CONFIG_HOME", m_configHome.path().toLocal8Bit());
    // systemShellDataPath()/standardPaths() read GenericDataLocation. The two roots must be
    // DIFFERENT dirs: point both at one path and standardPaths() answers with two identical
    // strings, which makes it its own reverse and any ordering assertion vacuous.
    qputenv("XDG_DATA_HOME", m_userData.path().toLocal8Bit());
    qputenv("XDG_DATA_DIRS", m_dataDir.path().toLocal8Bit());

    QDir(configPath()).mkpath(QStringLiteral("latte"));
}

void ImporterLogicTest::fileVersionLayoutFiles()
{
    const QString v2 = writeLayoutFile(latteDir() + QStringLiteral("/v2.layout.latte"), 2);
    const QString v1 = writeLayoutFile(latteDir() + QStringLiteral("/v1.layout.latte"), 1);

    QCOMPARE(Importer::fileVersion(v2), Importer::LayoutVersion2);
    // A .layout.latte with version != 2 is not a recognised modern layout.
    QCOMPARE(Importer::fileVersion(v1), Importer::UnknownFileType);
}

void ImporterLogicTest::fileVersionMissingAndForeign()
{
    QCOMPARE(Importer::fileVersion(latteDir() + QStringLiteral("/nope.layout.latte")),
             Importer::UnknownFileType);

    // A file that exists but is neither .layout.latte nor .latterc.
    const QString txt = configPath() + QStringLiteral("/plain.txt");
    QFile f(txt);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("hello");
    f.close();
    QCOMPARE(Importer::fileVersion(txt), Importer::UnknownFileType);

    // A .latterc path that does not exist at all.
    QCOMPARE(Importer::fileVersion(configPath() + QStringLiteral("/ghost.latterc")),
             Importer::UnknownFileType);
}

void ImporterLogicTest::fileVersionArchiveConfigV1()
{
    // version-1 rc + version-1 applets => ConfigVersion1.
    const QString arc = writeArchive(configPath() + QStringLiteral("/old.latterc"),
                                     /*rc*/ 1, /*applets*/ 1, /*latteDir*/ false);
    QVERIFY(!arc.isEmpty());
    QCOMPARE(Importer::fileVersion(arc), Importer::ConfigVersion1);
}

void ImporterLogicTest::fileVersionArchiveConfigV2()
{
    // version-2 rc + a latte/ directory => ConfigVersion2.
    const QString arc = writeArchive(configPath() + QStringLiteral("/new.latterc"),
                                     /*rc*/ 2, /*applets*/ -1, /*latteDir*/ true);
    QVERIFY(!arc.isEmpty());
    QCOMPARE(Importer::fileVersion(arc), Importer::ConfigVersion2);
}

void ImporterLogicTest::fileVersionArchiveUnknown()
{
    // version-2 rc but no latte/ dir: neither v1 nor a complete v2 => Unknown.
    const QString arc = writeArchive(configPath() + QStringLiteral("/partial.latterc"),
                                     /*rc*/ 2, /*applets*/ -1, /*latteDir*/ false);
    QVERIFY(!arc.isEmpty());
    QCOMPARE(Importer::fileVersion(arc), Importer::UnknownFileType);

    // A .latterc that is not a tar archive at all.
    const QString notar = configPath() + QStringLiteral("/garbage.latterc");
    QFile g(notar);
    QVERIFY(g.open(QIODevice::WriteOnly));
    g.write("this is not a tar archive");
    g.close();
    QCOMPARE(Importer::fileVersion(notar), Importer::UnknownFileType);
}

void ImporterLogicTest::nameOfConfigFile_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("expected");

    QTest::newRow("strips latterc") << QStringLiteral("/home/u/My Config.latterc") << QStringLiteral("My Config");
    QTest::newRow("bare latterc") << QStringLiteral("foo.latterc") << QStringLiteral("foo");
    QTest::newRow("non-latterc kept") << QStringLiteral("/p/lattedockrc") << QStringLiteral("lattedockrc");
    // The -1 lastIndexOf miss must NOT chop the trailing character (the Qt6
    // remove(-1,8) bug the chop()/endsWith() rewrite fixed).
    QTest::newRow("no extension kept") << QStringLiteral("/p/Plasma") << QStringLiteral("Plasma");
    QTest::newRow("single char kept") << QStringLiteral("/p/A") << QStringLiteral("A");
}

void ImporterLogicTest::nameOfConfigFile()
{
    QFETCH(QString, path);
    QFETCH(QString, expected);
    QCOMPARE(Importer::nameOfConfigFile(path), expected);
}

void ImporterLogicTest::layoutPaths()
{
    // layoutUserDir() == <configPath>/latte, layoutUserFilePath appends the name.
    QCOMPARE(Importer::layoutUserDir(), latteDir());
    QCOMPARE(Importer::layoutUserFilePath(QStringLiteral("My Layout")),
             latteDir() + QStringLiteral("/My Layout.layout.latte"));
}

void ImporterLogicTest::layoutExistsAndAvailable()
{
    writeLayoutFile(Importer::layoutUserFilePath(QStringLiteral("Alpha")), 2);
    writeLayoutFile(Importer::layoutUserFilePath(QStringLiteral("Beta")), 2);

    QVERIFY(Importer::layoutExists(QStringLiteral("Alpha")));
    QVERIFY(Importer::layoutExists(QStringLiteral("Beta")));
    QVERIFY(!Importer::layoutExists(QStringLiteral("Gamma")));

    const QStringList avail = Importer::availableLayouts();
    QVERIFY(avail.contains(QStringLiteral("Alpha")));
    QVERIFY(avail.contains(QStringLiteral("Beta")));
    QVERIFY(!avail.contains(QStringLiteral("Gamma")));
}

void ImporterLogicTest::uniqueLayoutNameDedups()
{
    // A name that doesn't exist is returned untouched.
    QCOMPARE(Importer::uniqueLayoutName(QStringLiteral("Fresh")), QStringLiteral("Fresh"));

    // Create "Taken", then the helper must produce "Taken - 2".
    writeLayoutFile(Importer::layoutUserFilePath(QStringLiteral("Taken")), 2);
    QCOMPARE(Importer::uniqueLayoutName(QStringLiteral("Taken")), QStringLiteral("Taken - 2"));

    // With "Taken" and "Taken - 2" present, the next free copy is "Taken - 3";
    // feeding the already-suffixed name must strip the old suffix, not stack it.
    writeLayoutFile(Importer::layoutUserFilePath(QStringLiteral("Taken - 2")), 2);
    QCOMPARE(Importer::uniqueLayoutName(QStringLiteral("Taken - 2")), QStringLiteral("Taken - 3"));
}

void ImporterLogicTest::systemPaths()
{
    const QString sysData = m_dataDir.path() + QStringLiteral("/plasma/shells/org.kde.latte.shell");
    QCOMPARE(Importer::systemShellDataPath(), sysData);
    QCOMPARE(Importer::layoutTemplateSystemFilePath(QStringLiteral("Default")),
             sysData + QStringLiteral("/contents/templates/Default.layout.latte"));
}

void ImporterLogicTest::standardPathsOrdering()
{
    const QStringList localFirst = Importer::standardPaths(true);
    const QStringList localLast = Importer::standardPaths(false);

    QVERIFY(localFirst.count() > 1);
    // Orientation, not just symmetry: a reversal assertion alone passes against a function
    // that never reverses anything. localfirst decides whether enableAutostart() finds the
    // system .desktop or a user-local override, so pin which end the user's dir lands on.
    QCOMPARE(localFirst.first(), m_userData.path());
    QCOMPARE(localLast.last(), m_userData.path());

    // localfirst=false is the exact reverse of localfirst=true.
    QStringList reversed = localFirst;
    std::reverse(reversed.begin(), reversed.end());
    QCOMPARE(localLast, reversed);

    // standardPathsFor appends the subPath to every entry, inserting a single
    // separator only when the subPath isn't already slash-prefixed.
    const QStringList withSub = Importer::standardPathsFor(QStringLiteral("latte"), true);
    QCOMPARE(withSub.count(), localFirst.count());
    for (int i = 0; i < withSub.count(); ++i) {
        QCOMPARE(withSub[i], localFirst[i] + QStringLiteral("/latte"));
    }

    const QStringList absSub = Importer::standardPathsFor(QStringLiteral("/abs"), true);
    for (int i = 0; i < absSub.count(); ++i) {
        QCOMPARE(absSub[i], localFirst[i] + QStringLiteral("/abs"));
    }
}

void ImporterLogicTest::multipleLayoutsStatusRoundTrip()
{
    // No linked file present yet => Uninitialized, and a set is a no-op.
    QCOMPARE(Importer::multipleLayoutsStatus(), Latte::MultipleLayouts::Uninitialized);
    Importer::setMultipleLayoutsStatus(Latte::MultipleLayouts::Running);
    QCOMPARE(Importer::multipleLayoutsStatus(), Latte::MultipleLayouts::Uninitialized);

    // Create the hidden linked file; now a status write round-trips through KConfig.
    const QString linked = Importer::layoutUserFilePath(
        QString::fromLatin1(Latte::Layout::MULTIPLELAYOUTSHIDDENNAME));
    writeLayoutFile(linked, 2);

    Importer::setMultipleLayoutsStatus(Latte::MultipleLayouts::Running);
    QCOMPARE(Importer::multipleLayoutsStatus(), Latte::MultipleLayouts::Running);

    Importer::setMultipleLayoutsStatus(Latte::MultipleLayouts::Paused);
    QCOMPARE(Importer::multipleLayoutsStatus(), Latte::MultipleLayouts::Paused);
}

void ImporterLogicTest::importLayoutHelperCopiesV2()
{
    // A version-2 layout file outside the latte dir is copied in under the given name.
    const QString src = writeLayoutFile(configPath() + QStringLiteral("/import-source.layout.latte"), 2);
    const QString name = Importer::importLayoutHelper(src, QStringLiteral("Imported"));
    QCOMPARE(name, QStringLiteral("Imported"));
    QVERIFY(Importer::layoutExists(QStringLiteral("Imported")));
    QCOMPARE(Importer::fileVersion(Importer::layoutUserFilePath(QStringLiteral("Imported"))), Importer::LayoutVersion2);
}

void ImporterLogicTest::importLayoutHelperRejectsNonV2()
{
    // A version-1 file is not a modern layout, so nothing is imported.
    const QString bad = writeLayoutFile(configPath() + QStringLiteral("/bad-source.layout.latte"), 1);
    QVERIFY(Importer::importLayoutHelper(bad, QStringLiteral("Bad")).isEmpty());
    QVERIFY(!Importer::layoutExists(QStringLiteral("Bad")));
}

void ImporterLogicTest::importLayoutHelperDedupsName()
{
    // Importing the same source twice yields a "- 2" copy; with no suggested name the
    // source file's base name is used.
    const QString src = writeLayoutFile(configPath() + QStringLiteral("/Dup.layout.latte"), 2);
    QCOMPARE(Importer::importLayoutHelper(src), QStringLiteral("Dup"));
    QCOMPARE(Importer::importLayoutHelper(src), QStringLiteral("Dup - 2"));
    QVERIFY(Importer::layoutExists(QStringLiteral("Dup")));
    QVERIFY(Importer::layoutExists(QStringLiteral("Dup - 2")));
}

void ImporterLogicTest::availableTemplatesScanAndDedup()
{
    const QString userTemplates = latteDir() + QStringLiteral("/templates");
    QVERIFY(QDir().mkpath(userTemplates));
    writeLayoutFile(userTemplates + QStringLiteral("/UserView.view.latte"), 2);
    writeLayoutFile(userTemplates + QStringLiteral("/UserLayout.layout.latte"), 2);

    const QString sysTemplates = m_dataDir.path() + QStringLiteral("/plasma/shells/org.kde.latte.shell/contents/templates");
    QVERIFY(QDir().mkpath(sysTemplates));
    writeLayoutFile(sysTemplates + QStringLiteral("/SysView.view.latte"), 2);
    // Same name in both trees must appear once (the system entry is deduped).
    writeLayoutFile(sysTemplates + QStringLiteral("/UserView.view.latte"), 2);

    const QStringList views = Importer::availableViewTemplates();
    QVERIFY(views.contains(QStringLiteral("UserView")));
    QVERIFY(views.contains(QStringLiteral("SysView")));
    QCOMPARE(views.count(QStringLiteral("UserView")), 1);

    QVERIFY(Importer::hasViewTemplate(QStringLiteral("UserView")));
    QVERIFY(!Importer::hasViewTemplate(QStringLiteral("NoSuchTemplate")));

    QVERIFY(Importer::availableLayoutTemplates().contains(QStringLiteral("UserLayout")));
}

void ImporterLogicTest::autostartEnableDisableRoundTrip()
{
    // enableAutostart copies the shipped .desktop from a data dir into the config
    // autostart dir and drops the deprecated file; disableAutostart removes it.
    const QString apps = m_dataDir.path() + QStringLiteral("/applications");
    QVERIFY(QDir().mkpath(apps));
    QFile meta(apps + QStringLiteral("/org.kde.latte-dock.desktop"));
    QVERIFY(meta.open(QIODevice::WriteOnly | QIODevice::Text));
    meta.write("[Desktop Entry]\nType=Application\nName=Latte\n");
    meta.close();

    // Seed the deprecated autostart file so its removal branch runs.
    QVERIFY(QDir().mkpath(configPath() + QStringLiteral("/autostart")));
    QFile old(configPath() + QStringLiteral("/autostart/latte-dock.desktop"));
    QVERIFY(old.open(QIODevice::WriteOnly));
    old.write("x");
    old.close();

    QVERIFY(!Importer::isAutostartEnabled());
    Importer::enableAutostart();
    QVERIFY(Importer::isAutostartEnabled());
    QVERIFY(!QFile::exists(configPath() + QStringLiteral("/autostart/latte-dock.desktop")));

    // A second enable with the file already present is a no-op.
    Importer::enableAutostart();
    QVERIFY(Importer::isAutostartEnabled());

    Importer::disableAutostart();
    QVERIFY(!Importer::isAutostartEnabled());
}

void ImporterLogicTest::checkRepairMovesLinkedContainments()
{
    writeLayoutFile(Importer::layoutUserFilePath(QStringLiteral("Work")), 2);

    const QString linkedPath = Importer::layoutUserFilePath(QString::fromLatin1(Latte::Layout::MULTIPLELAYOUTSHIDDENNAME));
    KSharedConfigPtr linkedPtr = KSharedConfig::openConfig(linkedPath);
    KConfigGroup linkedContainments(linkedPtr, QStringLiteral("Containments"));
    linkedContainments.group(QStringLiteral("101")).writeEntry(QStringLiteral("layoutId"), QStringLiteral("Work"));
    // A containment pointing at a non-existent layout is left unmoved and cleared as a ghost.
    linkedContainments.group(QStringLiteral("102")).writeEntry(QStringLiteral("layoutId"), QStringLiteral("GhostLayout"));
    linkedPtr->sync();

    const QStringList updated = Importer::checkRepairMultipleLayoutsLinkedFile();
    QVERIFY(updated.contains(QStringLiteral("Work")));
    QVERIFY(!updated.contains(QStringLiteral("GhostLayout")));

    // Containment 101 now lives in Work's file...
    KSharedConfigPtr workPtr = KSharedConfig::openConfig(Importer::layoutUserFilePath(QStringLiteral("Work")));
    QVERIFY(KConfigGroup(workPtr, QStringLiteral("Containments")).hasGroup(QStringLiteral("101")));

    // ...and the linked file's containments are emptied.
    KSharedConfigPtr linkedAfter = KSharedConfig::openConfig(linkedPath);
    QVERIFY(KConfigGroup(linkedAfter, QStringLiteral("Containments")).groupList().isEmpty());
}

void ImporterLogicTest::importLayoutInstanceEmitsSignal()
{
    // The instance importLayout wraps the static helper and announces the new path.
    // A null parent means no Manager, which importLayout never dereferences.
    Importer imp(nullptr);
    QVERIFY(!imp.storageTmpDir().isEmpty());

    const QString src = writeLayoutFile(configPath() + QStringLiteral("/InstanceSrc.layout.latte"), 2);
    QSignalSpy spy(&imp, &Importer::newLayoutAdded);

    const QString name = imp.importLayout(src, QStringLiteral("InstanceImported"));
    QCOMPARE(name, QStringLiteral("InstanceImported"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), Importer::layoutUserFilePath(QStringLiteral("InstanceImported")));

    // A rejected import emits nothing.
    const QString bad = writeLayoutFile(configPath() + QStringLiteral("/InstanceBad.layout.latte"), 1);
    QVERIFY(imp.importLayout(bad, QStringLiteral("InstanceBad")).isEmpty());
    QCOMPARE(spy.count(), 1);
}

void ImporterLogicTest::storageTmpDirExists()
{
    Importer imp(nullptr);
    const QString tmp = imp.storageTmpDir();
    QVERIFY(!tmp.isEmpty());
    QVERIFY(QDir(tmp).exists());
}

void ImporterLogicTest::exportFullConfigurationArchivesTree()
{
    // Lay down a small config tree: the rc file, one user layout, and two custom
    // templates.
    const QString rc = configPath() + QStringLiteral("/lattedockrc");
    QFile rcFile(rc);
    QVERIFY(rcFile.open(QIODevice::WriteOnly));
    rcFile.write("[UniversalSettings]\nversion=2\n");
    rcFile.close();

    writeLayoutFile(latteDir() + QStringLiteral("/MyLayout.layout.latte"), 2);

    QVERIFY(QDir(latteDir()).mkpath(QStringLiteral("templates")));
    const QString templatesDir = latteDir() + QStringLiteral("/templates");
    writeLayoutFile(templatesDir + QStringLiteral("/Custom.layout.latte"), 2);
    QFile viewTemplate(templatesDir + QStringLiteral("/Custom.view.latte"));
    QVERIFY(viewTemplate.open(QIODevice::WriteOnly));
    viewTemplate.write("[ViewTemplateSettings]\nversion=2\n");
    viewTemplate.close();

    const QString archivePath = configPath() + QStringLiteral("/full-export.latterc");

    Importer imp(nullptr);
    QVERIFY(imp.exportFullConfiguration(archivePath));
    QVERIFY(QFile::exists(archivePath));

    // The archive holds the rc at the root and the layouts + templates under latte/.
    KTar archive(archivePath, QStringLiteral("application/x-tar"));
    QVERIFY(archive.open(QIODevice::ReadOnly));
    const KArchiveDirectory *root = archive.directory();
    QVERIFY(root->entries().contains(QStringLiteral("lattedockrc")));

    const auto *latte = dynamic_cast<const KArchiveDirectory *>(root->entry(QStringLiteral("latte")));
    QVERIFY(latte);
    QVERIFY(latte->entries().contains(QStringLiteral("MyLayout.layout.latte")));

    const auto *templates = dynamic_cast<const KArchiveDirectory *>(latte->entry(QStringLiteral("templates")));
    QVERIFY(templates);
    QVERIFY(templates->entries().contains(QStringLiteral("Custom.layout.latte")));
    QVERIFY(templates->entries().contains(QStringLiteral("Custom.view.latte")));
    archive.close();

    // Exporting again over the existing file exercises the remove-then-rewrite path.
    QVERIFY(imp.exportFullConfiguration(archivePath));
}

void ImporterLogicTest::importOldLayoutDefaultSession()
{
    const QString oldPath = configPath() + QStringLiteral("/default-appletsrc");
    writeOldAppletsrc(oldPath, /*DefaultSession*/ 0, /*systrayId*/ 2,
                      {QStringLiteral("a.desktop"), QStringLiteral("b.desktop")}, {});

    Importer imp(nullptr);
    QVERIFY(imp.importOldLayout(oldPath, QStringLiteral("ImportedDefault"), false, QString()));

    const QString newPath = latteDir() + QStringLiteral("/ImportedDefault.layout.latte");
    QVERIFY(QFile::exists(newPath));

    // The Latte containment and the systray it referenced were both carried over.
    KSharedConfigPtr newFile = KSharedConfig::openConfig(newPath);
    KConfigGroup newContainments = KConfigGroup(newFile, QStringLiteral("Containments"));
    QVERIFY(newContainments.hasGroup(QStringLiteral("1")));
    QVERIFY(newContainments.hasGroup(QStringLiteral("2")));

    // A default-session import is version 2, blue, with the default launchers.
    Latte::Layout::AbstractLayout imported(nullptr, newPath, QStringLiteral("ImportedDefault"));
    QCOMPARE(imported.version(), 2);
    QCOMPARE(imported.color(), QStringLiteral("blue"));
    QCOMPARE(imported.launchers(),
             QStringList({QStringLiteral("a.desktop"), QStringLiteral("b.desktop")}));
}

void ImporterLogicTest::importOldLayoutAlternativeSession()
{
    const QString oldPath = configPath() + QStringLiteral("/alt-appletsrc");
    writeOldAppletsrc(oldPath, /*AlternativeSession*/ 1, -1, {}, {QStringLiteral("x.desktop")});

    Importer imp(nullptr);
    QVERIFY(imp.importOldLayout(oldPath, QStringLiteral("ImportedAlt"), true, QString()));

    const QString newPath = latteDir() + QStringLiteral("/ImportedAlt.layout.latte");
    QVERIFY(QFile::exists(newPath));

    // An alternative-session import is purple, with the alternative launchers.
    Latte::Layout::AbstractLayout imported(nullptr, newPath, QStringLiteral("ImportedAlt"));
    QCOMPARE(imported.color(), QStringLiteral("purple"));
    QCOMPARE(imported.launchers(), QStringList({QStringLiteral("x.desktop")}));
}

void ImporterLogicTest::importOldLayoutRejectsWhenNoLatteContainment()
{
    const QString oldPath = configPath() + QStringLiteral("/nolatte-appletsrc");
    {
        KConfig config(oldPath);
        KConfigGroup containment = config.group(QStringLiteral("Containments")).group(QStringLiteral("1"));
        containment.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.desktopcontainment"));
        config.sync();
    }

    Importer imp(nullptr);
    QVERIFY(!imp.importOldLayout(oldPath, QStringLiteral("ShouldFail"), false, QString()));
    QVERIFY(!QFile::exists(latteDir() + QStringLiteral("/ShouldFail.layout.latte")));
}

// Wrap an old appletsrc into a .latterc archive next to a (content-irrelevant)
// lattedockrc, the pair importOldConfiguration() expects.
static QString buildOldLatterc(const QString &arcPath, const QString &appletsrcPath)
{
    QFile appletsFile(appletsrcPath);
    if (!appletsFile.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const QByteArray appletsBytes = appletsFile.readAll();
    appletsFile.close();

    KTar archive(arcPath, QStringLiteral("application/x-tar"));
    if (!archive.open(QIODevice::WriteOnly)) {
        return QString();
    }
    archive.writeFile(QStringLiteral("lattedockrc"), QByteArray("[ScreenConnectors]\n"));
    archive.writeFile(QStringLiteral("lattedock-appletsrc"), appletsBytes);
    archive.close();
    return arcPath;
}

void ImporterLogicTest::importOldConfigurationExtractsAndImports()
{
    const QString srcApplets = configPath() + QStringLiteral("/config-appletsrc");
    writeOldAppletsrc(srcApplets, /*DefaultSession*/ 0, -1, {QStringLiteral("k.desktop")}, {});

    const QString arc = configPath() + QStringLiteral("/oldconfig.latterc");
    QVERIFY(!buildOldLatterc(arc, srcApplets).isEmpty());

    Importer imp(nullptr);
    QVERIFY(imp.importOldConfiguration(arc, QStringLiteral("ArchiveImport")));
    QVERIFY(QFile::exists(latteDir() + QStringLiteral("/ArchiveImport.layout.latte")));
}

void ImporterLogicTest::importOldConfigurationRejectsMissingAndWrongFormat()
{
    Importer imp(nullptr);

    // A .latterc that does not exist.
    QVERIFY(!imp.importOldConfiguration(configPath() + QStringLiteral("/ghost.latterc"),
                                        QStringLiteral("x")));

    // A tar that holds an unexpected member is rejected as the wrong format.
    const QString badArc = configPath() + QStringLiteral("/badformat.latterc");
    {
        KTar archive(badArc, QStringLiteral("application/x-tar"));
        QVERIFY(archive.open(QIODevice::WriteOnly));
        archive.writeFile(QStringLiteral("lattedockrc"), QByteArray("[X]\n"));
        archive.writeFile(QStringLiteral("junk.txt"), QByteArray("nope"));
        archive.close();
    }
    QVERIFY(!imp.importOldConfiguration(badArc, QStringLiteral("x")));
}

void ImporterLogicTest::importOldConfigurationDerivesNameFromArchive()
{
    const QString srcApplets = configPath() + QStringLiteral("/derive-appletsrc");
    writeOldAppletsrc(srcApplets, /*DefaultSession*/ 0, -1, {}, {});

    // An empty target name falls back to the archive's base name.
    const QString arc = configPath() + QStringLiteral("/DerivedName.latterc");
    QVERIFY(!buildOldLatterc(arc, srcApplets).isEmpty());

    Importer imp(nullptr);
    QVERIFY(imp.importOldConfiguration(arc, QString()));
    QVERIFY(QFile::exists(latteDir() + QStringLiteral("/DerivedName.layout.latte")));
}

void ImporterLogicTest::importHelperExtractsConfigArchive()
{
    // A ConfigVersion2 .latterc archive is unpacked into the config dir; this wipes
    // and recreates the latte/ dir, so it runs last.
    const QString arc = writeArchive(configPath() + QStringLiteral("/toimport.latterc"),
                                     /*rc*/ 2, /*applets*/ -1, /*latteDir*/ true);
    QVERIFY(!arc.isEmpty());

    // A plain layout file is not a config archive and is rejected.
    const QString layout = writeLayoutFile(configPath() + QStringLiteral("/plain.layout.latte"), 2);
    QVERIFY(!Importer::importHelper(layout));

    QVERIFY(Importer::importHelper(arc));
    QVERIFY(QFile::exists(latteDir() + QStringLiteral("/dummy")));
}

QTEST_GUILESS_MAIN(ImporterLogicTest)

#include "importerlogictest.moc"
