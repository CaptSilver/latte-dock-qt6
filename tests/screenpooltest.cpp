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
    void lattePool_removeScreensSkipsAbsentIdAndRemovesPresent();
    void lattePool_firstAvailableIdFillsGap();
    void lattePool_firstAvailableIdAdvancesPastContiguousRun();

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

void ScreenPoolTest::lattePool_removeScreensSkipsAbsentIdAndRemovesPresent()
{
    auto config = KSharedConfig::openConfig(m_configDir.filePath(QStringLiteral("lattepool_remove.rc")),
                                            KConfig::SimpleConfig);
    KConfigGroup group(config, QStringLiteral("ScreenConnectors"));
    group.writeEntry(QStringLiteral("10"), QStringLiteral("DP-1:::0,0 1920x1080"));
    group.writeEntry(QStringLiteral("11"), QStringLiteral("HDMI-1:::1920,0 1280x1024"));
    group.sync();

    Latte::ScreenPool pool(config);
    pool.load();
    QVERIFY(pool.hasScreenId(10));
    QVERIFY(pool.hasScreenId(11));

    // Obsolete list: an id that is NOT mapped, followed by one that IS. The
    // absent id must not abort processing of the present one (the bug returned
    // on the first miss, silently leaving later obsolete screens mapped).
    Latte::Data::ScreensTable obsolete;
    obsolete << Latte::Data::Screen(QStringLiteral("99"), QStringLiteral("ghost:::0,0 800x600"));
    obsolete << Latte::Data::Screen(QStringLiteral("11"), QStringLiteral("HDMI-1:::1920,0 1280x1024"));

    pool.removeScreens(obsolete);

    // id 11 was present and listed obsolete -> gone, despite the earlier absent id 99
    QVERIFY(!pool.hasScreenId(11));
    // id 10 was never obsolete -> still mapped
    QVERIFY(pool.hasScreenId(10));
}

void ScreenPoolTest::lattePool_firstAvailableIdFillsGap()
{
    auto config = KSharedConfig::openConfig(m_configDir.filePath(QStringLiteral("lattepool_gap.rc")),
                                            KConfig::SimpleConfig);
    KConfigGroup group(config, QStringLiteral("ScreenConnectors"));
    // Seed three connectors so the QGuiApp virtual screen (which load() auto-inserts
    // for any QScreen not yet in the table) occupies id 13, leaving 10-12 under our control.
    group.writeEntry(QStringLiteral("10"), QStringLiteral("DP-0:::0,0 1920x1080"));
    group.writeEntry(QStringLiteral("11"), QStringLiteral("DP-1:::1920,0 1920x1080"));
    group.writeEntry(QStringLiteral("12"), QStringLiteral("HDMI-1:::3840,0 1280x1024"));
    group.sync();

    Latte::ScreenPool pool(config);
    pool.load();
    // After load, ids 10-12 are ours; the virtual QScreen fills 13+.
    QVERIFY(pool.hasScreenId(10));
    QVERIFY(pool.hasScreenId(11));
    QVERIFY(pool.hasScreenId(12));

    // Punch a hole at 11 so there is a gap between 10 and 12.
    Latte::Data::ScreensTable toRemove;
    toRemove << Latte::Data::Screen(QStringLiteral("11"), QStringLiteral("DP-1:::1920,0 1920x1080"));
    pool.removeScreens(toRemove);
    QVERIFY(!pool.hasScreenId(11));

    // A new connector must reuse the lowest free id -- the hole at 11, not 13+.
    pool.insertScreenMapping(QStringLiteral("NEW-1"));

    QCOMPARE(pool.id(QStringLiteral("NEW-1")), 11);
    QCOMPARE(pool.connector(11), QStringLiteral("NEW-1"));
}

void ScreenPoolTest::lattePool_firstAvailableIdAdvancesPastContiguousRun()
{
    auto config = KSharedConfig::openConfig(m_configDir.filePath(QStringLiteral("lattepool_contig.rc")),
                                            KConfig::SimpleConfig);
    KConfigGroup group(config, QStringLiteral("ScreenConnectors"));
    // Seed two connectors; load() will auto-insert the virtual QScreen at 12.
    // insertScreenMapping("NEW-2") must then advance to 13.
    group.writeEntry(QStringLiteral("10"), QStringLiteral("DP-1:::0,0 1920x1080"));
    group.writeEntry(QStringLiteral("11"), QStringLiteral("HDMI-1:::1920,0 1280x1024"));
    group.sync();

    Latte::ScreenPool pool(config);
    pool.load();
    // ids 10 and 11 are ours; the QGuiApp virtual screen fills 12.
    QVERIFY(pool.hasScreenId(10));
    QVERIFY(pool.hasScreenId(11));
    QVERIFY(pool.hasScreenId(12));

    // The contiguous run 10,11,12 means the next new id is 13.
    pool.insertScreenMapping(QStringLiteral("NEW-2"));

    QCOMPARE(pool.id(QStringLiteral("NEW-2")), 13);
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
