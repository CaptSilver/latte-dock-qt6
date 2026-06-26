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
#include <QTemporaryDir>

#include <KSharedConfig>

#include "../app/coronaengine.h"
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

QTEST_MAIN(CoronaEngineGeometryTest)
#include "coronaenginegeometrytest.moc"
