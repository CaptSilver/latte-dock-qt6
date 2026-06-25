/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// LayoutManager owns the containment's applet ordering and per-applet option lists (locked-zoom,
// colorizing-blocked, scheduled-destruction). Those lists drive what the QML side stores in the
// config, so a setter that emits a *Changed on a no-op write, or fails to emit on a real change,
// silently corrupts dock layout persistence. This drives the real object (no Plasma stack needed
// for these paths) and asserts each list/property mutator behaves: it changes the observable
// value, fires its NOTIFY exactly once on a real change, and stays silent on a redundant write.
// The masquerade index helpers are pure integer logic that the drag-and-drop path relies on for
// round-tripping a target index through a fake QPoint, so they're checked end-to-end too.

#include <QtTest>
#include <QGuiApplication>
#include <QObject>
#include <QPoint>
#include <QQuickItem>
#include <QSignalSpy>

#include "layoutmanager.h"

using Latte::Containment::LayoutManager;

class LayoutManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void lockedZoomApplets_setAndSignal();
    void lockedZoomApplets_noSignalOnRedundantSet();
    void colorizingApplets_setAndSignal();
    void setOption_lockZoom_addRemove();
    void setOption_colorizing_addRemove();
    void setOption_unknownProperty_isNoOp();
    void scheduledDestruction_addRemoveAndSignal();
    void scheduledDestruction_idempotent();
    void quickItemProperties_setAndSignal();
    void quickItemProperties_noSignalOnRedundantSet();
    void masqueradedIndex_roundTrip();
    void masqueradedIndex_isMasqueradedClassification();
    void justifySplitterIdConstant();
};

void LayoutManagerTest::lockedZoomApplets_setAndSignal()
{
    LayoutManager lm(nullptr);
    QSignalSpy spy(&lm, &LayoutManager::lockedZoomAppletsChanged);
    QVERIFY(spy.isValid());

    QVERIFY(lm.lockedZoomApplets().isEmpty());

    const QList<int> applets{3, 7, 11};
    lm.requestAppletsInLockedZoom(applets);

    QCOMPARE(lm.lockedZoomApplets(), applets);
    QCOMPARE(spy.count(), 1);
}

void LayoutManagerTest::lockedZoomApplets_noSignalOnRedundantSet()
{
    LayoutManager lm(nullptr);
    const QList<int> applets{5, 9};
    lm.requestAppletsInLockedZoom(applets);

    QSignalSpy spy(&lm, &LayoutManager::lockedZoomAppletsChanged);
    // Same value again: the setter early-returns, so no NOTIFY.
    lm.requestAppletsInLockedZoom(applets);

    QCOMPARE(lm.lockedZoomApplets(), applets);
    QCOMPARE(spy.count(), 0);
}

void LayoutManagerTest::colorizingApplets_setAndSignal()
{
    LayoutManager lm(nullptr);
    QSignalSpy spy(&lm, &LayoutManager::userBlocksColorizingAppletsChanged);
    QVERIFY(spy.isValid());

    const QList<int> applets{2, 4, 8};
    lm.requestAppletsDisabledColoring(applets);

    QCOMPARE(lm.userBlocksColorizingApplets(), applets);
    QCOMPARE(spy.count(), 1);

    // Redundant write stays silent.
    lm.requestAppletsDisabledColoring(applets);
    QCOMPARE(spy.count(), 1);
}

void LayoutManagerTest::setOption_lockZoom_addRemove()
{
    LayoutManager lm(nullptr);
    QSignalSpy spy(&lm, &LayoutManager::lockedZoomAppletsChanged);

    // Enable for id 12 -> appended.
    lm.setOption(12, QStringLiteral("lockZoom"), true);
    QCOMPARE(lm.lockedZoomApplets(), QList<int>{12});
    QCOMPARE(spy.count(), 1);

    // Enabling an already-locked id is a no-op (the contains() guard).
    lm.setOption(12, QStringLiteral("lockZoom"), true);
    QCOMPARE(lm.lockedZoomApplets(), QList<int>{12});
    QCOMPARE(spy.count(), 1);

    // Add a second, then remove the first.
    lm.setOption(20, QStringLiteral("lockZoom"), true);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(lm.lockedZoomApplets(), (QList<int>{12, 20}));

    lm.setOption(12, QStringLiteral("lockZoom"), false);
    QCOMPARE(lm.lockedZoomApplets(), QList<int>{20});
    QCOMPARE(spy.count(), 3);

    // Disabling an id that isn't locked is a no-op.
    lm.setOption(999, QStringLiteral("lockZoom"), false);
    QCOMPARE(spy.count(), 3);
}

void LayoutManagerTest::setOption_colorizing_addRemove()
{
    LayoutManager lm(nullptr);
    QSignalSpy spy(&lm, &LayoutManager::userBlocksColorizingAppletsChanged);

    lm.setOption(3, QStringLiteral("userBlocksColorizing"), true);
    QCOMPARE(lm.userBlocksColorizingApplets(), QList<int>{3});
    QCOMPARE(spy.count(), 1);

    lm.setOption(3, QStringLiteral("userBlocksColorizing"), false);
    QVERIFY(lm.userBlocksColorizingApplets().isEmpty());
    QCOMPARE(spy.count(), 2);
}

void LayoutManagerTest::setOption_unknownProperty_isNoOp()
{
    LayoutManager lm(nullptr);
    QSignalSpy lockSpy(&lm, &LayoutManager::lockedZoomAppletsChanged);
    QSignalSpy colorSpy(&lm, &LayoutManager::userBlocksColorizingAppletsChanged);

    // A property name LayoutManager doesn't know must touch nothing.
    lm.setOption(7, QStringLiteral("somethingElse"), true);

    QVERIFY(lm.lockedZoomApplets().isEmpty());
    QVERIFY(lm.userBlocksColorizingApplets().isEmpty());
    QCOMPARE(lockSpy.count(), 0);
    QCOMPARE(colorSpy.count(), 0);
}

void LayoutManagerTest::scheduledDestruction_addRemoveAndSignal()
{
    LayoutManager lm(nullptr);
    QSignalSpy spy(&lm, &LayoutManager::appletsInScheduledDestructionChanged);

    QVERIFY(lm.appletsInScheduledDestruction().isEmpty());

    // With no layouts wired, appletItem(id) resolves to nullptr but the id key is still tracked.
    lm.setAppletInScheduledDestruction(42, true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(lm.appletsInScheduledDestruction(), QList<int>{42});

    lm.setAppletInScheduledDestruction(42, false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(lm.appletsInScheduledDestruction().isEmpty());
}

void LayoutManagerTest::scheduledDestruction_idempotent()
{
    LayoutManager lm(nullptr);
    QSignalSpy spy(&lm, &LayoutManager::appletsInScheduledDestructionChanged);

    lm.setAppletInScheduledDestruction(5, true);
    QCOMPARE(spy.count(), 1);

    // Re-enabling an already-scheduled id changes nothing and stays silent.
    lm.setAppletInScheduledDestruction(5, true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(lm.appletsInScheduledDestruction(), QList<int>{5});

    // Disabling an id that was never scheduled is also a no-op.
    lm.setAppletInScheduledDestruction(123, false);
    QCOMPARE(spy.count(), 1);
}

void LayoutManagerTest::quickItemProperties_setAndSignal()
{
    LayoutManager lm(nullptr);

    QQuickItem root, mainL, startL, endL, dnd, metrics;

    QSignalSpy rootSpy(&lm, &LayoutManager::rootItemChanged);
    QSignalSpy mainSpy(&lm, &LayoutManager::mainLayoutChanged);
    QSignalSpy startSpy(&lm, &LayoutManager::startLayoutChanged);
    QSignalSpy endSpy(&lm, &LayoutManager::endLayoutChanged);
    QSignalSpy dndSpy(&lm, &LayoutManager::dndSpacerChanged);
    QSignalSpy metricsSpy(&lm, &LayoutManager::metricsChanged);

    lm.setRootItem(&root);
    lm.setMainLayout(&mainL);
    lm.setStartLayout(&startL);
    lm.setEndLayout(&endL);
    lm.setDndSpacer(&dnd);
    lm.setMetrics(&metrics);

    QCOMPARE(lm.rootItem(), &root);
    QCOMPARE(lm.mainLayout(), &mainL);
    QCOMPARE(lm.startLayout(), &startL);
    QCOMPARE(lm.endLayout(), &endL);
    QCOMPARE(lm.dndSpacer(), &dnd);
    QCOMPARE(lm.metrics(), &metrics);

    QCOMPARE(rootSpy.count(), 1);
    QCOMPARE(mainSpy.count(), 1);
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(endSpy.count(), 1);
    QCOMPARE(dndSpy.count(), 1);
    QCOMPARE(metricsSpy.count(), 1);
}

void LayoutManagerTest::quickItemProperties_noSignalOnRedundantSet()
{
    LayoutManager lm(nullptr);
    QQuickItem mainL;
    lm.setMainLayout(&mainL);

    QSignalSpy mainSpy(&lm, &LayoutManager::mainLayoutChanged);
    lm.setMainLayout(&mainL); // same pointer -> no emit
    QCOMPARE(mainSpy.count(), 0);
    QCOMPARE(lm.mainLayout(), &mainL);
}

void LayoutManagerTest::masqueradedIndex_roundTrip()
{
    LayoutManager lm(nullptr);

    for (int index : {0, 1, 5, 42, 1000}) {
        QPoint p = lm.indexToMasquearadedPoint(index);
        // Encoded as a degenerate diagonal point so it can't collide with a real coordinate.
        QCOMPARE(p.x(), p.y());
        QVERIFY(lm.isMasqueradedIndex(p.x(), p.y()));
        QCOMPARE(lm.masquearadedIndex(p.x(), p.y()), index);
    }
}

void LayoutManagerTest::masqueradedIndex_isMasqueradedClassification()
{
    LayoutManager lm(nullptr);

    // Real on-screen coordinates are positive and unequal -> never classified as masqueraded.
    QVERIFY(!lm.isMasqueradedIndex(10, 20));
    QVERIFY(!lm.isMasqueradedIndex(100, 100)); // equal but well above the base threshold
    QVERIFY(!lm.isMasqueradedIndex(0, 0));

    // The base itself (index 0) is the boundary and must classify as masqueraded.
    QPoint base = lm.indexToMasquearadedPoint(0);
    QVERIFY(lm.isMasqueradedIndex(base.x(), base.y()));

    // x != y is rejected even when both are deep in the masquerade range.
    QVERIFY(!lm.isMasqueradedIndex(base.x(), base.y() - 1));
}

void LayoutManagerTest::justifySplitterIdConstant()
{
    // The justify-splitter sentinel must stay distinct from any real applet id (ids are positive)
    // and from the -1 "missing applet" marker used in restore().
    QCOMPARE(LayoutManager::JUSTIFYSPLITTERID, -10);
    QVERIFY(LayoutManager::JUSTIFYSPLITTERID < 0);
    QVERIFY(LayoutManager::JUSTIFYSPLITTERID != -1);
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    LayoutManagerTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "layoutmanagertest.moc"
