/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Pure unit test for Latte::Layouts::StorageValidator. Direct-links the validator
// and the Data value types (no Corona), driving detection over hand-built models
// and small KConfigGroup fixtures.

#include "layouts/storagevalidator.h"
#include "data/errordata.h"
#include "data/appletdata.h"
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

QTEST_MAIN(StorageValidatorTest)

#include "storagevalidatortest.moc"
