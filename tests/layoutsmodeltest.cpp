/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-object test for the layouts settings model
// (app/settings/settingsdialog/layoutsmodel.cpp).
//
// The model's header drags in lattecorona.h and its ctor immediately
// dereferences m_corona->universalSettings(), m_corona->layoutsManager()->
// synchronizer() and m_corona->activitiesConsumer(), so it cannot be built
// with a null Corona. A real Latte::Corona constructs fine offscreen (the same
// approach viewsmodeltest uses), so we build one and drive the real model
// through its header: rowCount/columnCount, headerData labels and the bold font
// role, flags (checkable/editable columns), data() across every user role and
// column, the out-of-range / row==-1 guard (the lower-bound hole that SIGABRT'd
// the Phase-1 appletsmodel/screensmodel), in-memory mutation through setData
// (name, in-menu, borderless, activities, locked, id, background), duplicate
// name rejection, removeRows guards, append/remove, the applyData/resetData
// change tracking and alteredLayouts diffing.

#include "settingsdialog/layoutsmodel.h"

#include "../app/lattecorona.h"
#include "../app/layouts/manager.h"
#include "../app/data/layoutdata.h"
#include "../app/data/layoutstable.h"

#include <Plasma/Plasma>

#include <QAbstractItemModel>
#include <QFont>
#include <QSignalSpy>
#include <QtTest>

using namespace Latte;
using LModel = Settings::Model::Layouts;

class LayoutsModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void emptyByDefault();
    void columnAndRoleHeaderData();
    void setOriginalDataPopulatesRowsAndData();
    void dataOutOfRangeIsSafe();          // row==-1 and row>=rowCount: no crash, invalid
    void displayRolesPerColumn();
    void userRolesPerColumn();
    void flagsCheckableAndEditableColumns();
    void setDataName();
    void setDataNameRejectsDuplicate();
    void setDataBooleanCells();
    void setDataLockedRole();
    void setDataActivities();
    void setDataRejectsBadIndex();
    void removeLayoutAndRows();
    void appendLayoutSortsByName();
    void applyAndResetTrackChanges();
    void alteredLayoutsDiff();
    void lookupsAndCurrentData();
    void inMultipleModeToggle();

private:
    static Data::Layout makeLayout(const QString &id, const QString &name);

    Latte::Corona *m_corona{nullptr};
};

Data::Layout LayoutsModelTest::makeLayout(const QString &id, const QString &name)
{
    Data::Layout l;
    l.id = id;
    l.name = name;
    return l;
}

void LayoutsModelTest::initTestCase()
{
    m_corona = new Latte::Corona(false, QString(), QString(), 0, nullptr);
    QVERIFY(m_corona != nullptr);
    QVERIFY(m_corona->universalSettings() != nullptr);
    QVERIFY(m_corona->layoutsManager() != nullptr);
    QVERIFY(m_corona->layoutsManager()->synchronizer() != nullptr);
    QVERIFY(m_corona->activitiesConsumer() != nullptr);
}

void LayoutsModelTest::cleanupTestCase()
{
    // The Corona owns a large graph; deleting it headlessly can re-enter
    // teardown paths that assume a live shell, so leak it deliberately for the
    // process lifetime rather than risk an at-exit crash that masks results.
    m_corona = nullptr;
}

void LayoutsModelTest::emptyByDefault()
{
    LModel model(nullptr, m_corona);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.rowCount(QModelIndex()), 0);
    QCOMPARE(model.columnCount(QModelIndex()), int(LModel::ACTIVITYCOLUMN) + 1);
    QVERIFY(!model.hasChangedData());
    QVERIFY(!model.inMultipleMode());
}

void LayoutsModelTest::columnAndRoleHeaderData()
{
    LModel model(nullptr, m_corona);

    // The id column header is the literal "#path" marker.
    QCOMPARE(model.headerData(LModel::IDCOLUMN, Qt::Horizontal, Qt::DisplayRole).toString(),
             QStringLiteral("#path"));
    // Name / in-menu / borderless / activities columns carry non-empty labels.
    QVERIFY(!model.headerData(LModel::NAMECOLUMN, Qt::Horizontal, Qt::DisplayRole).toString().isEmpty());
    QVERIFY(!model.headerData(LModel::MENUCOLUMN, Qt::Horizontal, Qt::DisplayRole).toString().isEmpty());
    QVERIFY(!model.headerData(LModel::BORDERSCOLUMN, Qt::Horizontal, Qt::DisplayRole).toString().isEmpty());
    QVERIFY(!model.headerData(LModel::ACTIVITYCOLUMN, Qt::Horizontal, Qt::DisplayRole).toString().isEmpty());

    // The hidden-text and background columns report empty display strings.
    QCOMPARE(model.headerData(LModel::HIDDENTEXTCOLUMN, Qt::Horizontal, Qt::DisplayRole).toString(),
             QString());
    QCOMPARE(model.headerData(LModel::BACKGROUNDCOLUMN, Qt::Horizontal, Qt::DisplayRole).toString(),
             QString());

    // Bold font role round-trips a bold QFont.
    QVariant fontVar = model.headerData(LModel::NAMECOLUMN, Qt::Horizontal, Qt::FontRole);
    QVERIFY(fontVar.canConvert<QFont>());
    QVERIFY(fontVar.value<QFont>().bold());

    // Vertical orientation falls through to the base implementation.
    QCOMPARE(model.headerData(0, Qt::Vertical, Qt::DisplayRole).toString(), QStringLiteral("1"));
}

void LayoutsModelTest::setOriginalDataPopulatesRowsAndData()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.layout.latte"), QStringLiteral("Alpha"));
    table << makeLayout(QStringLiteral("/b.layout.latte"), QStringLiteral("Beta"));

    QSignalSpy insertedSpy(&model, &LModel::rowsInserted);
    model.setOriginalData(table);

    QCOMPARE(insertedSpy.count(), 1);
    QCOMPARE(model.rowCount(), 2);

    // Original == current right after setOriginalData -> nothing changed.
    QVERIFY(!model.hasChangedData());

    const QModelIndex name0 = model.index(0, LModel::NAMECOLUMN);
    QCOMPARE(model.data(name0, Qt::DisplayRole).toString(), QStringLiteral("Alpha"));
    QCOMPARE(model.data(name0, Qt::UserRole).toString(), QStringLiteral("Alpha"));

    const QModelIndex id0 = model.index(0, LModel::IDCOLUMN);
    QCOMPARE(model.data(id0, Qt::UserRole).toString(), QStringLiteral("/a.layout.latte"));
    QCOMPARE(model.data(id0, LModel::IDROLE).toString(), QStringLiteral("/a.layout.latte"));

    // An existing-original layout is not new and reports no per-row changes.
    QCOMPARE(model.data(name0, LModel::ISNEWLAYOUTROLE).toBool(), false);
    QCOMPARE(model.data(name0, LModel::LAYOUTHASCHANGESROLE).toBool(), false);
}

void LayoutsModelTest::dataOutOfRangeIsSafe()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/only.latte"), QStringLiteral("Only"));
    model.setOriginalData(table);

    // row beyond the end -> invalid, no crash.
    QVERIFY(!model.data(model.index(5, LModel::NAMECOLUMN), Qt::DisplayRole).isValid());

    // A default-constructed (invalid, row==-1) index must short-circuit through
    // the rowExists() guard rather than dereferencing m_layoutsTable[-1]. This is
    // the lower-bound hole that SIGABRT'd the Phase-1 appletsmodel/screensmodel.
    QModelIndex invalid;
    QCOMPARE(invalid.row(), -1);
    QVERIFY(!model.data(invalid, Qt::DisplayRole).isValid());
    QVERIFY(!model.data(invalid, LModel::IDROLE).isValid());
    QVERIFY(!model.data(invalid, LModel::ASSIGNEDACTIVITIESROLE).isValid());
    QVERIFY(!model.data(invalid, LModel::BACKGROUNDUSERROLE).isValid());
    QVERIFY(!model.data(invalid, LModel::SORTINGROLE).isValid());

    // setData on an invalid / out-of-range row is rejected, not a crash.
    QVERIFY(!model.setData(invalid, QStringLiteral("x"), Qt::UserRole));
    QVERIFY(!model.setData(model.index(99, LModel::NAMECOLUMN), QStringLiteral("x"), Qt::UserRole));
}

void LayoutsModelTest::displayRolesPerColumn()
{
    LModel model(nullptr, m_corona);

    Data::Layout a = makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    a.background = QStringLiteral("/path/to/wall.png");
    Data::LayoutsTable table;
    table << a;
    model.setOriginalData(table);

    // Name display.
    QCOMPARE(model.data(model.index(0, LModel::NAMECOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("Alpha"));
    // Background display reflects the stored background path.
    QCOMPARE(model.data(model.index(0, LModel::BACKGROUNDCOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("/path/to/wall.png"));
    // Id display.
    QCOMPARE(model.data(model.index(0, LModel::IDCOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("/a.latte"));
    // The hidden-text column is always an invalid variant.
    QVERIFY(!model.data(model.index(0, LModel::HIDDENTEXTCOLUMN), Qt::DisplayRole).isValid());

    // SORTINGROLE produces a non-empty sortable token on the name column.
    QVERIFY(!model.data(model.index(0, LModel::NAMECOLUMN), LModel::SORTINGROLE).toString().isEmpty());
    // The background-column SORTINGROLE is the plain name.
    QCOMPARE(model.data(model.index(0, LModel::BACKGROUNDCOLUMN), LModel::SORTINGROLE).toString(),
             QStringLiteral("Alpha"));
}

void LayoutsModelTest::userRolesPerColumn()
{
    LModel model(nullptr, m_corona);

    Data::Layout a = makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    a.isShownInMenu = true;
    a.hasDisabledBorders = false;
    a.activities << QStringLiteral("act-1") << QStringLiteral("act-2");
    Data::LayoutsTable table;
    table << a;
    model.setOriginalData(table);

    // Per-column UserRole reads the underlying layout fields.
    QCOMPARE(model.data(model.index(0, LModel::MENUCOLUMN), Qt::UserRole).toBool(), true);
    QCOMPARE(model.data(model.index(0, LModel::BORDERSCOLUMN), Qt::UserRole).toBool(), false);
    QCOMPARE(model.data(model.index(0, LModel::ACTIVITYCOLUMN), Qt::UserRole).toStringList(),
             (QStringList{QStringLiteral("act-1"), QStringLiteral("act-2")}));

    // ASSIGNEDACTIVITIESROLE mirrors the activities list regardless of column.
    QCOMPARE(model.data(model.index(0, LModel::IDCOLUMN), LModel::ASSIGNEDACTIVITIESROLE).toStringList(),
             (QStringList{QStringLiteral("act-1"), QStringLiteral("act-2")}));

    // INMULTIPLELAYOUTSROLE reflects the model mode (single by default).
    QCOMPARE(model.data(model.index(0, LModel::IDCOLUMN), LModel::INMULTIPLELAYOUTSROLE).toBool(), false);

    // ALLACTIVITIESSORTEDROLE always seeds the three pseudo-activity ids first.
    QStringList allSorted =
        model.data(model.index(0, LModel::ACTIVITYCOLUMN), LModel::ALLACTIVITIESSORTEDROLE).toStringList();
    QVERIFY(allSorted.contains(QLatin1String(Data::Layout::ALLACTIVITIESID)));
    QVERIFY(allSorted.contains(QLatin1String(Data::Layout::FREEACTIVITIESID)));
    QVERIFY(allSorted.contains(QLatin1String(Data::Layout::CURRENTACTIVITYID)));
}

void LayoutsModelTest::flagsCheckableAndEditableColumns()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    model.setOriginalData(table);

    // In-menu and borderless columns are user-checkable.
    QVERIFY(model.flags(model.index(0, LModel::MENUCOLUMN)).testFlag(Qt::ItemIsUserCheckable));
    QVERIFY(model.flags(model.index(0, LModel::BORDERSCOLUMN)).testFlag(Qt::ItemIsUserCheckable));

    // Name and activities columns are editable.
    QVERIFY(model.flags(model.index(0, LModel::NAMECOLUMN)).testFlag(Qt::ItemIsEditable));
    QVERIFY(model.flags(model.index(0, LModel::ACTIVITYCOLUMN)).testFlag(Qt::ItemIsEditable));

    // The id and background columns are neither checkable nor editable.
    QVERIFY(!model.flags(model.index(0, LModel::IDCOLUMN)).testFlag(Qt::ItemIsUserCheckable));
    QVERIFY(!model.flags(model.index(0, LModel::IDCOLUMN)).testFlag(Qt::ItemIsEditable));
    QVERIFY(!model.flags(model.index(0, LModel::BACKGROUNDCOLUMN)).testFlag(Qt::ItemIsEditable));
}

void LayoutsModelTest::setDataName()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Old"));
    model.setOriginalData(table);

    const QModelIndex name0 = model.index(0, LModel::NAMECOLUMN);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    // New name -> applied, dataChanged fires, current diverges from original.
    QVERIFY(model.setData(name0, QStringLiteral("New"), Qt::UserRole));
    QCOMPARE(model.data(name0, Qt::DisplayRole).toString(), QStringLiteral("New"));
    QVERIFY(changed.count() >= 1);
    QVERIFY(model.hasChangedData());
    QVERIFY(model.layoutsAreChanged());
    QCOMPARE(model.data(name0, LModel::LAYOUTHASCHANGESROLE).toBool(), true);
}

void LayoutsModelTest::setDataNameRejectsDuplicate()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    table << makeLayout(QStringLiteral("/b.latte"), QStringLiteral("Beta"));
    model.setOriginalData(table);

    QSignalSpy dup(&model, &LModel::nameDuplicated);

    // Renaming Beta to the existing "Alpha" must be rejected and signal the clash.
    const QModelIndex name1 = model.index(1, LModel::NAMECOLUMN);
    QVERIFY(!model.setData(name1, QStringLiteral("Alpha"), Qt::UserRole));
    QCOMPARE(dup.count(), 1);
    // Beta keeps its name.
    QCOMPARE(model.data(name1, Qt::DisplayRole).toString(), QStringLiteral("Beta"));

    // Renaming a row to its own current name is allowed (not a self-duplicate).
    const QModelIndex name0 = model.index(0, LModel::NAMECOLUMN);
    QVERIFY(model.setData(name0, QStringLiteral("Alpha"), Qt::UserRole));
}

void LayoutsModelTest::setDataBooleanCells()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    model.setOriginalData(table);

    // In-menu toggle.
    const QModelIndex menu0 = model.index(0, LModel::MENUCOLUMN);
    QVERIFY(model.setData(menu0, true, Qt::UserRole));
    QCOMPARE(model.data(menu0, Qt::UserRole).toBool(), true);

    // Borderless toggle.
    const QModelIndex borders0 = model.index(0, LModel::BORDERSCOLUMN);
    QVERIFY(model.setData(borders0, true, Qt::UserRole));
    QCOMPARE(model.data(borders0, Qt::UserRole).toBool(), true);

    // Background path setData stores the path on the BACKGROUNDCOLUMN.
    const QModelIndex back0 = model.index(0, LModel::BACKGROUNDCOLUMN);
    QVERIFY(model.setData(back0, QStringLiteral("/wall.png"), Qt::UserRole));
    QCOMPARE(model.data(back0, Qt::DisplayRole).toString(), QStringLiteral("/wall.png"));

    // A non-slash background value is treated as a color, clearing the path.
    QVERIFY(model.setData(back0, QStringLiteral("#ff0000"), Qt::UserRole));
    QCOMPARE(model.data(back0, Qt::DisplayRole).toString(), QString());
}

void LayoutsModelTest::setDataLockedRole()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    model.setOriginalData(table);

    // ISLOCKEDROLE is a row-wide role: any cell in the row applies it.
    const QModelIndex id0 = model.index(0, LModel::IDCOLUMN);
    QCOMPARE(model.data(id0, LModel::ISLOCKEDROLE).toBool(), false);
    QVERIFY(model.setData(id0, true, LModel::ISLOCKEDROLE));
    QCOMPARE(model.data(id0, LModel::ISLOCKEDROLE).toBool(), true);
}

void LayoutsModelTest::setDataActivities()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    model.setOriginalData(table);

    const QModelIndex act0 = model.index(0, LModel::ACTIVITYCOLUMN);
    const QStringList wanted{QStringLiteral("x"), QStringLiteral("y")};
    QVERIFY(model.setData(act0, wanted, Qt::UserRole));
    QCOMPARE(model.data(act0, Qt::UserRole).toStringList(), wanted);
    QCOMPARE(model.data(act0, LModel::ASSIGNEDACTIVITIESROLE).toStringList(), wanted);
    QVERIFY(model.hasEnabledLayout());
}

void LayoutsModelTest::setDataRejectsBadIndex()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    model.setOriginalData(table);

    // Out-of-range row.
    QVERIFY(!model.setData(model.index(9, LModel::NAMECOLUMN), QStringLiteral("x"), Qt::UserRole));
    // A column past ACTIVITYCOLUMN is rejected by the upper-bound guard.
    QVERIFY(!model.setData(model.index(0, LModel::ACTIVITYCOLUMN + 1), QStringLiteral("x"), Qt::UserRole));
}

void LayoutsModelTest::removeLayoutAndRows()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    table << makeLayout(QStringLiteral("/b.latte"), QStringLiteral("Beta"));
    table << makeLayout(QStringLiteral("/c.latte"), QStringLiteral("Gamma"));
    model.setOriginalData(table);

    QSignalSpy removedSpy(&model, &LModel::rowsRemoved);

    // Remove an unknown id -> no-op, no signal.
    model.removeLayout(QStringLiteral("/missing.latte"));
    QCOMPARE(removedSpy.count(), 0);
    QCOMPARE(model.rowCount(), 3);

    // Remove a known layout by id.
    model.removeLayout(QStringLiteral("/b.latte"));
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.rowForId(QStringLiteral("/b.latte")), -1);

    // removeRows with count past the end is rejected.
    QVERIFY(!model.removeRows(1, 5));
    // removeRows with count 0 is rejected.
    QVERIFY(!model.removeRows(0, 0));
    // removeRows with a negative first row is rejected by the rowExists guard.
    QVERIFY(!model.removeRows(-1, 1));
    // Valid removeRows.
    QVERIFY(model.removeRows(0, 1));
    QCOMPARE(model.rowCount(), 1);
}

void LayoutsModelTest::appendLayoutSortsByName()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    table << makeLayout(QStringLiteral("/c.latte"), QStringLiteral("Gamma"));
    model.setOriginalData(table);

    QSignalSpy insertedSpy(&model, &LModel::rowsInserted);

    // "Beta" sorts between Alpha and Gamma; appendLayout inserts at the sorted slot.
    model.appendLayout(makeLayout(QStringLiteral("/b.latte"), QStringLiteral("Beta")));
    QCOMPARE(insertedSpy.count(), 1);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(1, LModel::NAMECOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("Beta"));

    // The appended layout has no original entry, so it reads as new.
    QCOMPARE(model.data(model.index(1, LModel::NAMECOLUMN), LModel::ISNEWLAYOUTROLE).toBool(), true);
}

void LayoutsModelTest::applyAndResetTrackChanges()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    model.setOriginalData(table);
    QVERIFY(!model.hasChangedData());

    // Edit the name: current diverges from original.
    model.setData(model.index(0, LModel::NAMECOLUMN), QStringLiteral("Alpha2"), Qt::UserRole);
    QVERIFY(model.layoutsAreChanged());

    // applyData makes the current state the new original baseline.
    model.applyData();
    QVERIFY(!model.hasChangedData());
    QVERIFY(!model.layoutsAreChanged());

    // Edit again then resetData: the original baseline is restored.
    model.setData(model.index(0, LModel::NAMECOLUMN), QStringLiteral("Alpha3"), Qt::UserRole);
    QVERIFY(model.layoutsAreChanged());
    model.resetData();
    QVERIFY(!model.layoutsAreChanged());
    QCOMPARE(model.data(model.index(0, LModel::NAMECOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("Alpha2"));
}

void LayoutsModelTest::alteredLayoutsDiff()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    table << makeLayout(QStringLiteral("/b.latte"), QStringLiteral("Beta"));
    model.setOriginalData(table);

    // No edits yet -> nothing altered.
    QCOMPARE(model.alteredLayouts().count(), 0);

    // Rename row 0: it becomes altered.
    model.setData(model.index(0, LModel::NAMECOLUMN), QStringLiteral("Alpha2"), Qt::UserRole);
    QList<Data::Layout> altered = model.alteredLayouts();
    QCOMPARE(altered.count(), 1);
    QCOMPARE(altered.first().id, QStringLiteral("/a.latte"));

    // A freshly appended layout (no original) is also altered.
    model.appendLayout(makeLayout(QStringLiteral("/c.latte"), QStringLiteral("Gamma")));
    QVERIFY(model.alteredLayouts().count() >= 2);
}

void LayoutsModelTest::lookupsAndCurrentData()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    table << makeLayout(QStringLiteral("/b.latte"), QStringLiteral("Beta"));
    model.setOriginalData(table);

    QCOMPARE(model.rowForId(QStringLiteral("/a.latte")), 0);
    QCOMPARE(model.rowForId(QStringLiteral("/b.latte")), 1);
    QCOMPARE(model.rowForId(QStringLiteral("/nope.latte")), -1);

    // at() returns the row's layout value.
    QCOMPARE(model.at(0).name, QStringLiteral("Alpha"));

    // currentData / originalData round-trip by id.
    QCOMPARE(model.currentData(QStringLiteral("/b.latte")).name, QStringLiteral("Beta"));
    QCOMPARE(model.originalData(QStringLiteral("/a.latte")).name, QStringLiteral("Alpha"));

    // containsCurrentName reflects the live table.
    QVERIFY(model.containsCurrentName(QStringLiteral("Beta")));
    QVERIFY(!model.containsCurrentName(QStringLiteral("Ghost")));
}

void LayoutsModelTest::inMultipleModeToggle()
{
    LModel model(nullptr, m_corona);

    Data::LayoutsTable table;
    table << makeLayout(QStringLiteral("/a.latte"), QStringLiteral("Alpha"));
    model.setOriginalData(table);

    QSignalSpy modeSpy(&model, &LModel::inMultipleModeChanged);

    QVERIFY(!model.inMultipleMode());
    model.setInMultipleMode(true);
    QCOMPARE(modeSpy.count(), 1);
    QVERIFY(model.inMultipleMode());
    // Mode now differs from the (single) original baseline.
    QVERIFY(model.modeIsChanged());
    QVERIFY(model.hasChangedData());

    // Setting the same mode again is a no-op (no extra signal).
    model.setInMultipleMode(true);
    QCOMPARE(modeSpy.count(), 1);

    // The INMULTIPLELAYOUTSROLE now reports multiple mode.
    QCOMPARE(model.data(model.index(0, LModel::IDCOLUMN), LModel::INMULTIPLELAYOUTSROLE).toBool(), true);
}

QTEST_MAIN(LayoutsModelTest)

#include "layoutsmodeltest.moc"
