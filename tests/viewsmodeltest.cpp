/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-object test for the views settings model
// (app/settings/viewsdialog/viewsmodel.cpp).
//
// The model's header drags in lattecorona.h and its ctor immediately
// dereferences m_corona->screenPool() in populateScreens(), so it cannot be
// built with a null Corona. A real Latte::Corona constructs fine offscreen
// (verified: ~70ms, screenPool() valid), so we build one and drive the real
// model object through its header: data() over every role and column, the
// out-of-range / row==-1 guard (the lower-bound hole that SIGABRT'd the
// Phase-1 appletsmodel/screensmodel), setData() for each editable cell, row
// removal, temporary-view append, and the altered/new view diffing.

#include "viewsmodel.h"

#include "../app/lattecorona.h"
#include "../app/data/viewdata.h"
#include "../app/data/viewstable.h"
#include "../app/data/screendata.h"

#include <coretypes.h>

#include <Plasma/Plasma>

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QtTest>

using namespace Latte;

class ViewsModelTest : public QObject
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
    void flagsEditableColumns();
    void setDataName();
    void setDataEdgeFlipsAlignment();
    void setDataScreenGroups();
    void setDataRejectsBadIndex();
    void removeViewAndRows();
    void appendTemporaryViewSignalsAndState();
    void alteredAndNewViews();
    void rowForIdAndLookups();
    void choicesRolesReturnTables();

private:
    static Data::View makeView(const QString &id, const QString &name,
                               Plasma::Types::Location edge = Plasma::Types::BottomEdge,
                               Latte::Types::Alignment align = Latte::Types::Center);

    Latte::Corona *m_corona{nullptr};
};

Data::View ViewsModelTest::makeView(const QString &id, const QString &name,
                                    Plasma::Types::Location edge, Latte::Types::Alignment align)
{
    Data::View v(id, name);
    v.setState(Data::View::IsCreated);
    v.edge = edge;
    v.alignment = align;
    v.onPrimary = true;
    v.screensGroup = Latte::Types::SingleScreenGroup;
    return v;
}

void ViewsModelTest::initTestCase()
{
    m_corona = new Latte::Corona(false, QString(), QString(), 0, nullptr);
    QVERIFY(m_corona != nullptr);
    QVERIFY(m_corona->screenPool() != nullptr);
}

void ViewsModelTest::cleanupTestCase()
{
    // The Corona owns a large graph; deleting it headlessly can re-enter
    // teardown paths that assume a live shell, so leak it deliberately for the
    // process lifetime rather than risk an at-exit crash that masks results.
    m_corona = nullptr;
}

void ViewsModelTest::emptyByDefault()
{
    Settings::Model::Views model(nullptr, m_corona);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.rowCount(QModelIndex()), 0);
    QCOMPARE(model.columnCount(QModelIndex()), int(Settings::Model::Views::LASTCOLUMN));
    QCOMPARE(Settings::Model::Views::columnCount(), int(Settings::Model::Views::LASTCOLUMN));
    QVERIFY(!model.hasChangedData());
}

void ViewsModelTest::columnAndRoleHeaderData()
{
    Settings::Model::Views model(nullptr, m_corona);

    QCOMPARE(model.headerData(Settings::Model::Views::IDCOLUMN, Qt::Horizontal, Qt::DisplayRole).toString(),
             QStringLiteral("#"));
    QVERIFY(!model.headerData(Settings::Model::Views::NAMECOLUMN, Qt::Horizontal, Qt::DisplayRole).toString().isEmpty());

    // Bold font role round-trips a QFont.
    QVariant fontVar = model.headerData(Settings::Model::Views::NAMECOLUMN, Qt::Horizontal, Qt::FontRole);
    QVERIFY(fontVar.canConvert<QFont>());
    QVERIFY(fontVar.value<QFont>().bold());

    // Vertical orientation falls through to the base implementation, which
    // returns the 1-based section number rather than a Latte column label.
    QCOMPARE(model.headerData(0, Qt::Vertical, Qt::DisplayRole).toString(), QStringLiteral("1"));
}

void ViewsModelTest::setOriginalDataPopulatesRowsAndData()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("Dock A"));
    table << makeView(QStringLiteral("2"), QStringLiteral("Dock B"), Plasma::Types::LeftEdge, Latte::Types::Top);

    QSignalSpy insertedSpy(&model, &Settings::Model::Views::rowsInserted);
    model.setOriginalData(table);

    QCOMPARE(insertedSpy.count(), 1);
    QCOMPARE(model.rowCount(), 2);

    // Original == current right after setOriginalData -> nothing changed.
    QVERIFY(!model.hasChangedData());

    const QModelIndex name0 = model.index(0, Settings::Model::Views::NAMECOLUMN);
    QCOMPARE(model.data(name0, Qt::DisplayRole).toString(), QStringLiteral("Dock A"));
    QCOMPARE(model.data(name0, Qt::UserRole).toString(), QStringLiteral("Dock A"));

    const QModelIndex id0 = model.index(0, Settings::Model::Views::IDCOLUMN);
    QCOMPARE(model.data(id0, Qt::UserRole).toString(), QStringLiteral("1"));
    QCOMPARE(model.data(id0, Settings::Model::Views::IDROLE).toString(), QStringLiteral("1"));
    QCOMPARE(model.data(id0, Settings::Model::Views::ISACTIVEROLE).toBool(), false);

    // VIEWROLE round-trips the whole Data::View value.
    Data::View back = model.data(name0, Settings::Model::Views::VIEWROLE).value<Data::View>();
    QCOMPARE(back.id, QStringLiteral("1"));
    QCOMPARE(back.name, QStringLiteral("Dock A"));

    // HASCHANGEDVIEWROLE is false for an unmodified original.
    QCOMPARE(model.data(name0, Settings::Model::Views::HASCHANGEDVIEWROLE).toBool(), false);
}

void ViewsModelTest::dataOutOfRangeIsSafe()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("Only"));
    model.setOriginalData(table);

    // row beyond the end -> invalid, no crash.
    QVERIFY(!model.data(model.index(5, Settings::Model::Views::NAMECOLUMN), Qt::DisplayRole).isValid());

    // A default-constructed (invalid, row==-1) index must short-circuit through
    // the rowExists() guard rather than dereferencing m_viewsTable[-1]. This is
    // the lower-bound hole that SIGABRT'd the Phase-1 appletsmodel/screensmodel.
    QModelIndex invalid;
    QCOMPARE(invalid.row(), -1);
    QVERIFY(!model.data(invalid, Qt::DisplayRole).isValid());
    QVERIFY(!model.data(invalid, Settings::Model::Views::VIEWROLE).isValid());
    QVERIFY(!model.data(invalid, Settings::Model::Views::SCREENROLE).isValid());
    QVERIFY(!model.data(invalid, Settings::Model::Views::CHOICESROLE).isValid());

    // setData on an invalid / out-of-range row is rejected, not a crash.
    QVERIFY(!model.setData(invalid, QStringLiteral("x"), Qt::UserRole));
    QVERIFY(!model.setData(model.index(99, Settings::Model::Views::NAMECOLUMN), QStringLiteral("x"), Qt::UserRole));
}

void ViewsModelTest::displayRolesPerColumn()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("Bottom Center"),
                      Plasma::Types::BottomEdge, Latte::Types::Center);
    table << makeView(QStringLiteral("2"), QStringLiteral("Left Top"),
                      Plasma::Types::LeftEdge, Latte::Types::Top);
    model.setOriginalData(table);

    // Edge display text.
    QCOMPARE(model.data(model.index(0, Settings::Model::Views::EDGECOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("Bottom"));
    QCOMPARE(model.data(model.index(1, Settings::Model::Views::EDGECOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("Left"));

    // Edge UserRole is the numeric Plasma location.
    QCOMPARE(model.data(model.index(0, Settings::Model::Views::EDGECOLUMN), Qt::UserRole).toString(),
             QString::number(Plasma::Types::BottomEdge));

    // Alignment display text.
    QCOMPARE(model.data(model.index(0, Settings::Model::Views::ALIGNMENTCOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("Center"));
    QCOMPARE(model.data(model.index(1, Settings::Model::Views::ALIGNMENTCOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("Top"));

    // Screen column for an onPrimary single-screen view reads "Primary".
    QCOMPARE(model.data(model.index(0, Settings::Model::Views::SCREENCOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("Primary"));
    QCOMPARE(model.data(model.index(0, Settings::Model::Views::SCREENCOLUMN), Qt::UserRole).toString(),
             QString::number(Data::Screen::ONPRIMARYID));

    // Empty subcontainments column is an empty string, not invalid.
    QVariant sub = model.data(model.index(0, Settings::Model::Views::SUBCONTAINMENTSCOLUMN), Qt::DisplayRole);
    QVERIFY(sub.isValid());
    QCOMPARE(sub.toString(), QString());

    // SORTINGROLE produces a non-empty sortable token on the id column.
    QVERIFY(!model.data(model.index(0, Settings::Model::Views::IDCOLUMN), Settings::Model::Views::SORTINGROLE)
                 .toString().isEmpty());
}

void ViewsModelTest::flagsEditableColumns()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("V"));
    model.setOriginalData(table);

    QVERIFY(model.flags(model.index(0, Settings::Model::Views::NAMECOLUMN)).testFlag(Qt::ItemIsEditable));
    QVERIFY(model.flags(model.index(0, Settings::Model::Views::SCREENCOLUMN)).testFlag(Qt::ItemIsEditable));
    QVERIFY(model.flags(model.index(0, Settings::Model::Views::EDGECOLUMN)).testFlag(Qt::ItemIsEditable));
    QVERIFY(model.flags(model.index(0, Settings::Model::Views::ALIGNMENTCOLUMN)).testFlag(Qt::ItemIsEditable));

    // The id and subcontainments columns are not editable.
    QVERIFY(!model.flags(model.index(0, Settings::Model::Views::IDCOLUMN)).testFlag(Qt::ItemIsEditable));
    QVERIFY(!model.flags(model.index(0, Settings::Model::Views::SUBCONTAINMENTSCOLUMN)).testFlag(Qt::ItemIsEditable));
}

void ViewsModelTest::setDataName()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("Old"));
    model.setOriginalData(table);

    const QModelIndex name0 = model.index(0, Settings::Model::Views::NAMECOLUMN);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    // Same name -> no change, returns false.
    QVERIFY(!model.setData(name0, QStringLiteral("Old"), Qt::UserRole));
    QCOMPARE(changed.count(), 0);

    // New name -> applied, dataChanged fires, current diverges from original.
    model.setData(name0, QStringLiteral("New"), Qt::UserRole);
    QCOMPARE(model.data(name0, Qt::DisplayRole).toString(), QStringLiteral("New"));
    QVERIFY(changed.count() >= 1);
    QVERIFY(model.hasChangedData());
    QCOMPARE(model.data(name0, Settings::Model::Views::HASCHANGEDVIEWROLE).toBool(), true);
}

void ViewsModelTest::setDataEdgeFlipsAlignment()
{
    Settings::Model::Views model(nullptr, m_corona);

    // Bottom edge (horizontal) with a Left alignment. Moving to a vertical edge
    // must convert Left -> Top so the alignment stays meaningful.
    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("V"), Plasma::Types::BottomEdge, Latte::Types::Left);
    model.setOriginalData(table);

    const QModelIndex edge0 = model.index(0, Settings::Model::Views::EDGECOLUMN);
    QVERIFY(model.setData(edge0, QString::number(Plasma::Types::LeftEdge), Qt::UserRole));

    QCOMPARE(model.data(edge0, Qt::UserRole).toString(), QString::number(Plasma::Types::LeftEdge));
    // alignment Left should have flipped to Top on the now-vertical edge.
    QCOMPARE(model.data(model.index(0, Settings::Model::Views::ALIGNMENTCOLUMN), Qt::UserRole).toString(),
             QString::number(Latte::Types::Top));

    // Setting the same edge again returns false.
    QVERIFY(!model.setData(edge0, QString::number(Plasma::Types::LeftEdge), Qt::UserRole));
}

void ViewsModelTest::setDataScreenGroups()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("V"));
    model.setOriginalData(table);

    const QModelIndex screen0 = model.index(0, Settings::Model::Views::SCREENCOLUMN);

    // All-screens group.
    model.setData(screen0, QString::number(Data::Screen::ONALLSCREENSID), Qt::UserRole);
    QCOMPARE(model.data(screen0, Qt::UserRole).toString(), QString::number(Data::Screen::ONALLSCREENSID));
    QCOMPARE(model.data(screen0, Qt::DisplayRole).toString(), QStringLiteral("All Screens"));

    // All-secondary-screens group.
    model.setData(screen0, QString::number(Data::Screen::ONALLSECONDARYSCREENSID), Qt::UserRole);
    QCOMPARE(model.data(screen0, Qt::UserRole).toString(), QString::number(Data::Screen::ONALLSECONDARYSCREENSID));
    QCOMPARE(model.data(screen0, Qt::DisplayRole).toString(), QStringLiteral("Secondary Screens"));

    // Explicit numeric screen id.
    model.setData(screen0, QStringLiteral("7"), Qt::UserRole);
    QCOMPARE(model.data(screen0, Qt::UserRole).toString(), QStringLiteral("7"));
    // Unknown explicit screen still yields a non-empty label, not a crash.
    QVERIFY(!model.data(screen0, Qt::DisplayRole).toString().isEmpty());

    // SCREENROLE returns a Data::Screen value object.
    Data::Screen scr = model.data(screen0, Settings::Model::Views::SCREENROLE).value<Data::Screen>();
    QVERIFY(!scr.id.isEmpty());
}

void ViewsModelTest::setDataRejectsBadIndex()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("V"));
    model.setOriginalData(table);

    // Out-of-range row.
    QVERIFY(!model.setData(model.index(9, Settings::Model::Views::NAMECOLUMN), QStringLiteral("x"), Qt::UserRole));
    // The id column is below the editable range -> rejected.
    QVERIFY(!model.setData(model.index(0, Settings::Model::Views::IDCOLUMN), QStringLiteral("x"), Qt::UserRole));
    // The subcontainments column is at the upper guard boundary -> rejected.
    QVERIFY(!model.setData(model.index(0, Settings::Model::Views::SUBCONTAINMENTSCOLUMN), QStringLiteral("x"), Qt::UserRole));
}

void ViewsModelTest::removeViewAndRows()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("A"));
    table << makeView(QStringLiteral("2"), QStringLiteral("B"));
    table << makeView(QStringLiteral("3"), QStringLiteral("C"));
    model.setOriginalData(table);

    QSignalSpy removedSpy(&model, &Settings::Model::Views::rowsRemoved);

    // Remove an unknown id -> no-op, no signal.
    model.removeView(QStringLiteral("missing"));
    QCOMPARE(removedSpy.count(), 0);
    QCOMPARE(model.rowCount(), 3);

    // Remove the middle row by id.
    model.removeView(QStringLiteral("2"));
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.rowForId(QStringLiteral("2")), -1);
    QCOMPARE(model.rowForId(QStringLiteral("3")), 1);

    // removeRows with a count past the end is rejected.
    QVERIFY(!model.removeRows(1, 5));
    // removeRows with count 0 is rejected.
    QVERIFY(!model.removeRows(0, 0));
    // Valid removeRows.
    QVERIFY(model.removeRows(0, 1));
    QCOMPARE(model.rowCount(), 1);
}

void ViewsModelTest::appendTemporaryViewSignalsAndState()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("A"));
    model.setOriginalData(table);

    QSignalSpy insertedSpy(&model, &Settings::Model::Views::rowsInserted);

    Data::View temp(QStringLiteral("#tmp"), QStringLiteral("Temp"));
    temp.setState(Data::View::IsInvalid);
    model.appendTemporaryView(temp);

    QCOMPARE(insertedSpy.count(), 1);
    QCOMPARE(model.rowCount(), 2);

    // A temporary (not-IsCreated) view shows '#' in its id display.
    QCOMPARE(model.data(model.index(1, Settings::Model::Views::IDCOLUMN), Qt::DisplayRole).toString(),
             QStringLiteral("#"));

    // The temporary view is new relative to the original snapshot.
    QVERIFY(model.hasChangedData());
}

void ViewsModelTest::alteredAndNewViews()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("A"));
    table << makeView(QStringLiteral("2"), QStringLiteral("B"));
    model.setOriginalData(table);

    // No edits yet -> nothing altered, nothing new.
    QCOMPARE(model.alteredViews().rowCount(), 0);
    QCOMPARE(model.newViews().rowCount(), 0);

    // Rename row 0: it becomes altered but not new.
    model.setData(model.index(0, Settings::Model::Views::NAMECOLUMN), QStringLiteral("A2"), Qt::UserRole);
    Data::ViewsTable altered = model.alteredViews();
    QCOMPARE(altered.rowCount(), 1);
    QCOMPARE(altered[0].id, QStringLiteral("1"));
    QCOMPARE(model.newViews().rowCount(), 0);

    // Append a brand-new view: it counts as both altered and new. The table
    // rewrites the incoming id to a generated temp id (temp:N), so the new view
    // is keyed by that, not the id we passed in.
    Data::View fresh = makeView(QStringLiteral("99"), QStringLiteral("Fresh"));
    model.appendTemporaryView(fresh);
    QCOMPARE(model.newViews().rowCount(), 1);
    QVERIFY(model.newViews()[0].id.startsWith(QStringLiteral("temp:")));
    QCOMPARE(model.newViews()[0].name, QStringLiteral("Fresh"));
    QVERIFY(model.alteredViews().rowCount() >= 1);
}

void ViewsModelTest::rowForIdAndLookups()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::View withSub = makeView(QStringLiteral("10"), QStringLiteral("WithSub"));
    Data::Generic sub;
    sub.id = QStringLiteral("555");
    withSub.subcontainments << sub;

    Data::ViewsTable table;
    table << withSub;
    table << makeView(QStringLiteral("11"), QStringLiteral("Plain"));
    model.setOriginalData(table);

    QCOMPARE(model.rowForId(QStringLiteral("10")), 0);
    QCOMPARE(model.rowForId(QStringLiteral("11")), 1);
    QCOMPARE(model.rowForId(QStringLiteral("nope")), -1);

    // currentData / originalData round-trip by id; unknown id returns a default View.
    QCOMPARE(model.currentData(QStringLiteral("10")).name, QStringLiteral("WithSub"));
    QCOMPARE(model.originalData(QStringLiteral("11")).name, QStringLiteral("Plain"));
    QVERIFY(!model.currentData(QStringLiteral("nope")).isValid());

    // viewForSubContainment maps a subcontainment id back to its owning view.
    QCOMPARE(model.viewForSubContainment(QStringLiteral("555")), QStringLiteral("10"));
    QCOMPARE(model.viewForSubContainment(QStringLiteral("nosuch")), QString());

    // containsCurrentName reflects the live table.
    QVERIFY(model.containsCurrentName(QStringLiteral("Plain")));
    QVERIFY(!model.containsCurrentName(QStringLiteral("Ghost")));
}

void ViewsModelTest::choicesRolesReturnTables()
{
    Settings::Model::Views model(nullptr, m_corona);

    Data::ViewsTable table;
    table << makeView(QStringLiteral("1"), QStringLiteral("Horizontal"), Plasma::Types::BottomEdge, Latte::Types::Center);
    table << makeView(QStringLiteral("2"), QStringLiteral("Vertical"), Plasma::Types::LeftEdge, Latte::Types::Center);
    model.setOriginalData(table);

    // Edge choices: four edges always offered.
    Data::ViewsTable edges =
        model.data(model.index(0, Settings::Model::Views::EDGECOLUMN), Settings::Model::Views::CHOICESROLE)
            .value<Data::ViewsTable>();
    QCOMPARE(edges.rowCount(), 4);

    // Horizontal row offers horizontal alignment choices; vertical row offers
    // the vertical set. Both are non-empty and the model picks per-edge.
    Data::ViewsTable hAligns =
        model.data(model.index(0, Settings::Model::Views::ALIGNMENTCOLUMN), Settings::Model::Views::CHOICESROLE)
            .value<Data::ViewsTable>();
    Data::ViewsTable vAligns =
        model.data(model.index(1, Settings::Model::Views::ALIGNMENTCOLUMN), Settings::Model::Views::CHOICESROLE)
            .value<Data::ViewsTable>();
    QVERIFY(hAligns.rowCount() > 0);
    QVERIFY(vAligns.rowCount() > 0);

    // Screen choices come back as a ScreensTable with at least the default
    // primary / all-screens / all-secondary entries populateScreens() seeded.
    Data::ScreensTable screens =
        model.data(model.index(0, Settings::Model::Views::SCREENCOLUMN), Settings::Model::Views::CHOICESROLE)
            .value<Data::ScreensTable>();
    QVERIFY(screens.rowCount() >= 3);
}

QTEST_MAIN(ViewsModelTest)

#include "viewsmodeltest.moc"
