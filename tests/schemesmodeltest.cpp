/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-link unit test for the details-dialog color-scheme model:
//   app/settings/detailsdialog/schemesmodel.cpp
// The model enumerates *.colors files found under the GenericDataLocation
// color-schemes/ subdir (via Layouts::Importer::standardPathsFor), wrapping each
// in a wm/schemecolors.cpp SchemeColors. schemesmodel.cpp links transitively
// against lattecorona.h (through importer.cpp), so it is driven through the
// prebuilt latte-dock application objects rather than recompiled here. This
// exercises the genuine compiled Schemes/SchemeColors/Importer code.

#include "settings/detailsdialog/schemesmodel.h"
#include "wm/schemecolors.h"

#include <QAbstractListModel>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

using namespace Latte::Settings::Model;

class SchemesModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void enumeratesSchemesFromDataDir();
    void dataReturnsNameAndIdRoles();
    void dataReturnsColorRoles();
    void row0IsSystemColors();
    void rowLookupMatchesSchemeFile();
    void invalidIndexAndRoleYieldEmptyVariant();

private:
    QString writeColorsFile(const QString &name, const QString &body);

    QTemporaryDir m_dir;
    QString m_schemesDir;     // <tmp>/color-schemes
    QString m_aquaPath;       // Aqua.colors absolute path
    QString m_zebraPath;      // Zebra.colors absolute path
};

QString SchemesModelTest::writeColorsFile(const QString &name, const QString &body)
{
    const QString path = m_schemesDir + QStringLiteral("/") + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream out(&f);
    out << body;
    f.close();
    return path;
}

void SchemesModelTest::initTestCase()
{
    QVERIFY(m_dir.isValid());

    // Importer::standardPaths() resolves to QStandardPaths::GenericDataLocation,
    // which is derived from XDG_DATA_DIRS on Linux and re-read on each call, so a
    // temp dir placed there is enough to make the model find our .colors files.
    // Point XDG_DATA_HOME at an empty subdir too so the system color schemes don't
    // leak in and perturb the row count / ordering.
    const QString dataHome = m_dir.filePath(QStringLiteral("xdg-data-home"));
    QVERIFY(QDir().mkpath(dataHome));
    qputenv("XDG_DATA_HOME", dataHome.toLocal8Bit());
    qputenv("XDG_DATA_DIRS", m_dir.path().toLocal8Bit());

    // Point XDG_CONFIG_HOME at an empty dir so possibleSchemeFile("kdeglobals")
    // (the model's synthetic row-0 entry) finds no real ~/.config/kdeglobals and
    // resolves to an empty scheme. That keeps the row-0 SchemeColors with an empty
    // name so it sorts first and our two named schemes land at deterministic rows.
    const QString configHome = m_dir.filePath(QStringLiteral("xdg-config-home"));
    QVERIFY(QDir().mkpath(configHome));
    qputenv("XDG_CONFIG_HOME", configHome.toLocal8Bit());

    m_schemesDir = m_dir.filePath(QStringLiteral("color-schemes"));
    QVERIFY(QDir().mkpath(m_schemesDir));

    // Two synthetic schemes. Names are chosen so the case-insensitive sort order
    // is deterministic: "Aqua" before "Zebra".
    m_aquaPath = writeColorsFile(QStringLiteral("Aqua.colors"), QStringLiteral(
        "[General]\n"
        "Name=Aqua Scheme\n"
        "\n"
        "[WM]\n"
        "activeBackground=10,20,30\n"
        "activeForeground=40,50,60\n"));
    QVERIFY(!m_aquaPath.isEmpty());

    m_zebraPath = writeColorsFile(QStringLiteral("Zebra.colors"), QStringLiteral(
        "[General]\n"
        "Name=Zebra Scheme\n"
        "\n"
        "[WM]\n"
        "activeBackground=200,100,50\n"
        "activeForeground=1,2,3\n"));
    QVERIFY(!m_zebraPath.isEmpty());
}

void SchemesModelTest::enumeratesSchemesFromDataDir()
{
    Schemes model;

    // Row 0 is always the synthetic "System Colors" entry (current kdeglobals),
    // then our two .colors files: 3 rows total.
    QCOMPARE(model.rowCount(), 3);

    // rowCount with a valid parent index is 0 (it's a flat list).
    QCOMPARE(model.rowCount(model.index(0, 0)), 0);
}

void SchemesModelTest::dataReturnsNameAndIdRoles()
{
    Schemes model;

    // Sorted case-insensitively by scheme name: row 0 System Colors (empty name),
    // then "Aqua Scheme" (row 1), then "Zebra Scheme" (row 2).
    const QModelIndex aqua = model.index(1, 0);
    const QModelIndex zebra = model.index(2, 0);

    QCOMPARE(model.data(aqua, Schemes::NAMEROLE).toString(), QStringLiteral("Aqua Scheme"));
    QCOMPARE(model.data(zebra, Schemes::NAMEROLE).toString(), QStringLiteral("Zebra Scheme"));

    // DisplayRole mirrors NAMEROLE.
    QCOMPARE(model.data(aqua, Qt::DisplayRole).toString(), QStringLiteral("Aqua Scheme"));

    // IDROLE is the absolute scheme file path for non-row-0 entries.
    QCOMPARE(model.data(aqua, Schemes::IDROLE).toString(), m_aquaPath);
    QCOMPARE(model.data(zebra, Schemes::IDROLE).toString(), m_zebraPath);
}

void SchemesModelTest::dataReturnsColorRoles()
{
    Schemes model;

    const QModelIndex aqua = model.index(1, 0);

    // The color roles come straight from the parsed SchemeColors of Aqua.colors:
    // [WM]/activeForeground -> text, [WM]/activeBackground -> background.
    QCOMPARE(model.data(aqua, Schemes::TEXTCOLORROLE).value<QColor>(), QColor(40, 50, 60));
    QCOMPARE(model.data(aqua, Schemes::BACKGROUNDCOLORROLE).value<QColor>(), QColor(10, 20, 30));
}

void SchemesModelTest::row0IsSystemColors()
{
    Schemes model;

    const QModelIndex sys = model.index(0, 0);

    // Row 0 is special-cased to the localized "System Colors" label and the
    // DEFAULTSCHEMEFILE ("kdeglobals") id regardless of what scheme it wraps.
    QCOMPARE(model.data(sys, Schemes::IDROLE).toString(), QStringLiteral("kdeglobals"));
    QVERIFY(!model.data(sys, Schemes::NAMEROLE).toString().isEmpty());
}

void SchemesModelTest::rowLookupMatchesSchemeFile()
{
    Schemes model;

    // Empty id and the default "kdeglobals" id both map to row 0.
    QCOMPARE(model.row(QString()), 0);
    QCOMPARE(model.row(QStringLiteral("kdeglobals")), 0);

    // A real scheme file path resolves to its sorted row.
    QCOMPARE(model.row(m_aquaPath), 1);
    QCOMPARE(model.row(m_zebraPath), 2);

    // An unknown id resolves to -1.
    QCOMPARE(model.row(m_dir.filePath(QStringLiteral("color-schemes/Missing.colors"))), -1);
}

void SchemesModelTest::invalidIndexAndRoleYieldEmptyVariant()
{
    Schemes model;

    // Out-of-range row -> invalid QVariant.
    QVERIFY(!model.data(model.index(99, 0), Schemes::NAMEROLE).isValid());

    // An invalid QModelIndex -> invalid QVariant.
    QVERIFY(!model.data(QModelIndex(), Schemes::NAMEROLE).isValid());

    // An unhandled role on a valid index -> invalid QVariant (default switch fall-through).
    QVERIFY(!model.data(model.index(1, 0), Qt::ToolTipRole).isValid());
}

QTEST_MAIN(SchemesModelTest)
#include "schemesmodeltest.moc"
