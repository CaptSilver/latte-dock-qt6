/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>
#include <QMetaEnum>
#include "coretypes.h"

using namespace Latte;

class CoreTypesEnumTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void popupPlacementHasUsedValues();
    void popupPlacementMatchesPlasmaExtras();
    void edgePositionHasUsedValues();
};

void CoreTypesEnumTest::popupPlacementHasUsedValues()
{
    QMetaEnum me = QMetaEnum::fromType<Types::PopupPlacement>();
    QVERIFY(me.keyToValue("TopPosedLeftAlignedPopup") >= 0);
    QVERIFY(me.keyToValue("BottomPosedLeftAlignedPopup") >= 0);
    QVERIFY(me.keyToValue("LeftPosedTopAlignedPopup") >= 0);
    QVERIFY(me.keyToValue("RightPosedTopAlignedPopup") >= 0);
}

void CoreTypesEnumTest::popupPlacementMatchesPlasmaExtras()
{
    // These integers are assigned to a PlasmaExtras menu's `placement` (a C++
    // enum property), so they must equal PlasmaExtras::PopupPlacement's values.
    QMetaEnum me = QMetaEnum::fromType<Types::PopupPlacement>();
    QCOMPARE(me.keyToValue("FloatingPopup"), 0);
    QCOMPARE(me.keyToValue("TopPosedLeftAlignedPopup"), 1);
    QCOMPARE(me.keyToValue("RightPosedBottomAlignedPopup"), 8);
}

void CoreTypesEnumTest::edgePositionHasUsedValues()
{
    QMetaEnum me = QMetaEnum::fromType<Types::EdgePosition>();
    QVERIFY(me.keyToValue("TopPositioned") >= 0);
    QVERIFY(me.keyToValue("BottomPositioned") >= 0);
    QVERIFY(me.keyToValue("LeftPositioned") >= 0);
    QVERIFY(me.keyToValue("RightPositioned") >= 0);
    QVERIFY(me.keyToValue("CenterPositioned") >= 0);
}

QTEST_MAIN(CoreTypesEnumTest)
#include "coretypesenumtest.moc"
