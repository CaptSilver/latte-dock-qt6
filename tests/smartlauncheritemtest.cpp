/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include "smartlauncheritem.h"
#include "smartlauncherbackend.h"

#include <limits>

class SmartLauncherItemTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultsAreEmpty();
    void retainsLauncherUrl();
    void sanitizesCount();
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
//! Unity badge counts arrive over D-Bus as whatever numeric type the peer felt
//! like sending. Anything past INT_MAX used to reach the entry through
//! QVariant::value<int>(), which wraps: 3000000000 came out as -1294967296.
void SmartLauncherItemTest::sanitizesCount()
{
    using SmartLauncher::Backend;

    QCOMPARE(Backend::sanitizedCount(QVariant(42)), 42);
    QCOMPARE(Backend::sanitizedCount(QVariant(0)), 0);

    QCOMPARE(Backend::sanitizedCount(QVariant(qlonglong(3000000000))), std::numeric_limits<int>::max());
    QCOMPARE(Backend::sanitizedCount(QVariant(double(3e9))), std::numeric_limits<int>::max());
    QCOMPARE(Backend::sanitizedCount(QVariant(std::numeric_limits<int>::max())), std::numeric_limits<int>::max());

    //! A badge count is a quantity; negatives are not a smaller badge, they are junk.
    QCOMPARE(Backend::sanitizedCount(QVariant(-7)), 0);

    QCOMPARE(Backend::sanitizedCount(QVariant(QStringLiteral("42"))), 42);
    QCOMPARE(Backend::sanitizedCount(QVariant(QStringLiteral("foo"))), 0);
}

#include "smartlauncheritemtest.moc"
