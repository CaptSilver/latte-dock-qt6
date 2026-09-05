/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "coronahelpers.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>

#include <KConfig>
#include <KConfigGroup>

using namespace Latte;

class CoronaHelpersTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void strippedLatteName_dropsARecognisedSuffix();
    void strippedLatteName_keepsAnythingElse();
    void strippedLatteName_stripsTheSuffixOnly();
    void isLayoutFilePath_acceptsAbsoluteAndFileUrls();
    void isLayoutFilePath_rejectsWrongSuffixOrRelative();
    void cleanLayoutFilePath_stripsFileScheme();
    void cleanLayoutFilePath_leavesPlainPath();
    void prune_removesObsoleteContainmentsAndApplets();
    void prune_noChangeWhenEverythingLive();
    void contextMenu_marshalsFieldsAndOriginalView();
    void contextMenu_encodesClonedViewAndMultipleLayouts();
    void contextMenu_encodesNeitherWhenNoView();
    void contextMenu_parsesLayoutsFieldBackFromTheWire();
    void contextMenu_dropsLayoutEntriesTruncatedByASeparatorInTheName();
    void contextMenu_parsesAnEmptyLayoutsFieldAsNoEntries();
    void parseWindowIdAndScheme_splitsOnFirstDash();
    void parseWindowIdAndScheme_handlesEdges();
    void validPageOrFirst_keepsInRangeElseFirst();
};

void CoronaHelpersTest::strippedLatteName_dropsARecognisedSuffix()
{
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/Default.layout.latte"), {CoronaHelpers::LAYOUTEXTENSION}),
             QStringLiteral("Default"));
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/Default Dock.view.latte"), {CoronaHelpers::VIEWEXTENSION}),
             QStringLiteral("Default Dock"));
    //! either of the two, whichever the name ends with
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/Default Dock.view.latte"),
                                              {CoronaHelpers::LAYOUTEXTENSION, CoronaHelpers::VIEWEXTENSION}),
             QStringLiteral("Default Dock"));
    //! a path with no directory part is a bare file name
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("Empty.layout.latte"), {CoronaHelpers::LAYOUTEXTENSION}),
             QStringLiteral("Empty"));
    //! everything before the extension survives, dots included -- QFileInfo::baseName()
    //! cuts at the FIRST dot and used to rename this one to "Plasma 5"
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/Plasma 5.27.layout.latte"), {CoronaHelpers::LAYOUTEXTENSION}),
             QStringLiteral("Plasma 5.27"));
}

void CoronaHelpersTest::strippedLatteName_keepsAnythingElse()
{
    //! an extension the caller did not ask for is not one of ours
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/Default Dock.view.latte"), {CoronaHelpers::LAYOUTEXTENSION}),
             QStringLiteral("Default Dock.view.latte"));
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/notes.txt"), {CoronaHelpers::LAYOUTEXTENSION, CoronaHelpers::VIEWEXTENSION}),
             QStringLiteral("notes.txt"));
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/Plasma"), {CoronaHelpers::LAYOUTEXTENSION}),
             QStringLiteral("Plasma"));
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/.directory"), {CoronaHelpers::LAYOUTEXTENSION}),
             QStringLiteral(".directory"));
    //! with nothing to strip the file name is all the caller gets back
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/Default.layout.latte"), {}),
             QStringLiteral("Default.layout.latte"));
    QCOMPARE(CoronaHelpers::strippedLatteName(QString(), {CoronaHelpers::LAYOUTEXTENSION}), QString());
}

void CoronaHelpersTest::strippedLatteName_stripsTheSuffixOnly()
{
    //! QString::remove(const QString &) deletes every occurrence anywhere in the name;
    //! only the trailing one is the extension
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/My .view.latte backup.view.latte"), {CoronaHelpers::VIEWEXTENSION}),
             QStringLiteral("My .view.latte backup"));
    //! and a name that merely contains the extension keeps all of it
    QCOMPARE(CoronaHelpers::strippedLatteName(QStringLiteral("/p/Backup.view.latte copy"), {CoronaHelpers::VIEWEXTENSION}),
             QStringLiteral("Backup.view.latte copy"));
}

void CoronaHelpersTest::isLayoutFilePath_acceptsAbsoluteAndFileUrls()
{
    QVERIFY(CoronaHelpers::isLayoutFilePath(QStringLiteral("/home/user/Default.layout.latte")));
    QVERIFY(CoronaHelpers::isLayoutFilePath(QStringLiteral("file:///home/user/Default.layout.latte")));
    QVERIFY(CoronaHelpers::isLayoutFilePath(QStringLiteral("file:/home/user/Default.layout.latte")));
}

void CoronaHelpersTest::isLayoutFilePath_rejectsWrongSuffixOrRelative()
{
    QVERIFY(!CoronaHelpers::isLayoutFilePath(QStringLiteral("/home/user/Default.layout")));
    QVERIFY(!CoronaHelpers::isLayoutFilePath(QStringLiteral("/home/user/Default.layout.latte.bak")));
    QVERIFY(!CoronaHelpers::isLayoutFilePath(QStringLiteral("relative/Default.layout.latte")));
    QVERIFY(!CoronaHelpers::isLayoutFilePath(QString()));
}

void CoronaHelpersTest::cleanLayoutFilePath_stripsFileScheme()
{
    //! file:///abs -> /abs (the empty-authority form)
    QCOMPARE(CoronaHelpers::cleanLayoutFilePath(QStringLiteral("file:///home/user/a.layout.latte")),
             QStringLiteral("/home/user/a.layout.latte"));
    //! file://abs -> /abs (the double-slash form)
    QCOMPARE(CoronaHelpers::cleanLayoutFilePath(QStringLiteral("file://home/user/a.layout.latte")),
             QStringLiteral("/home/user/a.layout.latte"));
}

void CoronaHelpersTest::cleanLayoutFilePath_leavesPlainPath()
{
    QCOMPARE(CoronaHelpers::cleanLayoutFilePath(QStringLiteral("/home/user/a.layout.latte")),
             QStringLiteral("/home/user/a.layout.latte"));
}

void CoronaHelpersTest::prune_removesObsoleteContainmentsAndApplets()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    KConfig config(dir.filePath(QStringLiteral("lattecorona.rc")), KConfig::SimpleConfig);
    KConfigGroup containments = config.group(QStringLiteral("Containments"));

    //! containment 1: live, has a live applet 10 and an obsolete applet 11
    containments.group(QStringLiteral("1")).writeEntry("plugin", "org.kde.latte.containment");
    containments.group(QStringLiteral("1")).group(QStringLiteral("Applets")).group(QStringLiteral("10")).writeEntry("plugin", "live");
    containments.group(QStringLiteral("1")).group(QStringLiteral("Applets")).group(QStringLiteral("11")).writeEntry("plugin", "obsolete");
    //! containment 2: obsolete entirely
    containments.group(QStringLiteral("2")).writeEntry("plugin", "org.kde.latte.containment");
    containments.group(QStringLiteral("2")).group(QStringLiteral("Applets")).group(QStringLiteral("20")).writeEntry("plugin", "x");
    //! containment 3: live, no applets
    containments.group(QStringLiteral("3")).writeEntry("plugin", "org.kde.latte.containment");

    const QSet<uint> liveContainments{1, 3};
    const QHash<uint, QSet<uint>> liveApplets{{1, {10}}, {3, {}}};

    const bool changed = CoronaHelpers::pruneObsoleteContainmentConfig(containments, liveContainments, liveApplets);

    QVERIFY(changed);

    QStringList remaining = containments.groupList();
    remaining.sort();
    QCOMPARE(remaining, QStringList({QStringLiteral("1"), QStringLiteral("3")}));

    QCOMPARE(containments.group(QStringLiteral("1")).group(QStringLiteral("Applets")).groupList(),
             QStringList({QStringLiteral("10")}));
}

void CoronaHelpersTest::prune_noChangeWhenEverythingLive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    KConfig config(dir.filePath(QStringLiteral("lattecorona.rc")), KConfig::SimpleConfig);
    KConfigGroup containments = config.group(QStringLiteral("Containments"));

    containments.group(QStringLiteral("5")).writeEntry("plugin", "org.kde.latte.containment");
    containments.group(QStringLiteral("5")).group(QStringLiteral("Applets")).group(QStringLiteral("50")).writeEntry("plugin", "live");

    const QSet<uint> liveContainments{5};
    const QHash<uint, QSet<uint>> liveApplets{{5, {50}}};

    const bool changed = CoronaHelpers::pruneObsoleteContainmentConfig(containments, liveContainments, liveApplets);

    QVERIFY(!changed);
    QCOMPARE(containments.groupList(), QStringList({QStringLiteral("5")}));
    QCOMPARE(containments.group(QStringLiteral("5")).group(QStringLiteral("Applets")).groupList(),
             QStringList({QStringLiteral("50")}));
}

void CoronaHelpersTest::contextMenu_parsesAnEmptyLayoutsFieldAsNoEntries()
{
    // Splitting an empty string yields one empty piece, so the loop still runs once
    // and reads a triple that is not there.
    QCOMPARE(Latte::Data::ContextMenu::parseLayoutsMenuField(QString()).count(), 0);
}

void CoronaHelpersTest::contextMenu_dropsLayoutEntriesTruncatedByASeparatorInTheName()
{
    // A layout may be named anything, including the field separator. "a;;b**0**icon"
    // therefore arrives as two pieces, and the first one carries no triple at all --
    // this is what crashes the dock when the Layouts submenu is hovered.
    const auto entries = Latte::Data::ContextMenu::parseLayoutsMenuField(QStringLiteral("a;;b**0**icon"));

    QCOMPARE(entries.count(), 1);
    QCOMPARE(entries.at(0).name, QStringLiteral("b"));
    QCOMPARE(entries.at(0).isBackgroundFile, false);
    QCOMPARE(entries.at(0).iconName, QStringLiteral("icon"));
}

void CoronaHelpersTest::contextMenu_parsesLayoutsFieldBackFromTheWire()
{
    CoronaHelpers::ContextMenuInputs in;
    in.menuLayouts = {{QStringLiteral("Default"), true, QStringLiteral("sunset")},
                      {QStringLiteral("Work"), false, QStringLiteral("blue")}};

    const QStringList payload = CoronaHelpers::buildContextMenuData(in);
    const auto entries = Latte::Data::ContextMenu::parseLayoutsMenuField(payload.at(4));

    QCOMPARE(entries.count(), in.menuLayouts.count());

    for (int i=0; i<entries.count(); ++i) {
        QCOMPARE(entries.at(i).name, in.menuLayouts.at(i).name);
        QCOMPARE(entries.at(i).isBackgroundFile, in.menuLayouts.at(i).isBackgroundFile);
        QCOMPARE(entries.at(i).iconName, in.menuLayouts.at(i).iconName);
    }
}

void CoronaHelpersTest::contextMenu_marshalsFieldsAndOriginalView()
{
    CoronaHelpers::ContextMenuInputs in;
    in.memoryUsage = 1;
    in.centralLayoutsNames = {QStringLiteral("Default"), QStringLiteral("Work")};
    in.currentLayoutsNames = {QStringLiteral("Default")};
    in.alwaysShownActions = {QStringLiteral("add"), QStringLiteral("remove")};
    in.menuLayouts = {{QStringLiteral("Default"), true, QStringLiteral("sunset")}};
    in.selectedViewLayoutName = QStringLiteral("Default");
    in.viewType = 0;
    in.viewIsOriginal = true;
    in.viewClonesCount = 3;

    const QStringList expected{
        QStringLiteral("1"),
        QStringLiteral("Default;;Work"),
        QStringLiteral("Default"),
        QStringLiteral("add;;remove"),
        QStringLiteral("Default**1**sunset"),
        QStringLiteral("Default"),
        QStringLiteral("0;;0;;3")};

    QCOMPARE(CoronaHelpers::buildContextMenuData(in), expected);
}

void CoronaHelpersTest::contextMenu_encodesClonedViewAndMultipleLayouts()
{
    CoronaHelpers::ContextMenuInputs in;
    in.memoryUsage = 0;
    in.centralLayoutsNames = {QStringLiteral("A")};
    in.currentLayoutsNames = {QStringLiteral("A"), QStringLiteral("B")};
    //! alwaysShownActions left empty -> joins to ""
    in.menuLayouts = {{QStringLiteral("A"), false, QStringLiteral("blue")},
                      {QStringLiteral("B"), true, QStringLiteral("red")}};
    in.selectedViewLayoutName = QStringLiteral("A");
    in.viewType = 2;
    in.viewIsCloned = true;

    const QStringList expected{
        QStringLiteral("0"),
        QStringLiteral("A"),
        QStringLiteral("A;;B"),
        QString(),
        QStringLiteral("A**0**blue;;B**1**red"),
        QStringLiteral("A"),
        QStringLiteral("2;;1;;0")};

    QCOMPARE(CoronaHelpers::buildContextMenuData(in), expected);
}

void CoronaHelpersTest::contextMenu_encodesNeitherWhenNoView()
{
    //! no view selected: empty layouts, empty selected name, neither original nor cloned
    CoronaHelpers::ContextMenuInputs in;

    const QStringList expected{
        QStringLiteral("0"),
        QString(),
        QString(),
        QString(),
        QString(),
        QString(),
        QStringLiteral("0;;0;;0")};

    QCOMPARE(CoronaHelpers::buildContextMenuData(in), expected);
}

void CoronaHelpersTest::parseWindowIdAndScheme_splitsOnFirstDash()
{
    auto a = CoronaHelpers::parseWindowIdAndScheme(QStringLiteral("123-MyScheme"));
    QCOMPARE(a.windowId, QStringLiteral("123"));
    QCOMPARE(a.scheme, QStringLiteral("MyScheme"));

    //! only the first dash splits; later dashes stay in the scheme
    auto b = CoronaHelpers::parseWindowIdAndScheme(QStringLiteral("42-Breeze-Dark"));
    QCOMPARE(b.windowId, QStringLiteral("42"));
    QCOMPARE(b.scheme, QStringLiteral("Breeze-Dark"));
}

void CoronaHelpersTest::parseWindowIdAndScheme_handlesEdges()
{
    //! leading dash -> empty id
    auto a = CoronaHelpers::parseWindowIdAndScheme(QStringLiteral("-Breeze"));
    QCOMPARE(a.windowId, QString());
    QCOMPARE(a.scheme, QStringLiteral("Breeze"));

    //! no dash -> indexOf(-1) leaves both fields equal to the whole string
    auto b = CoronaHelpers::parseWindowIdAndScheme(QStringLiteral("noscheme"));
    QCOMPARE(b.windowId, QStringLiteral("noscheme"));
    QCOMPARE(b.scheme, QStringLiteral("noscheme"));
}

void CoronaHelpersTest::validPageOrFirst_keepsInRangeElseFirst()
{
    QCOMPARE(CoronaHelpers::validPageOrFirst(2, 0, 4), 2);
    QCOMPARE(CoronaHelpers::validPageOrFirst(0, 0, 4), 0);
    QCOMPARE(CoronaHelpers::validPageOrFirst(4, 0, 4), 4);
    QCOMPARE(CoronaHelpers::validPageOrFirst(-1, 0, 4), 0);
    QCOMPARE(CoronaHelpers::validPageOrFirst(5, 0, 4), 0);
}

QTEST_MAIN(CoronaHelpersTest)
#include "coronahelperstest.moc"
