/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Pure unit test for Latte::Layouts::StorageValidator. Direct-links the validator
// and the Data value types (no Corona), driving detection over hand-built models
// and small KConfigGroup fixtures.

#include "layouts/storagevalidator.h"
#include "data/errordata.h"
#include "data/appletdata.h"
#include "data/genericdata.h"
#include "data/viewstable.h"
#include "data/viewdata.h"

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace Latte::Layouts;

class StorageValidatorTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    // A trivial metadata resolver: name == plugin id, so assertions read cleanly.
    StorageValidator::MetadataResolver resolver()
    {
        return [](const QString &pluginId) {
            Latte::Data::Applet a;
            a.id = pluginId;
            a.name = pluginId;
            return a;
        };
    }

    // A subcontainment-id resolver matching Storage's Configuration/ContainmentId
    // identity for these fixtures: reads [Configuration]SystrayContainmentId.
    std::function<int(const KConfigGroup &)> subIdResolver()
    {
        return [](const KConfigGroup &g) {
            KConfigGroup cfg = g.group(QStringLiteral("Configuration"));
            return cfg.hasKey(QStringLiteral("SystrayContainmentId"))
                       ? cfg.readEntry(QStringLiteral("SystrayContainmentId"), -1)
                       : -1;
        };
    }

private Q_SLOTS:
    void initTestCase();
    void buildFromConfigParsesContainmentsAppletsAndSubIds();
    void differentAppletsWithSameIdFlagsDuplicates();
    void appletCollidingWithContainmentIdFlagged();
    void orphanedParentAppletFlaggedWhenSubMissing();
    void orphanedSubcontainmentFlaggedWhenUnreachable();
};

void StorageValidatorTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void StorageValidatorTest::buildFromConfigParsesContainmentsAppletsAndSubIds()
{
    const QString path = m_dir.filePath(QStringLiteral("model.latte"));
    KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
    KConfigGroup conts(ptr, QStringLiteral("Containments"));

    KConfigGroup c1 = conts.group(QStringLiteral("1"));
    c1.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
    KConfigGroup a2 = c1.group(QStringLiteral("Applets")).group(QStringLiteral("2"));
    a2.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.plasmoid"));
    KConfigGroup a3 = c1.group(QStringLiteral("Applets")).group(QStringLiteral("3"));
    a3.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.plasma.private.systemtray"));
    a3.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("SystrayContainmentId"), 99);

    KConfigGroup c5 = conts.group(QStringLiteral("5"));
    c5.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.desktopcontainment"));
    ptr->sync();

    KConfig fresh(path);
    KConfigGroup containments = fresh.group(QStringLiteral("Containments"));
    StorageValidator::LayoutModel model =
        StorageValidator::buildFromConfig(containments, subIdResolver());

    // Behaviour contract: the model preserves KConfig's raw groupList() order
    // exactly (Storage's inactive branches iterate the same way; the error/warning
    // row order downstream depends on it).
    QStringList modelContIds;
    for (const auto &c : model.containments) {
        modelContIds << c.id;
    }
    QCOMPARE(modelContIds, containments.groupList());

    // Semantic assertions, looked up by id so they don't assume a traversal order.
    auto containmentById = [&](const QString &id) -> StorageValidator::ContainmentModel {
        for (const auto &c : model.containments) {
            if (c.id == id) {
                return c;
            }
        }
        return {};
    };

    StorageValidator::ContainmentModel m1 = containmentById(QStringLiteral("1"));
    QCOMPARE(m1.id, QStringLiteral("1"));
    QVERIFY(m1.isLatte);
    QCOMPARE(m1.applets.count(), 2);

    // Applet order also mirrors the raw Applets groupList().
    QStringList modelAppletIds;
    for (const auto &a : m1.applets) {
        modelAppletIds << a.id;
    }
    QCOMPARE(modelAppletIds,
             containments.group(QStringLiteral("1")).group(QStringLiteral("Applets")).groupList());

    auto appletById = [&](const StorageValidator::ContainmentModel &c, const QString &id) -> StorageValidator::AppletModel {
        for (const auto &a : c.applets) {
            if (a.id == id) {
                return a;
            }
        }
        return {};
    };
    StorageValidator::AppletModel am2 = appletById(m1, QStringLiteral("2"));
    QCOMPARE(am2.id, QStringLiteral("2"));
    QCOMPARE(am2.subContainmentId, StorageValidator::IDNULL);

    StorageValidator::AppletModel am3 = appletById(m1, QStringLiteral("3"));
    QCOMPARE(am3.id, QStringLiteral("3"));
    QCOMPARE(am3.subContainmentId, 99);

    StorageValidator::ContainmentModel m5 = containmentById(QStringLiteral("5"));
    QCOMPARE(m5.id, QStringLiteral("5"));
    QVERIFY(!m5.isLatte);
    QVERIFY(m5.applets.isEmpty());
}

void StorageValidatorTest::differentAppletsWithSameIdFlagsDuplicates()
{
    // Two containments each carry an applet with id "7" -> conflict on "7".
    StorageValidator::LayoutModel model;

    StorageValidator::ContainmentModel c1;
    c1.id = QStringLiteral("1");
    c1.pluginId = QStringLiteral("org.kde.latte.containment");
    c1.applets << StorageValidator::AppletModel{QStringLiteral("7"), QStringLiteral("plasmoidA"), -1};
    c1.applets << StorageValidator::AppletModel{QStringLiteral("8"), QStringLiteral("plasmoidB"), -1};

    StorageValidator::ContainmentModel c2;
    c2.id = QStringLiteral("2");
    c2.pluginId = QStringLiteral("org.kde.latte.containment");
    c2.applets << StorageValidator::AppletModel{QStringLiteral("7"), QStringLiteral("plasmoidC"), -1};

    model.containments << c1 << c2;

    Latte::Data::Error error;
    const bool found = StorageValidator::differentAppletsWithSameId(model, resolver(), error);

    QVERIFY(found);
    // Both occurrences of id "7" are reported, id "8" is not.
    QCOMPARE(error.information.rowCount(), 2);
    QCOMPARE(error.information[(uint)0].applet.storageId, QStringLiteral("7"));
    QCOMPARE(error.information[(uint)0].id, QStringLiteral("0"));
    QCOMPARE(error.information[(uint)1].applet.storageId, QStringLiteral("7"));
    QCOMPARE(error.information[(uint)1].id, QStringLiteral("1"));

    // No duplicates -> empty, returns false.
    StorageValidator::LayoutModel clean;
    clean.containments << c1;
    Latte::Data::Error none;
    QVERIFY(!StorageValidator::differentAppletsWithSameId(clean, resolver(), none));
    QVERIFY(none.information.isEmpty());
}

void StorageValidatorTest::appletCollidingWithContainmentIdFlagged()
{
    // Containment id "2" also appears as an applet id under containment "1".
    StorageValidator::LayoutModel model;

    StorageValidator::ContainmentModel c1;
    c1.id = QStringLiteral("1");
    c1.pluginId = QStringLiteral("org.kde.latte.containment");
    c1.applets << StorageValidator::AppletModel{QStringLiteral("2"), QStringLiteral("plasmoidA"), -1};

    StorageValidator::ContainmentModel c2;
    c2.id = QStringLiteral("2");
    c2.pluginId = QStringLiteral("org.kde.plasma.private.systemtray");

    model.containments << c1 << c2;

    Latte::Data::Warning warning;
    const bool found = StorageValidator::appletsAndContainmentsWithSameId(model, resolver(), warning);

    QVERIFY(found);
    // One row for the applet "2", one for the containment "2" (containment first
    // in the second loop iteration order): the containment row carries no applet.
    QVERIFY(warning.information.rowCount() >= 2);

    bool sawContainmentOnly = false;
    bool sawAppletRow = false;
    for (int i = 0; i < warning.information.rowCount(); ++i) {
        const auto &row = warning.information[(uint)i];
        if (row.containment.storageId == QStringLiteral("2") && row.applet.storageId.isEmpty()) {
            sawContainmentOnly = true;
        }
        if (row.applet.storageId == QStringLiteral("2")) {
            sawAppletRow = true;
        }
    }
    QVERIFY(sawContainmentOnly);
    QVERIFY(sawAppletRow);
}

void StorageValidatorTest::orphanedParentAppletFlaggedWhenSubMissing()
{
    // Applet "3" hosts subcontainment 99, but no containment 99 exists -> orphan.
    StorageValidator::LayoutModel model;
    StorageValidator::ContainmentModel c1;
    c1.id = QStringLiteral("1");
    c1.pluginId = QStringLiteral("org.kde.latte.containment");
    c1.applets << StorageValidator::AppletModel{QStringLiteral("3"), QStringLiteral("systray"), 99};
    model.containments << c1;

    Latte::Data::Error error;
    QVERIFY(StorageValidator::orphanedParentApplets(model, resolver(), error));
    QCOMPARE(error.information.rowCount(), 1);
    QCOMPARE(error.information[(uint)0].applet.storageId, QStringLiteral("3"));
    QCOMPARE(error.information[(uint)0].applet.subcontainmentId, QStringLiteral("99"));

    // Add the missing containment 99 -> no longer orphaned.
    StorageValidator::ContainmentModel c99;
    c99.id = QStringLiteral("99");
    c99.pluginId = QStringLiteral("org.kde.plasma.private.systemtray");
    model.containments << c99;

    Latte::Data::Error none;
    QVERIFY(!StorageValidator::orphanedParentApplets(model, resolver(), none));
}

void StorageValidatorTest::orphanedSubcontainmentFlaggedWhenUnreachable()
{
    // Views reach containment "1" (and its sub "99"); containment "5" is reachable
    // by nothing -> orphan warning.
    StorageValidator::LayoutModel model;
    for (const QString &id : {QStringLiteral("1"), QStringLiteral("5"), QStringLiteral("99")}) {
        StorageValidator::ContainmentModel c;
        c.id = id;
        c.pluginId = QStringLiteral("org.kde.plasma.private.systemtray");
        model.containments << c;
    }

    // Build a ViewsTable whose single view (containment 1) has subcontainment 99.
    Latte::Data::ViewsTable views;
    Latte::Data::View v;
    v.id = QStringLiteral("1");
    Latte::Data::Generic sub;
    sub.id = QStringLiteral("99");
    v.subcontainments << sub;
    views << v;

    Latte::Data::Warning warning;
    QVERIFY(StorageValidator::orphanedSubcontainments(model, views, resolver(), warning));
    QCOMPARE(warning.information.rowCount(), 1);
    QCOMPARE(warning.information[(uint)0].containment.storageId, QStringLiteral("5"));
}

QTEST_MAIN(StorageValidatorTest)

#include "storagevalidatortest.moc"
