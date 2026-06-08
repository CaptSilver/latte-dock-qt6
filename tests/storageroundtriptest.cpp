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

// Storage constants (storage.cpp:41-42, viewdata.cpp:11, screenpool.h:33-34).
static const int IDNULL = -1;
static const int IDBASE = 0;
static const int ISCLONEDNULL = -1;
static const int FIRSTSCREENID = 10;
static const int NOSCREENID = -1;

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

    // Mirror of Storage::isLatteContainment (storage.cpp:97-101).
    static bool isLatteContainment(const KConfigGroup &group)
    {
        return group.readEntry(QStringLiteral("plugin"), QString()) == QStringLiteral("org.kde.latte.containment");
    }

    // Mirror of Storage::isSubContainment(KConfigGroup) (storage.cpp:120).
    static bool isSubContainment(const KConfigGroup &appletGroup)
    {
        return isValid(subContainmentId(appletGroup));
    }

    // Mirror of Storage::exportTemplate's sanitization pass (storage.cpp:734-773):
    // clear layoutId + the shortcuts flag per containment, strip the config of
    // unapproved non-subcontainment applets, reset rejected subcontainments'
    // General group, then clear the export-sensitive LayoutSettings.
    static void sanitizeForTemplate(KConfig &config, const QStringList &approved)
    {
        KConfigGroup containments = config.group(QStringLiteral("Containments"));
        QStringList rejectedSubs;

        const QStringList cIds = containments.groupList();
        for (const QString &cId : cIds) {
            KConfigGroup c = containments.group(cId);
            c.writeEntry(QStringLiteral("layoutId"), QString());
            if (isLatteContainment(c)) {
                c.writeEntry(QStringLiteral("isPreferredForShortcuts"), false);
            }

            KConfigGroup applets = c.group(QStringLiteral("Applets"));
            const QStringList aIds = applets.groupList();
            for (const QString &aId : aIds) {
                KConfigGroup a = applets.group(aId);
                if (approved.contains(a.readEntry(QStringLiteral("plugin"), QString()))) {
                    continue;
                }
                if (!isSubContainment(a)) {
                    const QStringList cfgIds = a.groupList();
                    for (const QString &cfgId : cfgIds) {
                        a.group(cfgId).deleteGroup();
                    }
                } else {
                    rejectedSubs << QString::number(subContainmentId(a));
                }
            }
        }

        for (const QString &cId : cIds) {
            if (rejectedSubs.contains(cId)) {
                containments.group(cId).group(QStringLiteral("General")).deleteGroup();
            }
        }

        KConfigGroup ls = config.group(QStringLiteral("LayoutSettings"));
        ls.writeEntry(QStringLiteral("preferredForShortcutsTouched"), false);
        ls.writeEntry(QStringLiteral("lastUsedActivity"), QString());
        ls.writeEntry(QStringLiteral("activities"), QStringList());
        config.sync();
    }

    // Mirror of Storage::expectedViewScreenId (storage.cpp:1812-1825).
    enum ScreensGroup { SingleScreenGroup, AllScreensGroup, AllSecondaryScreensGroup };
    struct ViewScreen {
        ScreensGroup screensGroup{SingleScreenGroup};
        bool onPrimary{true};
        int screen{FIRSTSCREENID};
        bool cloned{false};
        bool valid{true};
    };
    static int expectedViewScreenId(const ViewScreen &v, int primaryId, const QList<int> &secondaries)
    {
        if (!v.valid) {
            return NOSCREENID;
        }
        if (v.screensGroup == SingleScreenGroup || v.cloned) {
            return v.onPrimary ? primaryId : v.screen;
        } else if (v.screensGroup == AllScreensGroup) {
            return primaryId;
        } else if (v.screensGroup == AllSecondaryScreensGroup) {
            return (secondaries.contains(v.screen) || secondaries.isEmpty()) ? v.screen : secondaries.first();
        }
        return NOSCREENID;
    }

private Q_SLOTS:
    void viewFieldsRoundTripThroughKConfig();
    void deserializesShippedDockTemplate();
    void appletGroupIsValidRejectsPreloadOnlyShell();
    void subContainmentIdResolvesIdentities();
    void exportTemplateSanitizesLayout();
    void expectedViewScreenIdMath();
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

void StorageRoundTripTest::exportTemplateSanitizesLayout()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("export.layout.latte"));

    {
        KConfig c(path);
        KConfigGroup cs = c.group(QStringLiteral("Containments"));

        KConfigGroup c1 = cs.group(QStringLiteral("1"));
        c1.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
        c1.writeEntry(QStringLiteral("layoutId"), QStringLiteral("MyLayout"));
        c1.writeEntry(QStringLiteral("isPreferredForShortcuts"), true);

        KConfigGroup applets = c1.group(QStringLiteral("Applets"));
        //! approved applet, keeps its config
        KConfigGroup a2 = applets.group(QStringLiteral("2"));
        a2.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.plasmoid"));
        a2.group(QStringLiteral("Configuration")).group(QStringLiteral("General")).writeEntry(QStringLiteral("foo"), QStringLiteral("bar"));
        //! unapproved plain applet, config must be stripped
        KConfigGroup a3 = applets.group(QStringLiteral("3"));
        a3.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.plasma.analogclock"));
        a3.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("PreloadWeight"), 42);
        //! unapproved subcontainment, registered for reset (config left in place)
        KConfigGroup a4 = applets.group(QStringLiteral("4"));
        a4.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.plasma.private.systemtray"));
        a4.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("SystrayContainmentId"), 99);

        KConfigGroup c99 = cs.group(QStringLiteral("99"));
        c99.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.plasma.private.systemtray"));
        c99.group(QStringLiteral("General")).writeEntry(QStringLiteral("someKey"), QStringLiteral("v"));

        KConfigGroup ls = c.group(QStringLiteral("LayoutSettings"));
        ls.writeEntry(QStringLiteral("lastUsedActivity"), QStringLiteral("abc"));
        ls.writeEntry(QStringLiteral("activities"), QStringList({QStringLiteral("x")}));
        ls.writeEntry(QStringLiteral("preferredForShortcutsTouched"), true);
        c.sync();
    }

    {
        KConfig c(path);
        sanitizeForTemplate(c, {QStringLiteral("org.kde.latte.plasmoid")});
    }

    KConfig c(path);
    KConfigGroup cs = c.group(QStringLiteral("Containments"));
    KConfigGroup c1 = cs.group(QStringLiteral("1"));

    QCOMPARE(c1.readEntry(QStringLiteral("layoutId"), QStringLiteral("untouched")), QString());
    QCOMPARE(c1.readEntry(QStringLiteral("isPreferredForShortcuts"), true), false);

    KConfigGroup applets = c1.group(QStringLiteral("Applets"));
    QVERIFY2(applets.group(QStringLiteral("2")).hasGroup(QStringLiteral("Configuration")), "approved applet config was stripped");
    QCOMPARE(applets.group(QStringLiteral("2")).group(QStringLiteral("Configuration")).group(QStringLiteral("General")).readEntry(QStringLiteral("foo"), QString()),
             QStringLiteral("bar"));
    QVERIFY2(applets.group(QStringLiteral("3")).groupList().isEmpty(), "unapproved applet config survived");
    QVERIFY2(!cs.group(QStringLiteral("99")).hasGroup(QStringLiteral("General")), "rejected subcontainment General survived");

    KConfigGroup ls = c.group(QStringLiteral("LayoutSettings"));
    QCOMPARE(ls.readEntry(QStringLiteral("lastUsedActivity"), QStringLiteral("untouched")), QString());
    QCOMPARE(ls.readEntry(QStringLiteral("activities"), QStringList({QStringLiteral("z")})), QStringList());
    QCOMPARE(ls.readEntry(QStringLiteral("preferredForShortcutsTouched"), true), false);
}

void StorageRoundTripTest::expectedViewScreenIdMath()
{
    const int primary = 10;
    const QList<int> secondaries = {11, 12};

    //! SingleScreenGroup on primary -> primary id
    QCOMPARE(expectedViewScreenId({SingleScreenGroup, true, 99, false, true}, primary, secondaries), primary);
    //! SingleScreenGroup off primary -> the view's own screen
    QCOMPARE(expectedViewScreenId({SingleScreenGroup, false, 12, false, true}, primary, secondaries), 12);
    //! a cloned view follows the single-screen rule regardless of group
    QCOMPARE(expectedViewScreenId({AllScreensGroup, false, 12, true, true}, primary, secondaries), 12);
    //! AllScreensGroup -> always primary
    QCOMPARE(expectedViewScreenId({AllScreensGroup, false, 12, false, true}, primary, secondaries), primary);
    //! AllSecondaryScreens, view.screen is a secondary -> keep it
    QCOMPARE(expectedViewScreenId({AllSecondaryScreensGroup, false, 12, false, true}, primary, secondaries), 12);
    //! AllSecondaryScreens, view.screen is not a secondary -> first secondary
    QCOMPARE(expectedViewScreenId({AllSecondaryScreensGroup, false, 99, false, true}, primary, secondaries), 11);
    //! AllSecondaryScreens with no secondaries -> keep view.screen
    QCOMPARE(expectedViewScreenId({AllSecondaryScreensGroup, false, 99, false, true}, primary, QList<int>()), 99);
    //! an invalid view -> no screen
    QCOMPARE(expectedViewScreenId({SingleScreenGroup, true, 10, false, false}, primary, secondaries), NOSCREENID);
}

QTEST_GUILESS_MAIN(StorageRoundTripTest)

#include "storageroundtriptest.moc"
