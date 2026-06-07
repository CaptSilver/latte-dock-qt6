/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include "smartlauncheritem.h"

class SmartLauncherItemTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultsAreEmpty();
    void retainsLauncherUrl();
};

void SmartLauncherItemTest::defaultsAreEmpty()
{
    SmartLauncher::Item item;
    QCOMPARE(item.count(), 0);
    QCOMPARE(item.countVisible(), false);
    QCOMPARE(item.progress(), 0);
    QCOMPARE(item.progressVisible(), false);
    QCOMPARE(item.urgent(), false);
}

void SmartLauncherItemTest::retainsLauncherUrl()
{
    SmartLauncher::Item item;
    // A deliberately unresolvable launcher: no service matches, so the item stays
    // empty but must still expose the url it was handed.
    const QUrl url(QStringLiteral("applications:org.kde.latte.nonexistent-test.desktop"));
    item.setLauncherUrl(url);
    QCOMPARE(item.launcherUrl(), url);
    QCOMPARE(item.count(), 0);
}

QTEST_GUILESS_MAIN(SmartLauncherItemTest)
#include "smartlauncheritemtest.moc"
