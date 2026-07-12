/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-object test for Latte::Templates::Manager (app/templates/templatesmanager.cpp).
//
// The header drags in lattecorona.h and init() scans the Corona's shell package
// for the shipped templates, so we build a real headless Corona (as
// layoutsmodeltest/viewsmodeltest do), redirect the config location at a temp
// dir, and drive the manager end to end: init() loading the system layout/view
// templates, the query surface (has*/layoutTemplates/viewTemplates/
// layoutTemplateForName/viewTemplateFilePath), the unique-name routing in
// proposedTemplateAbsolutePath(), and the file writers newLayout(),
// importSystemLayouts() and installCustomLayoutTemplate(). Everything Latte
// stores lives under configPath()/latte, so a single XDG_CONFIG_HOME redirect
// keeps the writes off the real config while the shipped package (on
// XDG_DATA_DIRS) still resolves.

#include "../app/lattecorona.h"
#include "../app/templates/templatesmanager.h"
#include "../app/layouts/importer.h"
#include "../app/tools/commontools.h"
#include "../app/data/layoutdata.h"
#include "../app/data/layoutstable.h"
#include "../app/data/genericbasictable.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

using namespace Latte;

class TemplatesManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void initLoadsSystemTemplates();
    void layoutTemplatesFrontLoadsDefaultAndEmpty();
    void layoutTemplateForNameFoundAndMissing();
    void viewTemplateLookups();
    void proposedPathDedupesAndRoutesByExtension();
    void newLayoutCreatesFileFromTemplate();
    void newLayoutRejectsUnknownTemplate();
    void importSystemLayoutsCopiesToUserDir();
    void installCustomLayoutTemplateCopiesAndIsCustom();

private:
    QTemporaryDir m_configDir;
    Latte::Corona *m_corona{nullptr};
    Templates::Manager *m_tm{nullptr};
};

void TemplatesManagerTest::initTestCase()
{
    QVERIFY(m_configDir.isValid());
    qputenv("XDG_CONFIG_HOME", m_configDir.path().toUtf8());
    // QFile::copy() does not create parent dirs, so make the layout + template
    // directories the writers target.
    QVERIFY(QDir(m_configDir.path()).mkpath(QStringLiteral("latte/templates")));

    m_corona = new Latte::Corona(false, QString(), QString(), 0, nullptr);
    QVERIFY(m_corona->universalSettings() != nullptr);

    m_tm = new Templates::Manager(m_corona);
    m_tm->init();
}

void TemplatesManagerTest::cleanupTestCase()
{
    delete m_tm;
    m_tm = nullptr;
    // Leak the Corona: its headless teardown can re-enter live-shell paths.
    m_corona = nullptr;
}

void TemplatesManagerTest::initLoadsSystemTemplates()
{
    QVERIFY(m_tm->hasLayoutTemplate(QStringLiteral("Default")));
    QVERIFY(m_tm->hasLayoutTemplate(QStringLiteral("Empty")));
    QVERIFY(m_tm->hasLayoutTemplate(QStringLiteral("Extended")));
    QVERIFY(m_tm->hasViewTemplate(QStringLiteral("Default Dock")));
    QVERIFY(m_tm->viewTemplates().rowCount() >= 3);
}

void TemplatesManagerTest::layoutTemplatesFrontLoadsDefaultAndEmpty()
{
    Data::LayoutsTable templates = m_tm->layoutTemplates();
    QVERIFY(templates.rowCount() >= 3);
    QCOMPARE(templates[0u].name, QStringLiteral("Default"));
    QCOMPARE(templates[1u].name, QStringLiteral("Empty"));
}

void TemplatesManagerTest::layoutTemplateForNameFoundAndMissing()
{
    Data::Layout found = m_tm->layoutTemplateForName(QStringLiteral("Default"));
    QCOMPARE(found.name, QStringLiteral("Default"));
    QVERIFY(!found.id.isEmpty());

    Data::Layout missing = m_tm->layoutTemplateForName(QStringLiteral("No Such Template"));
    QVERIFY(missing.id.isEmpty());
}

void TemplatesManagerTest::viewTemplateLookups()
{
    QVERIFY(m_tm->hasViewTemplate(QStringLiteral("Default Dock")));
    QVERIFY(!m_tm->hasViewTemplate(QStringLiteral("No Such View")));

    const QString path = m_tm->viewTemplateFilePath(QStringLiteral("Default Dock"));
    QVERIFY(path.endsWith(QStringLiteral("Default Dock.view.latte")));
    QVERIFY(QFile::exists(path));
    QVERIFY(m_tm->viewTemplateFilePath(QStringLiteral("No Such View")).isEmpty());

    // Shipped templates are system, never custom.
    QVERIFY(!m_tm->hasCustomLayoutTemplate(QStringLiteral("Default")));
}

void TemplatesManagerTest::proposedPathDedupesAndRoutesByExtension()
{
    const QString templatesDir = Latte::configPath() + QStringLiteral("/latte/templates/");

    // A layout-template name that already exists is bumped to " - 2".
    QCOMPARE(m_tm->proposedTemplateAbsolutePath(QStringLiteral("Default.layout.latte")),
             templatesDir + QStringLiteral("Default - 2.layout.latte"));

    // A fresh view-template name is kept as-is.
    QCOMPARE(m_tm->proposedTemplateAbsolutePath(QStringLiteral("Brandnew.view.latte")),
             templatesDir + QStringLiteral("Brandnew.view.latte"));

    // An unrecognised extension is passed straight through.
    QCOMPARE(m_tm->proposedTemplateAbsolutePath(QStringLiteral("plainname")),
             templatesDir + QStringLiteral("plainname"));
}

void TemplatesManagerTest::newLayoutCreatesFileFromTemplate()
{
    QSignalSpy spy(m_tm, &Templates::Manager::newLayoutAdded);

    const QString path = m_tm->newLayout(QStringLiteral("CoverageProbe"), QStringLiteral("Default"));

    QVERIFY(path.endsWith(QStringLiteral("CoverageProbe.layout.latte")));
    QVERIFY(QFile::exists(path));
    QCOMPARE(spy.count(), 1);
}

void TemplatesManagerTest::newLayoutRejectsUnknownTemplate()
{
    QSignalSpy spy(m_tm, &Templates::Manager::newLayoutAdded);

    const QString path = m_tm->newLayout(QStringLiteral("Whatever"), QStringLiteral("No Such Template"));

    QVERIFY(path.isEmpty());
    QCOMPARE(spy.count(), 0);
}

void TemplatesManagerTest::importSystemLayoutsCopiesToUserDir()
{
    m_tm->importSystemLayouts();

    QVERIFY(QFile::exists(Layouts::Importer::layoutUserFilePath(QStringLiteral("Default"))));
    QVERIFY(QFile::exists(Layouts::Importer::layoutUserFilePath(QStringLiteral("Extended"))));
}

void TemplatesManagerTest::installCustomLayoutTemplateCopiesAndIsCustom()
{
    // A path that is not a layout template is ignored: nothing lands in the
    // user templates dir.
    QDir userTemplates(Latte::configPath() + QStringLiteral("/latte/templates"));
    const int templatesBefore = userTemplates.entryList(QDir::Files).count();
    m_tm->installCustomLayoutTemplate(QStringLiteral("/tmp/not-a-template.txt"));
    QCOMPARE(userTemplates.entryList(QDir::Files).count(), templatesBefore);

    // A real .layout.latte outside the system dir installs as a custom template.
    const QString src = m_configDir.path() + QStringLiteral("/MyCustom.layout.latte");
    QVERIFY(QFile::copy(m_tm->layoutTemplateForName(QStringLiteral("Default")).id, src));

    m_tm->installCustomLayoutTemplate(src);

    const QString dest = Latte::configPath() + QStringLiteral("/latte/templates/MyCustom.layout.latte");
    QVERIFY(QFile::exists(dest));

    m_tm->init();
    QVERIFY(m_tm->hasCustomLayoutTemplate(QStringLiteral("MyCustom")));
}

QTEST_MAIN(TemplatesManagerTest)
#include "templatesmanagertest.moc"
