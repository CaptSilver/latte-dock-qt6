/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-link unit test for the two ScreenPool implementations: the Latte core
// pool (app/screenpool.cpp) and the Plasma-extended mirror pool
// (app/plasma/extended/screenpool.cpp). Both read a [ScreenConnectors] config
// group and expose id<->connector lookups; this seeds that group and asserts the
// mappings plus the known-id / not-found branches against real production code.

// local
#include "screenpool.h"
#include "plasma/extended/screenpool.h"

// Qt
#include <QGuiApplication>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

// KDE
#include <KConfigGroup>
#include <KSharedConfig>

class ScreenPoolTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    // Latte core ScreenPool (app/screenpool.cpp)
    void lattePool_loadsSeededConnectors();
    void lattePool_unknownConnectorIsNoScreenId();
    void lattePool_unknownIdHasEmptyConnector();
    void lattePool_hasScreenId();

    // Plasma-extended ScreenPool (app/plasma/extended/screenpool.cpp)
    void plasmaPool_loadsSeededConnectors();
    void plasmaPool_unknownConnectorIsNotFound();
    void plasmaPool_unknownIdIsEmpty();

private:
    QTemporaryDir m_configDir;
};

void ScreenPoolTest::initTestCase()
{
    QVERIFY(m_configDir.isValid());
}

// --- Latte::ScreenPool ------------------------------------------------------

void ScreenPoolTest::lattePool_loadsSeededConnectors()
{
    auto config = KSharedConfig::openConfig(m_configDir.filePath(QStringLiteral("lattepool.rc")),
                                            KConfig::SimpleConfig);
    KConfigGroup group(config, QStringLiteral("ScreenConnectors"));
    // serialized form is "name:::x,y wxh"
    group.writeEntry(QStringLiteral("10"), QStringLiteral("DP-1:::0,0 1920x1080"));
    group.writeEntry(QStringLiteral("11"), QStringLiteral("HDMI-1:::1920,0 1280x1024"));
    group.sync();

    Latte::ScreenPool pool(config);
    pool.load();

    QCOMPARE(pool.id(QStringLiteral("DP-1")), 10);
    QCOMPARE(pool.id(QStringLiteral("HDMI-1")), 11);
    QCOMPARE(pool.connector(10), QStringLiteral("DP-1"));
    QCOMPARE(pool.connector(11), QStringLiteral("HDMI-1"));
}

void ScreenPoolTest::lattePool_unknownConnectorIsNoScreenId()
{
    auto config = KSharedConfig::openConfig(m_configDir.filePath(QStringLiteral("lattepool2.rc")),
                                            KConfig::SimpleConfig);
    KConfigGroup group(config, QStringLiteral("ScreenConnectors"));
    group.writeEntry(QStringLiteral("10"), QStringLiteral("DP-1:::0,0 1920x1080"));
    group.sync();

    Latte::ScreenPool pool(config);
    pool.load();

    QCOMPARE(pool.id(QStringLiteral("does-not-exist")), int(Latte::ScreenPool::NOSCREENID));
    QCOMPARE(int(Latte::ScreenPool::NOSCREENID), -1);
}

void ScreenPoolTest::lattePool_unknownIdHasEmptyConnector()
{
    auto config = KSharedConfig::openConfig(m_configDir.filePath(QStringLiteral("lattepool3.rc")),
                                            KConfig::SimpleConfig);
    KConfigGroup group(config, QStringLiteral("ScreenConnectors"));
    group.writeEntry(QStringLiteral("10"), QStringLiteral("DP-1:::0,0 1920x1080"));
    group.sync();

    Latte::ScreenPool pool(config);
    pool.load();

    // an id that was never mapped resolves to an empty connector string
    QVERIFY(pool.connector(9999).isEmpty());
}

void ScreenPoolTest::lattePool_hasScreenId()
{
    auto config = KSharedConfig::openConfig(m_configDir.filePath(QStringLiteral("lattepool4.rc")),
                                            KConfig::SimpleConfig);
    KConfigGroup group(config, QStringLiteral("ScreenConnectors"));
    group.writeEntry(QStringLiteral("10"), QStringLiteral("DP-1:::0,0 1920x1080"));
    group.sync();

    Latte::ScreenPool pool(config);
    pool.load();

    QVERIFY(pool.hasScreenId(10));
    QVERIFY(!pool.hasScreenId(9999));
    // negative ids are never valid screen ids
    QVERIFY(!pool.hasScreenId(-1));
    QVERIFY(!pool.hasScreenId(int(Latte::ScreenPool::NOSCREENID)));
}

// --- Latte::PlasmaExtended::ScreenPool --------------------------------------
//
// This pool hardcodes KSharedConfig::openConfig("plasmashellrc"), which resolves
// against XDG_CONFIG_HOME. main() points XDG_CONFIG_HOME at a temp dir and the
// file is seeded there before each construction.

static void seedPlasmaShellRc()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("plasmashellrc"));
    KConfigGroup group(config, QStringLiteral("ScreenConnectors"));
    group.writeEntry(QStringLiteral("0"), QStringLiteral("eDP-1"));
    group.writeEntry(QStringLiteral("1"), QStringLiteral("DP-2"));
    group.writeEntry(QStringLiteral("2"), QStringLiteral("HDMI-A-1"));
    group.sync();
}

void ScreenPoolTest::plasmaPool_loadsSeededConnectors()
{
    seedPlasmaShellRc();

    Latte::PlasmaExtended::ScreenPool pool;

    QCOMPARE(pool.id(QStringLiteral("DP-2")), 1);
    QCOMPARE(pool.id(QStringLiteral("HDMI-A-1")), 2);
    QCOMPARE(pool.connector(1), QStringLiteral("DP-2"));
    QCOMPARE(pool.connector(2), QStringLiteral("HDMI-A-1"));
}

void ScreenPoolTest::plasmaPool_unknownConnectorIsNotFound()
{
    seedPlasmaShellRc();

    Latte::PlasmaExtended::ScreenPool pool;

    // a connector that is neither mapped nor the primary screen returns -1
    QCOMPARE(pool.id(QStringLiteral("totally-unknown-output")), -1);
}

void ScreenPoolTest::plasmaPool_unknownIdIsEmpty()
{
    seedPlasmaShellRc();

    Latte::PlasmaExtended::ScreenPool pool;

    // an unmapped, non-zero id has no connector
    QVERIFY(pool.connector(4242).isEmpty());
}

int main(int argc, char *argv[])
{
    // Point KSharedConfig at a throwaway config dir before QGuiApplication so the
    // Plasma-extended pool's hardcoded "plasmashellrc" resolves to our seed file
    // and never touches the real desktop config.
    static QTemporaryDir xdgConfig;
    qputenv("XDG_CONFIG_HOME", xdgConfig.path().toUtf8());

    QGuiApplication app(argc, argv);
    ScreenPoolTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "screenpooltest.moc"
