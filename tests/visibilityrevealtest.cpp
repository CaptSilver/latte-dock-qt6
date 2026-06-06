/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../app/view/visibilitymanager.h"

#include <QTest>
#include <QObject>

using Latte::ViewPart::VisibilityManager;

class VisibilityRevealTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void revealsOnScreenEdge_revealingModes();
    void revealsOnScreenEdge_nonRevealingModes();
};

void VisibilityRevealTest::revealsOnScreenEdge_revealingModes()
{
    //! These modes hide the dock and must re-reveal it when the pointer reaches the screen edge,
    //! so WaylandInterface::setActiveEdge() arms the edge-ghost detector for them.
    QVERIFY(VisibilityManager::revealsOnScreenEdge(Latte::Types::AutoHide));
    QVERIFY(VisibilityManager::revealsOnScreenEdge(Latte::Types::DodgeActive));
    QVERIFY(VisibilityManager::revealsOnScreenEdge(Latte::Types::DodgeMaximized));
    QVERIFY(VisibilityManager::revealsOnScreenEdge(Latte::Types::DodgeAllWindows));
}

void VisibilityRevealTest::revealsOnScreenEdge_nonRevealingModes()
{
    //! AlwaysVisible never hides; the WindowsCanCover/GoBelow family uses a stacking-layer mechanism
    //! rather than edge reveal. The edge detector must NOT be armed for these.
    QVERIFY(!VisibilityManager::revealsOnScreenEdge(Latte::Types::AlwaysVisible));
    QVERIFY(!VisibilityManager::revealsOnScreenEdge(Latte::Types::WindowsCanCover));
    QVERIFY(!VisibilityManager::revealsOnScreenEdge(Latte::Types::WindowsGoBelow));
    QVERIFY(!VisibilityManager::revealsOnScreenEdge(Latte::Types::None));
}

QTEST_MAIN(VisibilityRevealTest)
#include "visibilityrevealtest.moc"
