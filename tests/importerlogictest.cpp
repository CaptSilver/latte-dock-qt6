/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-object behavioral test for the static parser/path helpers of
// app/layouts/importer.cpp (Latte::Layouts::Importer). The header drags in
// manager.h/lattecorona.h transitively, so importer.cpp is driven through the
// prebuilt latte-dock application objects (the glob-link), not recompiled here.
//
// importernametest.cpp re-implements the suffix/extension string logic in the
// test itself; this test instead calls the REAL compiled statics:
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
#include <KTar>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using Latte::Layouts::Importer;

class ImporterLogicTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_configHome;  // XDG_CONFIG_HOME -> Latte::configPath()
    QTemporaryDir m_dataDir;     // XDG_DATA_DIRS    -> system data root

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
                         int rcVersion,        // -1 = omit lattedockrc
                         int appletsVersion,   // -1 = omit lattedock-appletsrc
                         bool withLatteDir)
    {
        KTar archive(path, QStringLiteral("application/x-tar"));
        if (!archive.open(QIODevice::WriteOnly)) {
            return QString();
        }

        if (rcVersion >= 0) {
            const QByteArray rc = QStringLiteral("[UniversalSettings]\nversion=%1\n")
                                      .arg(rcVersion).toUtf8();
            archive.writeFile(QStringLiteral("lattedockrc"), rc);
        }
        if (appletsVersion >= 0) {
            const QByteArray applets = QStringLiteral("[LayoutSettings]\nversion=%1\n")
                                           .arg(appletsVersion).toUtf8();
            archive.writeFile(QStringLiteral("lattedock-appletsrc"), applets);
        }
        if (withLatteDir) {
            // A directory entry; copyTo() recreates it so QDir(latte).exists() is true.
            archive.writeFile(QStringLiteral("latte/dummy"), QByteArray("x"));
        }

        archive.close();
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
};

void ImporterLogicTest::initTestCase()
{
    QVERIFY(m_configHome.isValid());
    QVERIFY(m_dataDir.isValid());

    // configPath() reads ConfigLocation (XDG_CONFIG_HOME); layout dir lives under it.
    qputenv("XDG_CONFIG_HOME", m_configHome.path().toLocal8Bit());
    // systemShellDataPath()/standardPaths() read GenericDataLocation. Pin DATA_HOME
    // to the same dir as DATA_DIRS' tail so the host's real data dirs don't leak in.
    qputenv("XDG_DATA_HOME", m_dataDir.path().toLocal8Bit());
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
                                     /*rc*/1, /*applets*/1, /*latteDir*/false);
    QVERIFY(!arc.isEmpty());
    QCOMPARE(Importer::fileVersion(arc), Importer::ConfigVersion1);
}

void ImporterLogicTest::fileVersionArchiveConfigV2()
{
    // version-2 rc + a latte/ directory => ConfigVersion2.
    const QString arc = writeArchive(configPath() + QStringLiteral("/new.latterc"),
                                     /*rc*/2, /*applets*/-1, /*latteDir*/true);
    QVERIFY(!arc.isEmpty());
    QCOMPARE(Importer::fileVersion(arc), Importer::ConfigVersion2);
}

void ImporterLogicTest::fileVersionArchiveUnknown()
{
    // version-2 rc but no latte/ dir: neither v1 nor a complete v2 => Unknown.
    const QString arc = writeArchive(configPath() + QStringLiteral("/partial.latterc"),
                                     /*rc*/2, /*applets*/-1, /*latteDir*/false);
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

    QTest::newRow("strips latterc")    << QStringLiteral("/home/u/My Config.latterc") << QStringLiteral("My Config");
    QTest::newRow("bare latterc")      << QStringLiteral("foo.latterc")                << QStringLiteral("foo");
    QTest::newRow("non-latterc kept")  << QStringLiteral("/p/lattedockrc")             << QStringLiteral("lattedockrc");
    // The -1 lastIndexOf miss must NOT chop the trailing character (the Qt6
    // remove(-1,8) bug the chop()/endsWith() rewrite fixed).
    QTest::newRow("no extension kept") << QStringLiteral("/p/Plasma")                  << QStringLiteral("Plasma");
    QTest::newRow("single char kept")  << QStringLiteral("/p/A")                       << QStringLiteral("A");
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
    const QString sysData = m_dataDir.path()
                            + QStringLiteral("/plasma/shells/org.kde.latte.shell");
    QCOMPARE(Importer::systemShellDataPath(), sysData);
    QCOMPARE(Importer::layoutTemplateSystemFilePath(QStringLiteral("Default")),
             sysData + QStringLiteral("/contents/templates/Default.layout.latte"));
}

void ImporterLogicTest::standardPathsOrdering()
{
    const QStringList localFirst = Importer::standardPaths(true);
    const QStringList localLast = Importer::standardPaths(false);

    QVERIFY(!localFirst.isEmpty());
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

QTEST_GUILESS_MAIN(ImporterLogicTest)

#include "importerlogictest.moc"
