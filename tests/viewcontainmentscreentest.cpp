/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// libplasma 6.8 keeps a containment's screen as plain state that only changes when the shell calls
// setScreen(), and its ContainmentView does not do that for us. The first test checks that a view
// given a containment tells it the screen the view is on, and the second that a resize arriving
// before the view has loaded its QML does not touch a root object that does not exist yet.

#include "../app/lattecorona.h"
#include "../app/plasmascreenid.h"
#include "../app/screenpool.h"
#include "../app/view/originalview.h"
#include "../app/view/positioner.h"
#include "coronafixture.h"

#include <QResizeEvent>
#include <QScreen>
#include <QtTest>

#include <Plasma/Containment>

using namespace Latte;

class ViewContainmentScreenTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void resizeBeforeTheViewHasItsQmlIsIgnored();
    void containmentIsToldTheScreenOfTheViewItIsGivenTo();
    void containmentFollowsTheViewWhetherItIsOnPrimaryOrExplicit();

private:
    CoronaSandbox m_sandbox;
    Latte::Corona *m_corona{nullptr};
};

void ViewContainmentScreenTest::initTestCase()
{
    QVERIFY(m_sandbox.arm());
    qputenv("XDG_DATA_HOME", m_sandbox.dir.path().toUtf8());

    m_corona = buildHeadlessCorona();
    QVERIFY(m_corona->screenPool()->primaryScreen() != nullptr);
}

void ViewContainmentScreenTest::resizeBeforeTheViewHasItsQmlIsIgnored()
{
    auto *view = new OriginalView(m_corona, m_corona->screenPool()->primaryScreen(), false);
    QVERIFY(!view->rootObject());

    //! ContainmentView::resizeEvent would dereference the missing root object
    QResizeEvent resize(QSize(300, 60), QSize(100, 40));
    QCoreApplication::sendEvent(view, &resize);

    QVERIFY(!view->rootObject());
    delete view;
}

void ViewContainmentScreenTest::containmentIsToldTheScreenOfTheViewItIsGivenTo()
{
    Plasma::Containment *containment = m_corona->createContainment(QStringLiteral("org.kde.latte.containment"));
    QVERIFY(containment);
    QVERIFY2(containment->pluginMetaData().isValid(), "the Latte containment plugin did not resolve from the staged shell package");

    //! a containment that was never placed
    QCOMPARE(latteScreenId(containment->screen()), NOSCREEN);

    auto *view = new OriginalView(m_corona, m_corona->screenPool()->primaryScreen(), false);
    view->setContainment(containment);

    QCOMPARE(latteScreenId(containment->screen()), m_corona->screenPool()->primaryScreenId());
    delete view;
}

void ViewContainmentScreenTest::containmentFollowsTheViewWhetherItIsOnPrimaryOrExplicit()
{
    Plasma::Containment *containment = m_corona->createContainment(QStringLiteral("org.kde.latte.containment"));
    QVERIFY(containment);

    //! the test runs on two offscreen screens, so an explicit view can be on a screen
    //! that is not the primary one
    QScreen *primary = m_corona->screenPool()->primaryScreen();
    QScreen *other = nullptr;

    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen != primary) {
            other = screen;
        }
    }

    QVERIFY2(other, "needs the two screens of offscreen-two-screens.json");

    const int primaryId = m_corona->screenPool()->primaryScreenId();
    const int otherId = m_corona->screenPool()->id(other->name());
    QVERIFY(primaryId >= 0);
    QVERIFY(otherId >= 0);
    QVERIFY(otherId != primaryId);

    auto *view = new OriginalView(m_corona, primary, false);
    view->setContainment(containment);
    view->positioner()->setScreenToFollow(other);
    QCOMPARE(view->positioner()->currentScreenId(), otherId);

    //! a screen the view is not on, left over from an earlier placement
    setContainmentScreen(containment, 99);
    QCOMPARE(latteScreenId(containment->screen()), 99);

    //! a primary view is on whatever the primary screen is, not on the screen it follows
    view->setOnPrimary(true);
    view->positioner()->updateContainmentScreen();
    QCOMPARE(latteScreenId(containment->screen()), primaryId);

    setContainmentScreen(containment, 99);

    //! an explicit view is on the screen it follows
    view->setOnPrimary(false);
    view->positioner()->updateContainmentScreen();
    QCOMPARE(latteScreenId(containment->screen()), otherId);

    delete view;
}

QTEST_MAIN(ViewContainmentScreenTest)

#include "viewcontainmentscreentest.moc"
