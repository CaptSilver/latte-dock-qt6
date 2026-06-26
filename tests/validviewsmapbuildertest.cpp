/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>
#include <Plasma/Plasma>

#include "../app/layout/validviewsmapbuilder.h"

using namespace Latte::Layout;

static ViewPlacement placement(uint id, Plasma::Types::Location edge, bool onPrimary,
                               const QString &expScreen, bool expActive)
{
    ViewPlacement p;
    p.containmentId = id;
    p.edge = edge;
    p.onPrimary = onPrimary;
    p.expectedScreenName = expScreen;
    p.expectedScreenActive = expActive;
    return p;
}

class ValidViewsMapBuilderTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void onPrimaryLandsUnderPrimaryName();
    void activeNonPrimaryLandsUnderExpectedName();
    void inactiveNonPrimaryIsDropped();
    void sameScreenEdgeAccumulatesInOrder();
    void emptyInputGivesEmptyMap();
};

void ValidViewsMapBuilderTest::onPrimaryLandsUnderPrimaryName()
{
    QList<ViewPlacement> in{placement(7, Plasma::Types::BottomEdge, true, QString(), true)};
    const ViewsMap map = ValidViewsMapBuilder::build(in, QStringLiteral("HDMI-1"));
    QCOMPARE(map[QStringLiteral("HDMI-1")][Plasma::Types::BottomEdge], QList<uint>{7});
}

void ValidViewsMapBuilderTest::activeNonPrimaryLandsUnderExpectedName()
{
    QList<ViewPlacement> in{placement(3, Plasma::Types::LeftEdge, false, QStringLiteral("DP-2"), true)};
    const ViewsMap map = ValidViewsMapBuilder::build(in, QStringLiteral("HDMI-1"));
    QCOMPARE(map[QStringLiteral("DP-2")][Plasma::Types::LeftEdge], QList<uint>{3});
    QVERIFY(!map.contains(QStringLiteral("HDMI-1")));
}

void ValidViewsMapBuilderTest::inactiveNonPrimaryIsDropped()
{
    QList<ViewPlacement> in{placement(9, Plasma::Types::TopEdge, false, QStringLiteral("DP-3"), false)};
    const ViewsMap map = ValidViewsMapBuilder::build(in, QStringLiteral("HDMI-1"));
    QVERIFY(map.isEmpty());
}

void ValidViewsMapBuilderTest::sameScreenEdgeAccumulatesInOrder()
{
    QList<ViewPlacement> in{
        placement(1, Plasma::Types::BottomEdge, true, QString(), true),
        placement(2, Plasma::Types::BottomEdge, true, QString(), true)};
    const ViewsMap map = ValidViewsMapBuilder::build(in, QStringLiteral("HDMI-1"));
    QCOMPARE(map[QStringLiteral("HDMI-1")][Plasma::Types::BottomEdge], (QList<uint>{1, 2}));
}

void ValidViewsMapBuilderTest::emptyInputGivesEmptyMap()
{
    const ViewsMap map = ValidViewsMapBuilder::build({}, QStringLiteral("HDMI-1"));
    QVERIFY(map.isEmpty());
}

QTEST_MAIN(ValidViewsMapBuilderTest)
#include "validviewsmapbuildertest.moc"
