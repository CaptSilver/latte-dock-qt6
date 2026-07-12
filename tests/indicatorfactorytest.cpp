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

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using Latte::Indicator::Factory;

class IndicatorFactoryTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dataDir; // XDG_DATA_HOME / XDG_DATA_DIRS

    QString indicatorsRoot() const { return m_dataDir.path() + QStringLiteral("/latte/indicators"); }

    // Write an indicator package with a valid KPlugin metadata.json (and a main.qml).
    void writeIndicator(const QString &dirName, const QString &id, const QString &name, const QString &category)
    {
        const QString dir = indicatorsRoot() + QLatin1Char('/') + dirName;
        QDir().mkpath(dir + QStringLiteral("/package"));

        const QString json = QStringLiteral(
                                 "{\n"
                                 "  \"KPlugin\": { \"Id\": \"%1\", \"Name\": \"%2\", \"Category\": \"%3\" },\n"
                                 "  \"X-Latte-MainScript\": \"main.qml\"\n"
                                 "}\n")
                                 .arg(id, name, category);
        QFile f(dir + QStringLiteral("/metadata.json"));
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        f.write(json.toUtf8());
        f.close();

        QFile ui(dir + QStringLiteral("/package/main.qml"));
        ui.open(QIODevice::WriteOnly | QIODevice::Text);
        ui.write("import QtQuick\nItem {}\n");
        ui.close();
    }

private Q_SLOTS:
    void initTestCase();

    void discoversAndSortsCustomIndicators();
    void rejectsInvalidCategory();
    void pluginTypeQueries();
    void metadataFileAbsolutePath_prefersJson();
    void staticMetadataValidity();
};

void IndicatorFactoryTest::initTestCase()
{
    QVERIFY(m_dataDir.isValid());
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

QTEST_MAIN(IndicatorFactoryTest)
#include "indicatorfactorytest.moc"
