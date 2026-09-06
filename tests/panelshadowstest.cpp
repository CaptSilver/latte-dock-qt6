/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "panelshadows_p.h"

#include <QTemporaryDir>
#include <QWindow>
#include <QtTest>

//! QObject::receivers() is protected, so the handlers hanging off destroyed() can only be
//! counted from inside the window itself.
class ProbeWindow : public QWindow
{
public:
    int destroyedReceivers()
    {
        return receivers(SIGNAL(destroyed(QObject *)));
    }
};

//! An image set nothing resolves. hasElement() is then false for every element, so
//! Private::updateShadow() returns at its first line and no KWindowShadow is ever built.
//! The header's default prefix would not do that -- the stock Plasma theme's
//! widgets/panel-background carries shadow-left, so the whole tile path would run and the
//! receiver counts would depend on KWindowSystem internals this test does not control.
static QString unresolvableImageSet()
{
    return QStringLiteral("latte/panelshadowstest/no-such-image-set");
}

class PanelShadowsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void repeatedAddWindowInstallsOneDestroyedHandler();
    void reAddAfterRemoveInstallsTheHandlerAgain();
    void eachWindowGetsItsOwnHandler();

private:
    QTemporaryDir m_cacheHome;
};

void PanelShadowsTest::initTestCase()
{
    //! KSvg keeps its element-rect cache under the cache location; without this the test
    //! writes into the developer's own ~/.cache.
    QVERIFY(m_cacheHome.isValid());
    QVERIFY(qputenv("XDG_CACHE_HOME", m_cacheHome.path().toLocal8Bit()));

    //! Pins the premise the three cases rest on: if this image set ever starts resolving,
    //! updateShadow() stops early-returning and the receiver counts stop being ours alone.
    PanelShadows shadows(nullptr, unresolvableImageSet());
    QVERIFY(!shadows.hasElement(QStringLiteral("shadow-left")));
}

void PanelShadowsTest::repeatedAddWindowInstallsOneDestroyedHandler()
{
    PanelShadows shadows(nullptr, unresolvableImageSet());
    ProbeWindow window;
    const int baseline = window.destroyedReceivers();

    shadows.addWindow(&window, KSvg::FrameSvg::AllBorders);
    //! The handler is the only thing that evicts a dead QWindow* from the hash, so a guard
    //! sampled after the hash write -- when every window already looks registered -- would
    //! skip it on the very first call.
    QCOMPARE(window.destroyedReceivers(), baseline + 1);

    shadows.addWindow(&window, KSvg::FrameSvg::TopBorder);
    shadows.addWindow(&window, KSvg::FrameSvg::TopBorder);
    QCOMPARE(window.destroyedReceivers(), baseline + 1);
}

void PanelShadowsTest::reAddAfterRemoveInstallsTheHandlerAgain()
{
    PanelShadows shadows(nullptr, unresolvableImageSet());
    ProbeWindow window;
    const int baseline = window.destroyedReceivers();

    shadows.addWindow(&window, KSvg::FrameSvg::AllBorders);
    //! removeWindow() drops the connection along with the hash entry, so re-registering has
    //! to install a fresh handler -- Effects and every config view alternate add/remove.
    shadows.removeWindow(&window);
    QCOMPARE(window.destroyedReceivers(), baseline);

    shadows.addWindow(&window, KSvg::FrameSvg::AllBorders);
    QCOMPARE(window.destroyedReceivers(), baseline + 1);
}

void PanelShadowsTest::eachWindowGetsItsOwnHandler()
{
    PanelShadows shadows(nullptr, unresolvableImageSet());
    ProbeWindow first;
    ProbeWindow second;
    const int firstBaseline = first.destroyedReceivers();
    const int secondBaseline = second.destroyedReceivers();

    shadows.addWindow(&first, KSvg::FrameSvg::AllBorders);
    shadows.addWindow(&second, KSvg::FrameSvg::AllBorders);

    //! The guard has to ask whether THIS window is registered. Asking whether the hash holds
    //! anything at all leaves every window after the first with no handler and a dangling
    //! pointer for the next theme repaint to walk.
    QCOMPARE(first.destroyedReceivers(), firstBaseline + 1);
    QCOMPARE(second.destroyedReceivers(), secondBaseline + 1);
}

QTEST_MAIN(PanelShadowsTest)
#include "panelshadowstest.moc"
