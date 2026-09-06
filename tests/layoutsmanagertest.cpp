/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-object test for Latte::Layouts::Manager (app/layouts/manager.cpp).
//
// Manager's header drags in lattecorona.h and most of its methods dereference
// the Corona (universalSettings, templatesManager, kPackage), so it can't run
// against a null Corona. A real Latte::Corona builds fine offscreen (the same
// trick layoutsmodeltest/viewsmodeltest use), so we construct one, take its
// real layoutsManager(), and drive the surface that needs no live shell: the
// object accessors, the memory-usage round-trip, iconForLayout()'s user-icon /
// background-file / empty branches, moveView()'s argument guards, and the
// empty-state delegations to the Synchronizer. The private startup/cleanup
// helpers and the settings/about dialogs only run against a live dock and are
// left to the live capture.

#include "../app/lattecorona.h"
#include "../app/layouts/manager.h"
#include "../app/layouts/importer.h"
#include "../app/layouts/synchronizer.h"
#include "../app/layout/abstractlayout.h"
#include "../app/data/layoutdata.h"
#include "../app/data/layouticondata.h"
#include "../app/settings/universalsettings.h"

#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

using namespace Latte;

class LayoutsManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void exposesRealCollaborators();
    void memoryUsageRoundTrips();
    void iconForLayoutPrefersUserIcon();
    void iconForLayoutUsesExistingBackgroundFile();
    void iconForLayoutIsEmptyWhenBackgroundMissing();
    void iconForLayoutByNameMatchesEmptyLayout();
    void iconForLayoutPatternBackgroundBranch();
    void moveViewIgnoresInvalidArguments();
    void delegationsAreEmptyWithoutLoadedLayouts();
    void viewTemplateNamesAndIdsAlign();

private:
    QTemporaryDir m_configDir;
    Latte::Corona *m_corona{nullptr};
    Layouts::Manager *m_manager{nullptr};
};

void LayoutsManagerTest::initTestCase()
{
    // setMemoryUsage() writes lattedockrc, so point config writes at a temp dir
    // and keep the test out of the real config. XDG_DATA_DIRS is left untouched
    // so the installed Latte shell package still resolves for the Corona.
    QVERIFY(m_configDir.isValid());
    qputenv("XDG_CONFIG_HOME", m_configDir.path().toUtf8());

    //! Latte resolves packages through XDG_DATA_HOME too, so a shell package installed under
    //! ~/.local/share/plasma/shells would shadow the staged one and fail this test for reasons
    //! unrelated to the code. Redirect it alongside the config dir.
    qputenv("XDG_DATA_HOME", m_configDir.path().toUtf8());
    m_corona = new Latte::Corona(false, QString(), QString(), 0, nullptr);
    QVERIFY(m_corona->universalSettings() != nullptr);
    m_manager = m_corona->layoutsManager();
    QVERIFY(m_manager != nullptr);
}

void LayoutsManagerTest::cleanupTestCase()
{
    // The Corona owns a large graph whose headless teardown can re-enter shell
    // paths; leak it for the process lifetime like the sibling model tests.
    m_corona = nullptr;
}

void LayoutsManagerTest::exposesRealCollaborators()
{
    QCOMPARE(m_manager->corona(), m_corona);
    QVERIFY(m_manager->importer() != nullptr);
    QVERIFY(m_manager->synchronizer() != nullptr);
    QVERIFY(m_manager->syncedLaunchers() != nullptr);
}

void LayoutsManagerTest::memoryUsageRoundTrips()
{
    m_manager->setMemoryUsage(MemoryUsage::MultipleLayouts);
    QCOMPARE(m_manager->memoryUsage(), MemoryUsage::MultipleLayouts);

    m_manager->setMemoryUsage(MemoryUsage::SingleLayout);
    QCOMPARE(m_manager->memoryUsage(), MemoryUsage::SingleLayout);
}

void LayoutsManagerTest::iconForLayoutPrefersUserIcon()
{
    Data::Layout layout;
    layout.icon = QStringLiteral("preferences-desktop");

    Data::LayoutIcon icon = m_manager->iconForLayout(layout);

    QCOMPARE(icon.name, QStringLiteral("preferences-desktop"));
    QVERIFY(!icon.isBackgroundFile);
    QVERIFY(!icon.isEmpty());
}

void LayoutsManagerTest::iconForLayoutUsesExistingBackgroundFile()
{
    // An absolute background path that exists is returned verbatim as a
    // background-file icon.
    const QString bgPath = m_configDir.path() + QStringLiteral("/background.png");
    QFile bg(bgPath);
    QVERIFY(bg.open(QIODevice::WriteOnly));
    bg.write("not-a-real-image");
    bg.close();

    Data::Layout layout;
    layout.background = bgPath;

    Data::LayoutIcon icon = m_manager->iconForLayout(layout);

    QCOMPARE(icon.name, bgPath);
    QVERIFY(icon.isBackgroundFile);
    QVERIFY(!icon.isEmpty());
}

void LayoutsManagerTest::iconForLayoutIsEmptyWhenBackgroundMissing()
{
    Data::Layout layout;
    layout.background = QStringLiteral("/definitely/not/here/missing.png");

    Data::LayoutIcon icon = m_manager->iconForLayout(layout);

    QVERIFY(icon.isEmpty());
}

void LayoutsManagerTest::iconForLayoutByNameMatchesEmptyLayout()
{
    // The name overload resolves through Synchronizer::data(); an unknown name
    // yields an empty Data::Layout, so its icon must match the empty-layout icon
    // computed directly. This pins the delegation without depending on which
    // package fallback file happens to exist.
    Data::Layout empty;
    const Data::LayoutIcon expected = m_manager->iconForLayout(empty);

    const Data::LayoutIcon byName = m_manager->iconForLayout(QStringLiteral("no-such-layout"));

    QCOMPARE(byName.name, expected.name);
    QCOMPARE(byName.isBackgroundFile, expected.isBackgroundFile);
}

void LayoutsManagerTest::iconForLayoutPatternBackgroundBranch()
{
    // A pattern-style layout with no explicit background takes the
    // "defaultcustomprint.jpg" package-image branch. Whether that asset ships
    // decides background-file vs empty, but either way the branch executes and
    // the result stays internally consistent.
    Data::Layout layout;
    layout.backgroundStyle = Layout::PatternBackgroundStyle;

    Data::LayoutIcon icon = m_manager->iconForLayout(layout);

    QVERIFY(icon.isEmpty() || (icon.isBackgroundFile && !icon.name.isEmpty()));
}

void LayoutsManagerTest::moveViewIgnoresInvalidArguments()
{
    // moveView only acts in MultipleLayouts mode with two distinct, existing
    // layouts and a positive view id. Every invalid combination is a safe
    // no-op; with no layouts loaded none of them can move anything.
    m_manager->setMemoryUsage(MemoryUsage::SingleLayout);
    m_manager->moveView(QStringLiteral("a"), 1, QStringLiteral("b")); // wrong mode

    m_manager->setMemoryUsage(MemoryUsage::MultipleLayouts);
    m_manager->moveView(QString(), 1, QStringLiteral("b"));            // empty origin
    m_manager->moveView(QStringLiteral("a"), 1, QString());           // empty destination
    m_manager->moveView(QStringLiteral("a"), 0, QStringLiteral("b")); // non-positive id
    m_manager->moveView(QStringLiteral("a"), 1, QStringLiteral("a")); // same layout
    m_manager->moveView(QStringLiteral("a"), 1, QStringLiteral("b")); // unknown layouts

    QVERIFY(m_manager->currentLayouts().isEmpty());
    m_manager->setMemoryUsage(MemoryUsage::SingleLayout);
}

void LayoutsManagerTest::delegationsAreEmptyWithoutLoadedLayouts()
{
    QVERIFY(m_manager->currentLayoutsNames().isEmpty());
    QVERIFY(m_manager->centralLayoutsNames().isEmpty());
    QVERIFY(m_manager->currentLayouts().isEmpty());

    // unload() with nothing loaded is a safe delegation to the Synchronizer.
    m_manager->unload();
    QVERIFY(m_manager->currentLayouts().isEmpty());
}

void LayoutsManagerTest::viewTemplateNamesAndIdsAlign()
{
    // Both lists walk the same view-template table, so they stay the same length
    // whatever the loaded template set is.
    const QStringList names = m_manager->viewTemplateNames();
    const QStringList ids = m_manager->viewTemplateIds();
    QCOMPARE(names.size(), ids.size());
}

QTEST_MAIN(LayoutsManagerTest)
#include "layoutsmanagertest.moc"
