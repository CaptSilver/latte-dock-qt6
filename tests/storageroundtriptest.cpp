/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Round-trips and reads the .layout.latte / .view.latte KConfig format that
// Latte::Layouts::Storage serializes. Storage itself cannot be linked headlessly
// (storage.cpp pulls in Corona/View/Importer), so the read/write key mappings are
// mirrored verbatim from storage.cpp and exercised over a real temp KConfig and a
// shipped template fixture. Mirroring the writer (updateView) and reader (view)
// from their own literal keys means a serialize/deserialize KEY MISMATCH surfaces
// as a failed round-trip, and parsing the real fixture guards the on-disk format
// the port must keep reading.

#include <KConfig>
#include <KConfigGroup>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>

// Storage constants (storage.cpp:41-42, viewdata.cpp:11).
static const int IDNULL = -1;
static const int IDBASE = 0;
static const int ISCLONEDNULL = -1;

class StorageRoundTripTest : public QObject
{
    Q_OBJECT

private:
    // Mirror of Storage::appletGroupIsValid (storage.cpp:288-295).
    static bool appletGroupIsValid(const KConfigGroup &appletGroup)
    {
        return !(appletGroup.keyList().count() == 0
                 && appletGroup.groupList().count() == 1
                 && appletGroup.groupList().at(0) == QLatin1String("Configuration")
                 && appletGroup.group(QStringLiteral("Configuration")).keyList().count() == 1
                 && appletGroup.group(QStringLiteral("Configuration")).hasKey(QStringLiteral("PreloadWeight")));
    }

    // Mirror of Storage::subContainmentId (storage.cpp:129-151): the two seeded
    // identities both look under [Configuration], for SystrayContainmentId then
    // ContainmentId.
    static int subContainmentId(const KConfigGroup &appletGroup)
    {
        const char *properties[] = {"SystrayContainmentId", "ContainmentId"};
        for (const char *property : properties) {
            if (appletGroup.hasGroup(QStringLiteral("Configuration"))) {
                const KConfigGroup cfg = appletGroup.group(QStringLiteral("Configuration"));
                if (cfg.hasKey(property)) {
                    return cfg.readEntry(property, IDNULL);
                }
            }
        }
        return IDNULL;
    }

    static bool isValid(int id) { return id >= IDBASE; }

private Q_SLOTS:
    void viewFieldsRoundTripThroughKConfig();
    void deserializesShippedDockTemplate();
    void appletGroupIsValidRejectsPreloadOnlyShell();
    void subContainmentIdResolvesIdentities();
};

void StorageRoundTripTest::viewFieldsRoundTripThroughKConfig()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("test.layout.latte"));

    // Write via Storage::updateView's keys (storage.cpp:1615-1631).
    {
        KConfig config(path);
        KConfigGroup g = config.group(QStringLiteral("Containments")).group(QStringLiteral("7"));
        g.writeEntry(QStringLiteral("name"), QStringLiteral("My Dock"));
        g.writeEntry(QStringLiteral("screensGroup"), 2);
        g.writeEntry(QStringLiteral("onPrimary"), false);
        g.writeEntry(QStringLiteral("isClonedFrom"), 5);
        g.writeEntry(QStringLiteral("lastScreen"), 12);
        g.group(QStringLiteral("General")).writeEntry(QStringLiteral("screenEdgeMargin"), 7);
        g.writeEntry(QStringLiteral("location"), 5);
        g.group(QStringLiteral("General")).writeEntry(QStringLiteral("maxLength"), (float)80.0);
        g.group(QStringLiteral("General")).writeEntry(QStringLiteral("alignment"), 3);
        g.sync();
    }

    // Read back from a fresh KConfig (forces a real on-disk round-trip) via
    // Storage::view's keys and defaults (storage.cpp:1584-1613).
    KConfig config(path);
    KConfigGroup g = config.group(QStringLiteral("Containments")).group(QStringLiteral("7"));
    QCOMPARE(g.readEntry(QStringLiteral("name"), QString()), QStringLiteral("My Dock"));
    QCOMPARE(g.readEntry(QStringLiteral("screensGroup"), 0), 2);
    QCOMPARE(g.readEntry(QStringLiteral("onPrimary"), true), false);
    QCOMPARE(g.readEntry(QStringLiteral("isClonedFrom"), ISCLONEDNULL), 5);
    QCOMPARE(g.readEntry(QStringLiteral("lastScreen"), IDNULL), 12);
    QCOMPARE(g.group(QStringLiteral("General")).readEntry(QStringLiteral("screenEdgeMargin"), -1), 7);
    QCOMPARE(g.readEntry(QStringLiteral("location"), 4), 5);
    QCOMPARE(g.group(QStringLiteral("General")).readEntry(QStringLiteral("maxLength"), (float)100.0), (float)80.0);
    QCOMPARE(g.group(QStringLiteral("General")).readEntry(QStringLiteral("alignment"), 0), 3);

    // maxLength must live in General (the plasmoid config group view() reads),
    // not at the containment-group level — guards the updateView write location.
    QCOMPARE(g.readEntry(QStringLiteral("maxLength"), (float)-1.0), (float)-1.0);
}

void StorageRoundTripTest::deserializesShippedDockTemplate()
{
    KConfig fixture(QStringLiteral(DOCK_TEMPLATE_PATH));
    KConfigGroup c = fixture.group(QStringLiteral("Containments")).group(QStringLiteral("1"));

    // The guard Storage::view applies before reading anything (storage.cpp:1588).
    QCOMPARE(c.readEntry(QStringLiteral("plugin"), QString()), QStringLiteral("org.kde.latte.containment"));

    // Keys present in the real file must parse to their on-disk values.
    QCOMPARE(c.readEntry(QStringLiteral("name"), QString()), QStringLiteral("Default Dock"));
    QCOMPARE(c.readEntry(QStringLiteral("location"), -99), 4); // Plasma::Types::BottomEdge
    QCOMPARE(c.readEntry(QStringLiteral("onPrimary"), false), true);
    QCOMPARE(c.readEntry(QStringLiteral("lastScreen"), 999), -1);

    // Keys absent from the file must fall to Storage::view's declared defaults.
    QCOMPARE(c.readEntry(QStringLiteral("isClonedFrom"), ISCLONEDNULL), ISCLONEDNULL);
    QCOMPARE(c.group(QStringLiteral("General")).readEntry(QStringLiteral("maxLength"), (float)100.0), (float)100.0);

    // The bundled plasmoid applet is present under this containment.
    KConfigGroup applet = c.group(QStringLiteral("Applets")).group(QStringLiteral("2"));
    QCOMPARE(applet.readEntry(QStringLiteral("plugin"), QString()), QStringLiteral("org.kde.latte.plasmoid"));
}

void StorageRoundTripTest::appletGroupIsValidRejectsPreloadOnlyShell()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KConfig config(dir.filePath(QStringLiteral("applets.latte")));

    // The empty shell: no own keys, only [Configuration]PreloadWeight.
    KConfigGroup shell = config.group(QStringLiteral("shell"));
    shell.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("PreloadWeight"), 42);
    QVERIFY2(!appletGroupIsValid(shell), "preload-only shell should be invalid");

    // A real applet has a plugin key.
    KConfigGroup real = config.group(QStringLiteral("real"));
    real.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.plasmoid"));
    real.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("PreloadWeight"), 42);
    QVERIFY2(appletGroupIsValid(real), "an applet with a plugin should be valid");

    // Extra Configuration keys also make it real.
    KConfigGroup extra = config.group(QStringLiteral("extra"));
    KConfigGroup extraCfg = extra.group(QStringLiteral("Configuration"));
    extraCfg.writeEntry(QStringLiteral("PreloadWeight"), 42);
    extraCfg.writeEntry(QStringLiteral("SystrayContainmentId"), 9);
    QVERIFY2(appletGroupIsValid(extra), "extra configuration keys should be valid");
}

void StorageRoundTripTest::subContainmentIdResolvesIdentities()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    KConfig config(dir.filePath(QStringLiteral("subs.latte")));

    KConfigGroup systray = config.group(QStringLiteral("systray"));
    systray.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("SystrayContainmentId"), 99);
    QCOMPARE(subContainmentId(systray), 99);
    QVERIFY(isValid(subContainmentId(systray)));

    KConfigGroup plain = config.group(QStringLiteral("plain"));
    plain.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("ContainmentId"), 42);
    QCOMPARE(subContainmentId(plain), 42);

    KConfigGroup applet = config.group(QStringLiteral("applet"));
    applet.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("PreloadWeight"), 42);
    QCOMPARE(subContainmentId(applet), IDNULL);
    QVERIFY(!isValid(subContainmentId(applet)));
}

QTEST_GUILESS_MAIN(StorageRoundTripTest)

#include "storageroundtriptest.moc"
