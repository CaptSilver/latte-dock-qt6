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
    void isLayoutFilePath_acceptsAbsoluteAndFileUrls();
    void isLayoutFilePath_rejectsWrongSuffixOrRelative();
    void cleanLayoutFilePath_stripsFileScheme();
    void cleanLayoutFilePath_leavesPlainPath();
    void prune_removesObsoleteContainmentsAndApplets();
    void prune_noChangeWhenEverythingLive();
};

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

QTEST_MAIN(CoronaHelpersTest)
#include "coronahelperstest.moc"
