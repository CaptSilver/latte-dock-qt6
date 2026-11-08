/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Storage::newView imports the containments of a view template into a real corona through
// importLayoutFile.
// libplasma 6.8 gives the corona no call that creates a containment with a chosen id, so the
// containments are created first, take whatever ids the corona hands out, and every id the
// file mentions (the containment an applet points at, the applets themselves) is rewritten to
// match. These tests import a file into a headless Latte::Corona and read the result back out
// of the corona's own configuration.

#include "../app/lattecorona.h"
#include "../app/layout/centrallayout.h"
#include "../app/data/viewdata.h"
#include "../app/layouts/storage.h"
#include "coronafixture.h"

#include <QSignalSpy>
#include <QtTest>

#include <KConfigGroup>
#include <KSharedConfig>

#include <Plasma/Containment>

using namespace Latte;
using Layouts::Storage;

class StorageImportTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void importedViewCarriesTheCoronasIds();
    void appletReferenceFollowsTheSubcontainmentsNewId();
    void containmentConfigurationIsCopiedUnderTheNewId();
    void desktopContainmentsOfTheFileAreLeftOut();
    void containmentAddedIsEmittedOncePerContainment();
    void importingTwiceKeepsEveryIdUnique();

private:
    //! Latte containment 1 holding an applet that points at subcontainment 99, the
    //! subcontainment, and a desktop containment that must not be imported.
    QString writeLayout(const QString &name);

    //! Imports a fresh copy of the layout file as a new view of the layout.
    Latte::Data::View newView(CentralLayout &layout, const QString &templateName);

    Plasma::Containment *containmentById(uint id) const;
    KConfigGroup coronaContainment(uint id) const;
    uint subcontainmentIdOf(Plasma::Containment *latte) const;

    CoronaSandbox m_sandbox;
    Latte::Corona *m_corona{nullptr};
    QTemporaryDir m_files;
};

void StorageImportTest::initTestCase()
{
    QVERIFY(m_sandbox.arm());
    QVERIFY(m_files.isValid());
    //! packages resolve through XDG_DATA_HOME too; keep a shell package under the real home out
    qputenv("XDG_DATA_HOME", m_sandbox.dir.path().toUtf8());

    m_corona = buildHeadlessCorona();
    QVERIFY(m_corona->layoutsManager() != nullptr);
}

QString StorageImportTest::writeLayout(const QString &name)
{
    const QString path = m_files.filePath(name);
    KSharedConfigPtr ptr = KSharedConfig::openConfig(path);
    KConfigGroup conts(ptr, QStringLiteral("Containments"));

    KConfigGroup c1 = conts.group(QStringLiteral("1"));
    c1.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.latte.containment"));
    c1.writeEntry(QStringLiteral("name"), QStringLiteral("Imported Dock"));
    c1.writeEntry(QStringLiteral("lastScreen"), 12);
    c1.group(QStringLiteral("General")).writeEntry(QStringLiteral("appletOrder"), QStringLiteral("2"));

    KConfigGroup a2 = c1.group(QStringLiteral("Applets")).group(QStringLiteral("2"));
    a2.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.example.missingapplet"));
    a2.group(QStringLiteral("Configuration")).writeEntry(QStringLiteral("SystrayContainmentId"), 99);

    KConfigGroup c5 = conts.group(QStringLiteral("5"));
    c5.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.kde.desktopcontainment"));

    KConfigGroup c99 = conts.group(QStringLiteral("99"));
    c99.writeEntry(QStringLiteral("plugin"), QStringLiteral("org.example.missingsubcontainment"));

    ptr->sync();
    return path;
}

Plasma::Containment *StorageImportTest::containmentById(uint id) const
{
    for (Plasma::Containment *containment : m_corona->containments()) {
        if (containment->id() == id) {
            return containment;
        }
    }

    return nullptr;
}

KConfigGroup StorageImportTest::coronaContainment(uint id) const
{
    return KConfigGroup(m_corona->config(), QStringLiteral("Containments")).group(QString::number(id));
}

uint StorageImportTest::subcontainmentIdOf(Plasma::Containment *latte) const
{
    const KConfigGroup applets = coronaContainment(latte->id()).group(QStringLiteral("Applets"));
    const QStringList ids = applets.groupList();

    if (ids.size() != 1) {
        return 0;
    }

    return applets.group(ids.first()).group(QStringLiteral("Configuration")).readEntry(QStringLiteral("SystrayContainmentId"), 0u);
}

Latte::Data::View StorageImportTest::newView(CentralLayout &layout, const QString &templateName)
{
    Data::View next;
    next.name = QStringLiteral("Imported Dock");
    next.screen = 12;
    next.setState(Data::View::OriginFromViewTemplate, writeLayout(templateName));
    return Storage::self()->newView(&layout, next);
}

void StorageImportTest::importedViewCarriesTheCoronasIds()
{
    CentralLayout layout(nullptr, writeLayout(QStringLiteral("ids.latte")), QStringLiteral("importids"));
    layout.setCorona(m_corona);

    const int before = m_corona->containments().size();
    const Data::View view = newView(layout, QStringLiteral("ids-template.latte"));

    QVERIFY(view.isValid());
    Plasma::Containment *created = containmentById(view.id.toUInt());
    QVERIFY2(created, "the id the view reports is not a containment of the corona");
    QVERIFY2(created->pluginMetaData().isValid(), "the Latte containment plugin did not resolve from the staged shell package");
    QVERIFY(Storage::self()->isLatteContainment(created));

    //! the Latte containment and the subcontainment, not the desktop containment
    QCOMPARE(m_corona->containments().size(), before + 2);
}

void StorageImportTest::appletReferenceFollowsTheSubcontainmentsNewId()
{
    CentralLayout layout(nullptr, writeLayout(QStringLiteral("refs.latte")), QStringLiteral("importrefs"));
    layout.setCorona(m_corona);

    const Data::View view = newView(layout, QStringLiteral("refs-template.latte"));
    QVERIFY(view.isValid());
    Plasma::Containment *latte = containmentById(view.id.toUInt());
    QVERIFY(latte);

    const uint referenced = subcontainmentIdOf(latte);
    QVERIFY2(referenced != 0, "the applet lost its subcontainment reference");

    //! the id in the applet's configuration is the id of a containment that now exists
    Plasma::Containment *sub = containmentById(referenced);
    QVERIFY2(sub, "the applet points at a containment that does not exist in the corona");
    QVERIFY(sub != latte);
    QCOMPARE(coronaContainment(sub->id()).readEntry(QStringLiteral("plugin"), QString()),
             QStringLiteral("org.example.missingsubcontainment"));
}

void StorageImportTest::containmentConfigurationIsCopiedUnderTheNewId()
{
    CentralLayout layout(nullptr, writeLayout(QStringLiteral("cfg.latte")), QStringLiteral("importcfg"));
    layout.setCorona(m_corona);

    const Data::View view = newView(layout, QStringLiteral("cfg-template.latte"));
    QVERIFY(view.isValid());

    const KConfigGroup group = coronaContainment(view.id.toUInt());
    QCOMPARE(group.readEntry(QStringLiteral("name"), QString()), QStringLiteral("Imported Dock"));
    QCOMPARE(group.readEntry(QStringLiteral("plugin"), QString()), QStringLiteral("org.kde.latte.containment"));

    //! The applet order lists applet ids, which are made unique too: the id in the list is the
    //! id of the applet group that exists under the containment.
    const QString order = group.group(QStringLiteral("General")).readEntry(QStringLiteral("appletOrder"), QString());
    QVERIFY2(!order.isEmpty() && group.group(QStringLiteral("Applets")).hasGroup(order),
             qPrintable(QStringLiteral("applet order '") + order + QStringLiteral("' names no applet")));
}

void StorageImportTest::desktopContainmentsOfTheFileAreLeftOut()
{
    CentralLayout layout(nullptr, writeLayout(QStringLiteral("desktop.latte")), QStringLiteral("importdesktop"));
    layout.setCorona(m_corona);

    QVERIFY(newView(layout, QStringLiteral("desktop-template.latte")).isValid());

    for (Plasma::Containment *containment : m_corona->containments()) {
        QVERIFY2(containment->pluginMetaData().pluginId() != QLatin1String("org.kde.desktopcontainment"),
                 "a desktop containment was imported into the dock's corona");
        QVERIFY(coronaContainment(containment->id()).readEntry(QStringLiteral("plugin"), QString()) != QLatin1String("org.kde.desktopcontainment"));
    }
}

void StorageImportTest::containmentAddedIsEmittedOncePerContainment()
{
    CentralLayout layout(nullptr, writeLayout(QStringLiteral("added.latte")), QStringLiteral("importadded"));
    layout.setCorona(m_corona);

    QSignalSpy added(m_corona, &Plasma::Corona::containmentAdded);
    //! a view is made for the containment as soon as this fires, so by then its configuration
    //! has to be readable from the corona
    QStringList namesSeen;
    connect(m_corona, &Plasma::Corona::containmentAdded, this, [this, &namesSeen](Plasma::Containment *containment) {
        namesSeen << coronaContainment(containment->id()).readEntry(QStringLiteral("name"), QString());
    });

    QVERIFY(newView(layout, QStringLiteral("added-template.latte")).isValid());

    QCOMPARE(added.count(), 2);
    QVERIFY(namesSeen.contains(QStringLiteral("Imported Dock")));
    m_corona->disconnect(this);
}

void StorageImportTest::importingTwiceKeepsEveryIdUnique()
{
    CentralLayout layout(nullptr, writeLayout(QStringLiteral("twice.latte")), QStringLiteral("importtwice"));
    layout.setCorona(m_corona);

    const Data::View first = newView(layout, QStringLiteral("twice-a.latte"));
    const Data::View second = newView(layout, QStringLiteral("twice-b.latte"));
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QVERIFY(first.id != second.id);

    //! each dock points at a subcontainment of its own
    const uint firstSub = subcontainmentIdOf(containmentById(first.id.toUInt()));
    const uint secondSub = subcontainmentIdOf(containmentById(second.id.toUInt()));
    QVERIFY(firstSub != 0);
    QVERIFY(secondSub != 0);
    QVERIFY(firstSub != secondSub);

    //! no id is used by two containments or by a containment and an applet
    QStringList containmentIds;
    QStringList appletIds;
    for (Plasma::Containment *containment : m_corona->containments()) {
        containmentIds << QString::number(containment->id());
        appletIds << coronaContainment(containment->id()).group(QStringLiteral("Applets")).groupList();
    }

    QCOMPARE(QSet<QString>(containmentIds.begin(), containmentIds.end()).size(), containmentIds.size());
    QCOMPARE(QSet<QString>(appletIds.begin(), appletIds.end()).size(), appletIds.size());

    for (const QString &id : containmentIds) {
        QVERIFY2(!appletIds.contains(id), qPrintable(QStringLiteral("id %1 names a containment and an applet").arg(id)));
    }
}

QTEST_MAIN(StorageImportTest)

#include "storageimporttest.moc"
