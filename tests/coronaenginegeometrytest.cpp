/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Builds a real Latte::CoronaEngine headlessly (no init(), no live infra) with a fake
// IScreenInfo, and drives its available-screen geometry. This is the first time a
// Corona-kernel object runs under a unit test: the engine ctor is side-effect free, so
// construction stands up the collaborators with a null shell (they stay inert), and the
// geometry math runs against canned screen rects.

#include <QtTest>
#include <QPointer>
#include <QTemporaryDir>

#include <KSharedConfig>

#include "../app/coronaengine.h"
#include "../app/wm/waylandinterface.h"
#include "fakescreeninfo.h"

using namespace Latte;

class CoronaEngineGeometryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void numScreensReflectsScreenInfo();
    void screenGeometryReflectsScreenInfo();
    void availableRectWithNoViewsReturnsFullScreen();
    void availableRectUnknownScreenIsEmpty();
    void engineDoesNotOwnAnInjectedWindowInterface();
    void engineDestroysTheWindowInterfaceItCreated();

private:
    CoronaEngine::Deps depsWith(FakeScreenInfo *info)
    {
        CoronaEngine::Deps deps;
        deps.screenInfo = info;                                              //! engine builds the real WaylandInterface (inert without init())
        deps.config = KSharedConfig::openConfig(m_dir.filePath(QStringLiteral("lattetestrc")));
        return deps;
    }

    QTemporaryDir m_dir;
};

void CoronaEngineGeometryTest::numScreensReflectsScreenInfo()
{
    FakeScreenInfo info;
    info.count = 3;
    CoronaEngine engine(nullptr, depsWith(&info));   // no init() — construction must be side-effect free
    QCOMPARE(engine.numScreens(), 3);
}

void CoronaEngineGeometryTest::screenGeometryReflectsScreenInfo()
{
    FakeScreenInfo info;
    info.geometries.insert(0, QRect(0, 0, 1920, 1080));
    CoronaEngine engine(nullptr, depsWith(&info));
    QCOMPARE(engine.screenGeometry(0), QRect(0, 0, 1920, 1080));
}

void CoronaEngineGeometryTest::availableRectWithNoViewsReturnsFullScreen()
{
    FakeScreenInfo info;
    info.geometries.insert(0, QRect(0, 0, 1920, 1080));
    CoronaEngine engine(nullptr, depsWith(&info));
    // No Latte views exist (init() never ran, so the engine has no layout manager), so no
    // footprint reserves space and the full screen rect is available.
    QCOMPARE(engine.availableScreenRectWithCriteria(0), QRect(0, 0, 1920, 1080));
}

void CoronaEngineGeometryTest::availableRectUnknownScreenIsEmpty()
{
    FakeScreenInfo info;   // no geometry registered for id 5
    CoronaEngine engine(nullptr, depsWith(&info));
    QCOMPARE(engine.availableScreenRectWithCriteria(5), QRect());
}

void CoronaEngineGeometryTest::engineDoesNotOwnAnInjectedWindowInterface()
{
    FakeScreenInfo info;
    QObject owner;   // stands in for the caller that owns the window interface; heap-allocating
                     // the wm under it keeps a regression a failed assertion, not a double free
    WindowSystem::AbstractWindowInterface *wm = new WindowSystem::WaylandInterface(&owner);
    QPointer<WindowSystem::AbstractWindowInterface> alive(wm);

    {
        CoronaEngine::Deps deps = depsWith(&info);
        deps.wm = wm;
        CoronaEngine engine(nullptr, deps);
        QCOMPARE(engine.wm(), wm);
        QCOMPARE(wm->parent(), &owner);   // the engine must not adopt what it was handed
    }

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY2(!alive.isNull(), "the engine destroyed a window interface it did not create");
}

void CoronaEngineGeometryTest::engineDestroysTheWindowInterfaceItCreated()
{
    FakeScreenInfo info;
    QPointer<WindowSystem::AbstractWindowInterface> alive;

    {
        CoronaEngine engine(nullptr, depsWith(&info));   // no deps.wm — the engine builds its own
        alive = engine.wm();
        QVERIFY(!alive.isNull());
    }

    // No sendPostedEvents here on purpose: parentage is what frees it, and the child sweep
    // in ~QObject is synchronous.
    QVERIFY2(alive.isNull(), "the engine-built window interface must be reaped with the engine");
}

QTEST_MAIN(CoronaEngineGeometryTest)
#include "coronaenginegeometrytest.moc"
