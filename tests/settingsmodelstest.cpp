/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-link unit test for the two settings table models:
//   app/settings/exporttemplatedialog/appletsmodel.cpp
//   app/settings/screensdialog/screensmodel.cpp
// Both are plain QAbstractTableModel subclasses built with a null parent, so we
// can hand-build a Data table, feed it through setData(), and assert the
// observable model surface (rowCount/data/row(id)/flags) plus the check-state
// toggling that the settings dialogs rely on.

#include "appletsmodel.h"
#include "screensmodel.h"

#include "appletdata.h"
#include "screendata.h"

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QtTest>

using namespace Latte;

class SettingsModelsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // Applets model
    void appletsModel_emptyByDefault();
    void appletsModel_setDataPopulatesRowsAndData();
    void appletsModel_initDefaultsSelectsNoPersonalData();
    void appletsModel_rowLookupById();
    void appletsModel_checkStateToggleAndSelectedApplets();
    void appletsModel_selectAllDeselectAllAndChangedTracking();
    void appletsModel_setDataRejectsBadIndex();

    // Screens model
    void screensModel_emptyByDefault();
    void screensModel_setDataPopulatesRowsAndData();
    void screensModel_initDefaultsClearsSelection();
    void screensModel_checkStateAndCheckedScreens();
    void screensModel_flagsForNonRemovable();
    void screensModel_sortingRolePriority();

private:
    static Data::Applet makeApplet(const QString &id, const QString &name, const QString &icon = QString());
    static Data::Screen makeScreen(const QString &id, const QString &name, bool active, bool removable);
};

Data::Applet SettingsModelsTest::makeApplet(const QString &id, const QString &name, const QString &icon)
{
    Data::Applet a;
    a.id = id;
    a.name = name;
    a.icon = icon;
    a.isSelected = false;
    return a;
}

Data::Screen SettingsModelsTest::makeScreen(const QString &id, const QString &name, bool active, bool removable)
{
    Data::Screen s;
    s.id = id;
    s.name = name;
    s.isActive = active;
    s.isRemovable = removable;
    s.isSelected = false;
    return s;
}

//
// ---- Applets model -------------------------------------------------------
//

void SettingsModelsTest::appletsModel_emptyByDefault()
{
    Settings::Model::Applets model(nullptr);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.rowCount(QModelIndex()), 0);
    QCOMPARE(model.columnCount(QModelIndex()), 1);
    // An empty model is in default state and has not changed.
    QVERIFY(model.inDefaultValues());
    QVERIFY(!model.hasChangedData());
}

void SettingsModelsTest::appletsModel_setDataPopulatesRowsAndData()
{
    Settings::Model::Applets model(nullptr);

    Data::AppletsTable table;
    table << makeApplet(QStringLiteral("org.kde.foo"), QStringLiteral("Foo"), QStringLiteral("foo-icon"));
    table << makeApplet(QStringLiteral("org.kde.bar"), QStringLiteral("Bar"), QStringLiteral("bar-icon"));

    QSignalSpy changedSpy(&model, &Settings::Model::Applets::appletsDataChanged);
    model.setData(table);

    QVERIFY(changedSpy.count() >= 1);
    QCOMPARE(model.rowCount(), 2);

    const QModelIndex idx0 = model.index(0, Settings::Model::Applets::NAMECOLUMN);
    QCOMPARE(model.data(idx0, Qt::DisplayRole).toString(), QStringLiteral("Foo"));
    QCOMPARE(model.data(idx0, Settings::Model::Applets::NAMEROLE).toString(), QStringLiteral("Foo"));
    QCOMPARE(model.data(idx0, Settings::Model::Applets::IDROLE).toString(), QStringLiteral("org.kde.foo"));
    QCOMPARE(model.data(idx0, Settings::Model::Applets::ICONROLE).toString(), QStringLiteral("foo-icon"));

    // Out-of-range rows return an invalid QVariant (the row >= rowCount guard).
    const QModelIndex oob = model.index(5, Settings::Model::Applets::NAMECOLUMN);
    QVERIFY(!model.data(oob, Qt::DisplayRole).isValid());
}

void SettingsModelsTest::appletsModel_initDefaultsSelectsNoPersonalData()
{
    // setData() calls initDefaults(), which pre-selects applets with no personal
    // data (the hardcoded list in the model). A foreign id stays unselected.
    Settings::Model::Applets model(nullptr);

    Data::AppletsTable table;
    table << makeApplet(QStringLiteral("org.kde.latte.separator"), QStringLiteral("Separator"));
    table << makeApplet(QStringLiteral("org.kde.weather"), QStringLiteral("Weather"));
    model.setData(table);

    const QModelIndex sep = model.index(0, 0);
    const QModelIndex weather = model.index(1, 0);

    QCOMPARE(model.data(sep, Qt::CheckStateRole).toInt(), int(Qt::Checked));
    QCOMPARE(model.data(sep, Settings::Model::Applets::SELECTEDROLE).toBool(), true);
    QCOMPARE(model.data(weather, Qt::CheckStateRole).toInt(), int(Qt::Unchecked));

    // The default-selected separator is part of the default value set.
    QVERIFY(model.inDefaultValues());
    QVERIFY(!model.hasChangedData());

    // selectedApplets reflects only the pre-selected separator.
    Data::AppletsTable selected = model.selectedApplets();
    QCOMPARE(selected.rowCount(), 1);
    QCOMPARE(selected[0].id, QStringLiteral("org.kde.latte.separator"));
}

void SettingsModelsTest::appletsModel_rowLookupById()
{
    Settings::Model::Applets model(nullptr);

    Data::AppletsTable table;
    table << makeApplet(QStringLiteral("a"), QStringLiteral("A"));
    table << makeApplet(QStringLiteral("b"), QStringLiteral("B"));
    table << makeApplet(QStringLiteral("c"), QStringLiteral("C"));
    model.setData(table);

    QCOMPARE(model.row(QStringLiteral("a")), 0);
    QCOMPARE(model.row(QStringLiteral("c")), 2);
    QCOMPARE(model.row(QStringLiteral("missing")), -1);
}

void SettingsModelsTest::appletsModel_checkStateToggleAndSelectedApplets()
{
    Settings::Model::Applets model(nullptr);

    Data::AppletsTable table;
    table << makeApplet(QStringLiteral("org.kde.weather"), QStringLiteral("Weather"));
    table << makeApplet(QStringLiteral("org.kde.clock"), QStringLiteral("Clock"));
    model.setData(table);

    // Nothing pre-selected (no personal-data applets in the set).
    QCOMPARE(model.selectedApplets().rowCount(), 0);

    const QModelIndex idx0 = model.index(0, 0);
    QSignalSpy changedSpy(&model, &Settings::Model::Applets::appletsDataChanged);

    QVERIFY(model.setData(idx0, int(Qt::Checked), Qt::CheckStateRole));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(model.data(idx0, Qt::CheckStateRole).toInt(), int(Qt::Checked));

    Data::AppletsTable selected = model.selectedApplets();
    QCOMPARE(selected.rowCount(), 1);
    QCOMPARE(selected[0].id, QStringLiteral("org.kde.weather"));

    // Toggling a check changes data away from the original snapshot.
    QVERIFY(model.hasChangedData());
    QVERIFY(!model.inDefaultValues());

    // Uncheck it again.
    QVERIFY(model.setData(idx0, int(Qt::Unchecked), Qt::CheckStateRole));
    QCOMPARE(model.data(idx0, Qt::CheckStateRole).toInt(), int(Qt::Unchecked));
    QCOMPARE(model.selectedApplets().rowCount(), 0);
}

void SettingsModelsTest::appletsModel_selectAllDeselectAllAndChangedTracking()
{
    Settings::Model::Applets model(nullptr);

    Data::AppletsTable table;
    table << makeApplet(QStringLiteral("org.kde.weather"), QStringLiteral("Weather"));
    table << makeApplet(QStringLiteral("org.kde.clock"), QStringLiteral("Clock"));
    table << makeApplet(QStringLiteral("org.kde.notes"), QStringLiteral("Notes"));
    model.setData(table);

    model.selectAll();
    QCOMPARE(model.selectedApplets().rowCount(), 3);

    model.deselectAll();
    QCOMPARE(model.selectedApplets().rowCount(), 0);

    // reset() restores the original snapshot (all unselected here).
    model.selectAll();
    QCOMPARE(model.selectedApplets().rowCount(), 3);
    model.reset();
    QCOMPARE(model.selectedApplets().rowCount(), 0);
    QVERIFY(model.inDefaultValues());
}

void SettingsModelsTest::appletsModel_setDataRejectsBadIndex()
{
    Settings::Model::Applets model(nullptr);

    Data::AppletsTable table;
    table << makeApplet(QStringLiteral("x"), QStringLiteral("X"));
    model.setData(table);

    // Bad row -> rejected.
    QVERIFY(!model.setData(model.index(9, 0), int(Qt::Checked), Qt::CheckStateRole));
    // Valid row but unsupported role -> rejected.
    QVERIFY(!model.setData(model.index(0, 0), QStringLiteral("hi"), Settings::Model::Applets::NAMEROLE));
}

//
// ---- Screens model -------------------------------------------------------
//

void SettingsModelsTest::screensModel_emptyByDefault()
{
    Settings::Model::Screens model(nullptr);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(QModelIndex()), 1);
    QVERIFY(!model.hasChecked());
    QVERIFY(model.inDefaultValues());
}

void SettingsModelsTest::screensModel_setDataPopulatesRowsAndData()
{
    Settings::Model::Screens model(nullptr);

    Data::ScreensTable table;
    table << makeScreen(QStringLiteral("0"), QStringLiteral("Primary"), true, false);
    table << makeScreen(QStringLiteral("1"), QStringLiteral("HDMI-1"), false, true);

    QSignalSpy changedSpy(&model, &Settings::Model::Screens::screenDataChanged);
    model.setData(table);

    QVERIFY(changedSpy.count() >= 1);
    QCOMPARE(model.rowCount(), 2);

    const QModelIndex idx0 = model.index(0, Settings::Model::Screens::SCREENCOLUMN);
    QCOMPARE(model.data(idx0, Settings::Model::Screens::IDROLE).toString(), QStringLiteral("0"));
    QCOMPARE(model.data(idx0, Settings::Model::Screens::ISSCREENACTIVEROLE).toBool(), true);
    QCOMPARE(model.data(idx0, Qt::DisplayRole).toString(), QStringLiteral("{0} Primary"));

    // SCREENDATAROLE round-trips a full Data::Screen value.
    const QModelIndex idx1 = model.index(1, 0);
    Data::Screen back = model.data(idx1, Settings::Model::Screens::SCREENDATAROLE).value<Data::Screen>();
    QCOMPARE(back.id, QStringLiteral("1"));
    QCOMPARE(back.name, QStringLiteral("HDMI-1"));
    QCOMPARE(back.isRemovable, true);

    // Out-of-range row guard.
    QVERIFY(!model.data(model.index(7, 0), Qt::DisplayRole).isValid());
}

void SettingsModelsTest::screensModel_initDefaultsClearsSelection()
{
    // Even if the incoming table has isSelected set, initDefaults() clears it.
    Settings::Model::Screens model(nullptr);

    Data::ScreensTable table;
    Data::Screen s = makeScreen(QStringLiteral("1"), QStringLiteral("HDMI-1"), false, true);
    s.isSelected = true;
    table << s;
    model.setData(table);

    QCOMPARE(model.data(model.index(0, 0), Qt::CheckStateRole).toInt(), int(Qt::Unchecked));
    QVERIFY(!model.hasChecked());

    QCOMPARE(model.row(QStringLiteral("1")), 0);
    QCOMPARE(model.row(QStringLiteral("nope")), -1);
}

void SettingsModelsTest::screensModel_checkStateAndCheckedScreens()
{
    Settings::Model::Screens model(nullptr);

    Data::ScreensTable table;
    table << makeScreen(QStringLiteral("0"), QStringLiteral("Primary"), true, false);   // active, not removable
    table << makeScreen(QStringLiteral("1"), QStringLiteral("HDMI-1"), false, true);    // removable
    table << makeScreen(QStringLiteral("2"), QStringLiteral("DP-1"), false, true);      // removable
    model.setData(table);

    QSignalSpy changedSpy(&model, &Settings::Model::Screens::screenDataChanged);

    // Check the two removable screens plus the active one.
    QVERIFY(model.setData(model.index(0, 0), int(Qt::Checked), Qt::CheckStateRole));
    QVERIFY(model.setData(model.index(1, 0), int(Qt::Checked), Qt::CheckStateRole));
    QVERIFY(model.setData(model.index(2, 0), int(Qt::Checked), Qt::CheckStateRole));
    QCOMPARE(changedSpy.count(), 3);

    QVERIFY(model.hasChecked());

    // checkedScreens() returns only non-active selected screens, so the active
    // primary is excluded even though it is selected.
    Data::ScreensTable checked = model.checkedScreens();
    QCOMPARE(checked.rowCount(), 2);
    QVERIFY(checked.containsId(QStringLiteral("1")));
    QVERIFY(checked.containsId(QStringLiteral("2")));
    QVERIFY(!checked.containsId(QStringLiteral("0")));

    // deselectAll clears every checkbox.
    model.deselectAll();
    QVERIFY(!model.hasChecked());
    QCOMPARE(model.checkedScreens().rowCount(), 0);
}

void SettingsModelsTest::screensModel_flagsForNonRemovable()
{
    Settings::Model::Screens model(nullptr);

    Data::ScreensTable table;
    table << makeScreen(QStringLiteral("0"), QStringLiteral("Primary"), true, false);  // not removable
    table << makeScreen(QStringLiteral("1"), QStringLiteral("HDMI-1"), false, true);   // removable
    model.setData(table);

    // Removable rows are user-checkable.
    Qt::ItemFlags removable = model.flags(model.index(1, 0));
    QVERIFY(removable.testFlag(Qt::ItemIsUserCheckable));

    // Non-removable rows drop selectable/editable and are not user-checkable.
    Qt::ItemFlags fixed = model.flags(model.index(0, 0));
    QVERIFY(!fixed.testFlag(Qt::ItemIsUserCheckable));
    QVERIFY(!fixed.testFlag(Qt::ItemIsSelectable));
}

void SettingsModelsTest::screensModel_sortingRolePriority()
{
    Settings::Model::Screens model(nullptr);

    Data::ScreensTable table;
    table << makeScreen(QStringLiteral("0"), QStringLiteral("Primary"), true, false);   // active -> HIGHEST
    table << makeScreen(QStringLiteral("1"), QStringLiteral("Fixed"), false, false);    // not removable -> HIGH
    table << makeScreen(QStringLiteral("2"), QStringLiteral("Normal"), false, true);    // removable -> NORMAL
    model.setData(table);

    const QString sActive = model.data(model.index(0, 0), Settings::Model::Screens::SORTINGROLE).toString();
    const QString sFixed = model.data(model.index(1, 0), Settings::Model::Screens::SORTINGROLE).toString();
    const QString sNormal = model.data(model.index(2, 0), Settings::Model::Screens::SORTINGROLE).toString();

    // Sorting strings are zero-padded priority + reversed id; higher priority
    // sorts after lower lexicographically, so active > fixed > normal.
    QVERIFY(sActive > sFixed);
    QVERIFY(sFixed > sNormal);
}

QTEST_MAIN(SettingsModelsTest)

#include "settingsmodelstest.moc"
