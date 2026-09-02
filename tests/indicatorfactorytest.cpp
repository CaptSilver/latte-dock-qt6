/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-object behavioral test for Latte::Indicator::Factory. factory.cpp pulls in
// the layouts/importer -> Corona chain through its includes, so it is driven via
// the prebuilt latte-dock application objects (the glob-link). A real Factory is
// built over a throwaway XDG data tree holding a few indicator packages, exercising
// the discover -> reload -> alphabetical-register path plus the static helpers.

#include "indicator/factory.h"

#include <KPluginMetaData>
#include <KArchive/KTar>
#include <KArchive/KZip>

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using Latte::Indicator::Factory;

class IndicatorFactoryTest : public QObject
{
    Q_OBJECT

private:
    enum Format {
        Zip,
        Tar
    };

    QTemporaryDir m_dataDir; // XDG_DATA_HOME / XDG_DATA_DIRS

    QTemporaryDir m_scratch; // archives and loose metadata files, kept out of the data tree

    QString indicatorsRoot() const { return m_dataDir.path() + QStringLiteral("/latte/indicators"); }

    static QString metadataJson(const QString &id, const QString &name, const QString &category)
    {
        return QStringLiteral(
                   "{\n"
                   "  \"KPlugin\": { \"Id\": \"%1\", \"Name\": \"%2\", \"Category\": \"%3\" },\n"
                   "  \"X-Latte-MainScript\": \"main.qml\"\n"
                   "}\n")
            .arg(id, name, category);
    }

    // Write an indicator package with a valid KPlugin metadata.json (and a main.qml).
    void writeIndicator(const QString &dirName, const QString &id, const QString &name, const QString &category)
    {
        const QString dir = indicatorsRoot() + QLatin1Char('/') + dirName;
        QDir().mkpath(dir + QStringLiteral("/package"));

        const QString json = metadataJson(id, name, category);
        QFile f(dir + QStringLiteral("/metadata.json"));
        QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text), qPrintable(f.fileName()));
        f.write(json.toUtf8());
        f.close();

        QFile ui(dir + QStringLiteral("/package/main.qml"));
        QVERIFY2(ui.open(QIODevice::WriteOnly | QIODevice::Text), qPrintable(ui.fileName()));
        ui.write("import QtQuick\nItem {}\n");
        ui.close();
    }

    // Build the kind of *.indicator.latte a user picks in "Add Indicator...": one
    // package directory, metadata.json and all, zipped or tarred as the import
    // supports both.
    QString writeIndicatorArchive(const QString &fileName, const QString &dirName, const QString &id, Format format = Zip)
    {
        const QString path = m_scratch.path() + QLatin1Char('/') + fileName;
        std::unique_ptr<KArchive> archive;

        if (format == Zip) {
            archive = std::make_unique<KZip>(path);
        } else {
            archive = std::make_unique<KTar>(path, QStringLiteral("application/x-tar"));
        }

        if (!archive->open(QIODevice::WriteOnly)) {
            return QString();
        }

        archive->writeFile(dirName + QStringLiteral("/metadata.json"),
                           metadataJson(id, QStringLiteral("Payload Indicator"), QStringLiteral("Latte Indicator")).toUtf8());
        archive->writeFile(dirName + QStringLiteral("/package/main.qml"), QByteArray("import QtQuick\nItem {}\n"));
        archive->close();

        return path;
    }

    // A loose metadata.json for the record-level checks; the file name doubles as the
    // KPluginMetaData id fallback, so it is unique per case.
    KPluginMetaData loadMetadata(const QString &fileName, const QString &id)
    {
        const QString path = m_scratch.path() + QLatin1Char('/') + fileName;
        QFile f(path);

        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return KPluginMetaData();
        }

        f.write(metadataJson(id, QStringLiteral("Some Indicator"), QStringLiteral("Latte Indicator")).toUtf8());
        f.close();

        return KPluginMetaData::fromJsonFile(path);
    }

private Q_SLOTS:
    void initTestCase();

    void discoversAndSortsCustomIndicators();
    void rejectsInvalidCategory();
    void pluginTypeQueries();
    void metadataFileAbsolutePath_prefersJson();
    void staticMetadataValidity();

    //! Declared last: a successful import adds an indicator to the data tree, which
    //! would throw off discoversAndSortsCustomIndicators()'s exact count.
    void metadataAreValidRejectsUnsafeIds();
    void importRejectsTraversingPluginId();
    void importCreatesMissingIndicatorsRoot();
    void importAcceptsTarArchives();
    void importFailsCleanlyWithUnusableTmpdir();
};

void IndicatorFactoryTest::initTestCase()
{
    QVERIFY(m_dataDir.isValid());
    QVERIFY(m_scratch.isValid());

    // KNotification would otherwise pop real import toasts onto the session's desktop.
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/dev/null");

    qputenv("XDG_DATA_HOME", m_dataDir.path().toLocal8Bit());
    qputenv("XDG_DATA_DIRS", m_dataDir.path().toLocal8Bit());

    // Three well-formed custom indicators (discovered in filesystem order) plus one
    // with the wrong category that must be ignored.
    writeIndicator(QStringLiteral("zeta"), QStringLiteral("org.kde.latte.custom.zeta"), QStringLiteral("Zeta Indicator"), QStringLiteral("Latte Indicator"));
    writeIndicator(QStringLiteral("alpha"), QStringLiteral("org.kde.latte.custom.alpha"), QStringLiteral("Alpha Indicator"), QStringLiteral("Latte Indicator"));
    writeIndicator(QStringLiteral("mango"), QStringLiteral("org.kde.latte.custom.mango"), QStringLiteral("Mango Indicator"), QStringLiteral("Latte Indicator"));
    writeIndicator(QStringLiteral("broken"), QStringLiteral("org.kde.latte.custom.broken"), QStringLiteral("Broken Indicator"), QStringLiteral("Not An Indicator"));
}

void IndicatorFactoryTest::discoversAndSortsCustomIndicators()
{
    Factory factory(nullptr);

    // The three valid customs are registered and their names sorted alphabetically
    // regardless of the discovery order (the reload() insertion sort).
    QCOMPARE(factory.customPluginsCount(), 3);
    QCOMPARE(factory.customPluginNames(),
             (QStringList{QStringLiteral("Alpha Indicator"), QStringLiteral("Mango Indicator"), QStringLiteral("Zeta Indicator")}));
    QCOMPARE(factory.customPluginIds(),
             (QStringList{QStringLiteral("org.kde.latte.custom.alpha"), QStringLiteral("org.kde.latte.custom.mango"), QStringLiteral("org.kde.latte.custom.zeta")}));

    QVERIFY(factory.metadata(QStringLiteral("org.kde.latte.custom.alpha")).isValid());
    QCOMPARE(factory.metadata(QStringLiteral("org.kde.latte.custom.alpha")).name(), QStringLiteral("Alpha Indicator"));
}

void IndicatorFactoryTest::rejectsInvalidCategory()
{
    Factory factory(nullptr);
    // The wrong-category package parses but fails metadataAreValid(), so it never
    // registers.
    QVERIFY(!factory.pluginExists(QStringLiteral("org.kde.latte.custom.broken")));
    QVERIFY(!factory.customPluginIds().contains(QStringLiteral("org.kde.latte.custom.broken")));
}

void IndicatorFactoryTest::pluginTypeQueries()
{
    Factory factory(nullptr);
    QVERIFY(factory.pluginExists(QStringLiteral("org.kde.latte.custom.zeta")));
    QVERIFY(!factory.pluginExists(QStringLiteral("org.kde.latte.custom.nope")));

    QVERIFY(factory.isCustomType(QStringLiteral("org.kde.latte.custom.zeta")));
    // The three built-in ids are never "custom".
    QVERIFY(!factory.isCustomType(QStringLiteral("org.kde.latte.default")));
    QVERIFY(!factory.isCustomType(QStringLiteral("org.kde.latte.plasma")));
}

void IndicatorFactoryTest::metadataFileAbsolutePath_prefersJson()
{
    QTemporaryDir probe;
    QVERIFY(probe.isValid());

    // Empty dir -> nothing found.
    QVERIFY(Factory::metadataFileAbsolutePath(probe.path()).isEmpty());

    // A .desktop alone is returned...
    const QString desktop = probe.path() + QStringLiteral("/metadata.desktop");
    QFile d(desktop);
    QVERIFY(d.open(QIODevice::WriteOnly));
    d.write("[Desktop Entry]\n");
    d.close();
    QCOMPARE(Factory::metadataFileAbsolutePath(probe.path()), desktop);

    // ...but metadata.json takes precedence once present.
    const QString jsonPath = probe.path() + QStringLiteral("/metadata.json");
    QFile j(jsonPath);
    QVERIFY(j.open(QIODevice::WriteOnly));
    j.write("{}\n");
    j.close();
    QCOMPARE(Factory::metadataFileAbsolutePath(probe.path()), jsonPath);
}

void IndicatorFactoryTest::staticMetadataValidity()
{
    // A non-existent file is never valid.
    QString missing = m_dataDir.path() + QStringLiteral("/nowhere/metadata.json");
    QVERIFY(!Factory::metadataAreValid(missing));

    // A real indicator's metadata.json parses valid.
    QString good = indicatorsRoot() + QStringLiteral("/alpha/metadata.json");
    QVERIFY(Factory::metadataAreValid(good));

    // The record-level check enforces category + main script.
    KPluginMetaData valid = KPluginMetaData::fromJsonFile(good);
    QVERIFY(Factory::metadataAreValid(valid));

    KPluginMetaData wrongCategory = KPluginMetaData::fromJsonFile(indicatorsRoot() + QStringLiteral("/broken/metadata.json"));
    QVERIFY(!Factory::metadataAreValid(wrongCategory));
}

void IndicatorFactoryTest::metadataAreValidRejectsUnsafeIds()
{
    // The id ends up as a directory name under the indicators tree and as the argument
    // kpackagetool6 removes, so anything that is not a plain component is refused.
    KPluginMetaData traversing = loadMetadata(QStringLiteral("traversing.json"), QStringLiteral("../../x"));
    QVERIFY(!Factory::metadataAreValid(traversing));

    KPluginMetaData dot = loadMetadata(QStringLiteral("dot.json"), QStringLiteral("."));
    QVERIFY(!Factory::metadataAreValid(dot));

    KPluginMetaData dotdot = loadMetadata(QStringLiteral("dotdot.json"), QStringLiteral(".."));
    QVERIFY(!Factory::metadataAreValid(dotdot));

    KPluginMetaData nested = loadMetadata(QStringLiteral("nested.json"), QStringLiteral("ok/sub"));
    QVERIFY(!Factory::metadataAreValid(nested));

    // A trailing newline must not sneak past an unanchored match either.
    KPluginMetaData newline = loadMetadata(QStringLiteral("newline.json"), QStringLiteral("evil\\n"));
    QVERIFY(!Factory::metadataAreValid(newline));

    KPluginMetaData ordinary = loadMetadata(QStringLiteral("ordinary.json"), QStringLiteral("org.kde.latte.custom.alpha"));
    QVERIFY(Factory::metadataAreValid(ordinary));
}

void IndicatorFactoryTest::importRejectsTraversingPluginId()
{
    // Everything the crafted id can reach stays inside the test's own temp tree:
    // <data>/latte/indicators/../../victim resolves to <data>/victim.
    const QString victim = m_dataDir.path() + QStringLiteral("/victim");
    QDir().mkpath(victim);
    QFile keep(victim + QStringLiteral("/keepme.txt"));
    QVERIFY(keep.open(QIODevice::WriteOnly | QIODevice::Text));
    keep.write("precious\n");
    keep.close();

    const QString archivePath = writeIndicatorArchive(QStringLiteral("evil.indicator.latte"),
                                                      QStringLiteral("pkg"),
                                                      QStringLiteral("../../victim"));
    QVERIFY(!archivePath.isEmpty());

    QCOMPARE(Factory::importIndicatorFile(archivePath), Latte::ImportExport::FailedState);

    QVERIFY(QFile::exists(victim + QStringLiteral("/keepme.txt")));
    QVERIFY(!QDir(victim + QStringLiteral("/package")).exists());
}

void IndicatorFactoryTest::importCreatesMissingIndicatorsRoot()
{
    // A profile that never downloaded an indicator has no <data>/latte/indicators.
    QTemporaryDir fresh;
    QVERIFY(fresh.isValid());

    const QByteArray previousDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", fresh.path().toLocal8Bit());

    const QString archivePath = writeIndicatorArchive(QStringLiteral("fresh.indicator.latte"),
                                                      QStringLiteral("pkg"),
                                                      QStringLiteral("org.kde.latte.custom.fresh"));
    const Latte::ImportExport::State state = archivePath.isEmpty() ? Latte::ImportExport::FailedState
                                                                   : Factory::importIndicatorFile(archivePath);

    qputenv("XDG_DATA_HOME", previousDataHome);

    QVERIFY(!archivePath.isEmpty());
    QCOMPARE(state, Latte::ImportExport::InstalledState);
    QVERIFY(QFile::exists(fresh.path() + QStringLiteral("/latte/indicators/org.kde.latte.custom.fresh/metadata.json")));
}

void IndicatorFactoryTest::importAcceptsTarArchives()
{
    // A tarred package must import too: KArchive::isOpen() stays true after a failed
    // open(), so a naive isOpen() check would keep feeding a tar to the zip reader.
    const QString archivePath = writeIndicatorArchive(QStringLiteral("tarred.indicator.latte"),
                                                      QStringLiteral("pkg"),
                                                      QStringLiteral("org.kde.latte.custom.tarred"),
                                                      Tar);
    QVERIFY(!archivePath.isEmpty());

    QCOMPARE(Factory::importIndicatorFile(archivePath), Latte::ImportExport::InstalledState);
    QVERIFY(QFile::exists(indicatorsRoot() + QStringLiteral("/org.kde.latte.custom.tarred/metadata.json")));
}

void IndicatorFactoryTest::importFailsCleanlyWithUnusableTmpdir()
{
    // With no usable temp dir the extraction target is an empty string, which resolves
    // to the working directory - so run this from a scratch dir and check it stays clean.
    QTemporaryDir cwd;
    QVERIFY(cwd.isValid());

    const QString archivePath = writeIndicatorArchive(QStringLiteral("valid.indicator.latte"),
                                                      QStringLiteral("pkg"),
                                                      QStringLiteral("org.kde.latte.custom.tmpdir"));
    QVERIFY(!archivePath.isEmpty());

    const QString previousCwd = QDir::currentPath();
    const QByteArray previousTmpDir = qgetenv("TMPDIR");
    QVERIFY(QDir::setCurrent(cwd.path()));
    qputenv("TMPDIR", "/nonexistent/zz");

    const Latte::ImportExport::State state = Factory::importIndicatorFile(archivePath);
    const QStringList leftovers = QDir(cwd.path()).entryList(QDir::NoDotAndDotDot | QDir::AllEntries);

    if (previousTmpDir.isEmpty()) {
        qunsetenv("TMPDIR");
    } else {
        qputenv("TMPDIR", previousTmpDir);
    }
    QVERIFY(QDir::setCurrent(previousCwd));

    QCOMPARE(state, Latte::ImportExport::FailedState);
    QVERIFY2(leftovers.isEmpty(), qPrintable(leftovers.join(QLatin1Char(','))));
}

QTEST_MAIN(IndicatorFactoryTest)
#include "indicatorfactorytest.moc"
