/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-link unit test for the details-dialog layout-color model:
//   app/settings/detailsdialog/colorsmodel.cpp
// The model publishes the eleven built-in canvas colors to the Layout Details
// combo box. Its header pulls in lattecorona.h, so it is driven through the
// prebuilt latte-dock application objects rather than recompiled here.
//
// The interesting case is a negative row. Colors::row() answers -1 for any id
// that is not one of the eleven, DetailsHandler feeds that straight into
// QComboBox::setCurrentIndex(), and QComboBox::itemData() forwards an invalid
// index to data() without checking it -- so data() has to survive row == -1.

#include "settings/detailsdialog/colorsmodel.h"

#include <QComboBox>
#include <QModelIndex>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>

using namespace Latte::Settings::Model;

class ColorsModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void publishesTheBuiltInColors();
    void rowLookupAnswersMinusOneForUnknownIds();
    void colorPathAppendsTheCanvasFileName();
    void dataReturnsTheColorRoles();
    void dataRejectsNegativeRow();
    void comboBoxCurrentIndexMinusOneIsSurvivable();

private:
    QTemporaryDir m_dir;
    QString m_canvasPath;
};

void ColorsModelTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    //! init() only concatenates paths, so nothing has to exist on disk
    m_canvasPath = m_dir.path() + QLatin1Char('/');
}

void ColorsModelTest::publishesTheBuiltInColors()
{
    Colors colors(nullptr, m_canvasPath);

    QCOMPARE(colors.rowCount(), 11);
    QCOMPARE(colors.rowCount(QModelIndex()), 11);
    QCOMPARE(colors.columnCount(QModelIndex()), int(Colors::TEXTCOLORROLE) + 1);
}

void ColorsModelTest::rowLookupAnswersMinusOneForUnknownIds()
{
    Colors colors(nullptr, m_canvasPath);

    QCOMPARE(colors.row(QStringLiteral("blue")), 0);
    QCOMPARE(colors.row(QStringLiteral("wheat")), 10);
    QCOMPARE(colors.row(QStringLiteral("nosuchcolor")), -1);
    //! a layout file carrying "color=" reads back as empty, not as the default
    QCOMPARE(colors.row(QString()), -1);
}

void ColorsModelTest::colorPathAppendsTheCanvasFileName()
{
    Colors colors(nullptr, m_canvasPath);

    QCOMPARE(colors.colorPath(QStringLiteral("blue")), m_canvasPath + QStringLiteral("blueprint.jpg"));
}

void ColorsModelTest::dataReturnsTheColorRoles()
{
    Colors colors(nullptr, m_canvasPath);

    const QModelIndex first = colors.index(0, 0);
    QVERIFY(first.isValid());
    QCOMPARE(colors.data(first, Colors::IDROLE).toString(), QStringLiteral("blue"));
    QCOMPARE(colors.data(first, Colors::TEXTCOLORROLE).toString(), QStringLiteral("#D7E3FF"));
    QCOMPARE(colors.data(first, Colors::PATHROLE).toString(), m_canvasPath + QStringLiteral("blueprint.jpg"));
    QVERIFY(!colors.data(first, Colors::NAMEROLE).toString().isEmpty());
    QCOMPARE(colors.data(first, Qt::DisplayRole), colors.data(first, Colors::NAMEROLE));

    //! an unhandled role falls through to an empty QVariant
    QVERIFY(!colors.data(first, Qt::ToolTipRole).isValid());
}

void ColorsModelTest::dataRejectsNegativeRow()
{
    Colors colors(nullptr, m_canvasPath);

    //! a default-constructed QModelIndex has row() == -1, which is exactly what
    //! QComboBox::itemData() hands over for an out-of-range combo row
    const QModelIndex invalid;
    QCOMPARE(invalid.row(), -1);

    QVERIFY(!colors.data(invalid, Colors::IDROLE).isValid());
    QVERIFY(!colors.data(invalid, Colors::NAMEROLE).isValid());
    QVERIFY(!colors.data(invalid, Colors::PATHROLE).isValid());
    QVERIFY(!colors.data(invalid, Colors::TEXTCOLORROLE).isValid());
    QVERIFY(!colors.data(invalid, Qt::DisplayRole).isValid());
}

void ColorsModelTest::comboBoxCurrentIndexMinusOneIsSurvivable()
{
    Colors colors(nullptr, m_canvasPath);

    QComboBox cmb;
    cmb.setModel(&colors);
    QCOMPARE(cmb.count(), 11);

    //! mirrors DetailsHandler::loadLayout(): row() of an unrecognised color id
    //! goes straight to setCurrentIndex(), and the currentIndexChanged handler
    //! then reads itemData() back off that same row
    cmb.setCurrentIndex(colors.row(QStringLiteral("teal")));
    QCOMPARE(cmb.currentIndex(), -1);

    QVERIFY(cmb.itemData(-1, Colors::IDROLE).toString().isEmpty());
    QVERIFY(cmb.itemData(colors.rowCount(), Colors::IDROLE).toString().isEmpty());
}

QTEST_MAIN(ColorsModelTest)
#include "colorsmodeltest.moc"
