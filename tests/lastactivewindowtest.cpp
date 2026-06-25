/*
    SPDX-FileCopyrightText: 2026 Latte contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-object behavioral test for LastActiveWindow. The header pulls in the
// Corona/View object graph, so the production logic is linked through the
// prebuilt latte-dock application objects (minus main.cpp) and driven through
// the headers. A minimal concrete AbstractWindowInterface implements the pure
// virtuals so the real Windows -> TrackedGeneralInfo -> LastActiveWindow graph
// can be built headlessly.

#include "wm/abstractwindowinterface.h"
#include "wm/windowinfowrap.h"
#include "wm/tracker/windowstracker.h"
#include "wm/tracker/trackedgeneralinfo.h"
#include "wm/tracker/lastactivewindow.h"

#include <QObject>
#include <QSignalSpy>
#include <QtTest>

using namespace Latte::WindowSystem;

//! Concrete WM whose pure virtuals are no-ops or record the wid they were asked
//! to act on. Enough to construct the real tracker graph and observe that the
//! request* forwarders pass the current window id through.
class TestWindowInterface : public AbstractWindowInterface
{
    Q_OBJECT
public:
    explicit TestWindowInterface(QObject *parent = nullptr)
        : AbstractWindowInterface(parent) {}

    void setViewExtraFlags(QObject *, bool, Latte::Types::Visibility) override {}
    void setViewStruts(QWindow &, const QRect &, Plasma::Types::Location) override {}
    void setWindowOnActivities(const WindowId &, const QStringList &) override {}
    void removeViewStruts(QWindow &) override {}

    WindowId activeWindow() override { return WindowId(); }
    WindowInfoWrap requestInfo(WindowId wid) override { return registry.value(wid, WindowInfoWrap()); }
    WindowInfoWrap requestInfoActive() override { return WindowInfoWrap(); }

    void skipTaskBar(const QDialog &) override {}
    void slideWindow(QWindow &, Slide) override {}
    void enableBlurBehind(QWindow &) override {}
    void setActiveEdge(QWindow *, bool) override {}

    void requestActivate(WindowId wid) override { lastActivated = wid; }
    void requestClose(WindowId wid) override { lastClosed = wid; }
    void requestMoveWindow(WindowId, QPoint) override {}
    void requestToggleIsOnAllDesktops(WindowId wid) override { lastToggledAllDesktops = wid; }
    void requestToggleKeepAbove(WindowId wid) override { lastToggledKeepAbove = wid; }
    void requestToggleMinimized(WindowId wid) override { lastToggledMinimized = wid; }
    void requestToggleMaximized(WindowId wid) override { lastToggledMaximized = wid; }
    void setKeepAbove(WindowId, bool) override {}
    void setKeepBelow(WindowId, bool) override {}

    bool windowCanBeDragged(WindowId wid) override { return draggable.value(wid, false); }
    bool windowCanBeMaximized(WindowId) override { return true; }

    QIcon iconFor(WindowId) override { return QIcon(); }
    WindowId winIdFor(QString, QRect) override { return WindowId(); }
    WindowId winIdFor(QString, QString) override { return WindowId(); }
    AppData appDataFor(WindowId) override { return AppData(); }

    void switchToNextVirtualDesktop() override {}
    void switchToPreviousVirtualDesktop() override {}
    void setFrameExtents(QWindow *, const QMargins &) override {}
    void setInputMask(QWindow *, const QRect &) override {}

    WindowId lastActivated;
    WindowId lastClosed;
    WindowId lastToggledAllDesktops;
    WindowId lastToggledKeepAbove;
    WindowId lastToggledMinimized;
    WindowId lastToggledMaximized;
    QHash<WindowId, bool> draggable;

    //! Windows the tracker can resolve via requestInfo()/infoFor(). Populated so
    //! the history re-selection path (updateInformationFromHistory) can fetch
    //! valid info for retained window ids, the way a live WM would.
    QHash<WindowId, WindowInfoWrap> registry;

    //! Register a window and push it into the tracker's window map the way a live
    //! WM would, so infoFor()/the re-selection path can resolve it later.
    void announce(const WindowInfoWrap &w)
    {
        registry[w.wid()] = w;
        Q_EMIT windowAdded(w.wid());
    }
};

class LastActiveWindowTest : public QObject
{
    Q_OBJECT

private:
    //! A window that passes both isTracking() (valid, not minimized, no skip) and
    //! isShown() (valid, on-all-desktops + on-all-activities so it matches whatever
    //! activity/desktop the live WM reports).
    static WindowInfoWrap makeShownWindow(int id)
    {
        WindowInfoWrap w;
        w.setWid(QVariant(id));
        w.setIsValid(true);
        w.setIsActive(true);
        w.setIsOnAllDesktops(true);
        w.setIsOnAllActivities(true);
        w.setAppName(QStringLiteral("app-%1").arg(id));
        w.setDisplay(QStringLiteral("win-%1").arg(id));
        w.setGeometry(QRect(id, id, 100, 100));
        return w;
    }

private Q_SLOTS:
    void initTestCase();

    void initialState();
    void setInformationPopulatesGetters();
    void invalidWindowIsRejected();
    void minimizedWindowIsNotShown();
    void historyPrunesToBounds();
    void requestForwardersUseCurrentWindow();
    void canBeDraggedReflectsWm();

private:
    TestWindowInterface *m_wm{nullptr};
};

void LastActiveWindowTest::initTestCase()
{
    m_wm = new TestWindowInterface(this);
}

void LastActiveWindowTest::initialState()
{
    Tracker::TrackedGeneralInfo info(m_wm->windowsTracker());
    info.setEnabled(true);
    auto *law = info.lastActiveWindow();

    QVERIFY(law != nullptr);
    QVERIFY(!law->isValid());
    QVERIFY(law->currentWinId().isNull() || !law->currentWinId().isValid());
    QVERIFY(law->appName().isEmpty());
}

void LastActiveWindowTest::setInformationPopulatesGetters()
{
    Tracker::TrackedGeneralInfo info(m_wm->windowsTracker());
    info.setEnabled(true);
    auto *law = info.lastActiveWindow();

    QSignalSpy validSpy(law, &Tracker::LastActiveWindow::isValidChanged);
    QSignalSpy winIdSpy(law, &Tracker::LastActiveWindow::currentWinIdChanged);

    WindowInfoWrap w = makeShownWindow(7);
    w.setIsMaxVert(true);   // isMaximized() == maxVert && maxHoriz
    w.setIsMaxHoriz(true);
    law->setInformation(w);

    QVERIFY(law->isValid());
    QCOMPARE(validSpy.count(), 1);
    QCOMPARE(winIdSpy.count(), 1);
    QCOMPARE(law->currentWinId().toInt(), 7);
    QCOMPARE(law->appName(), QStringLiteral("app-7"));
    QCOMPARE(law->display(), QStringLiteral("win-7"));
    QCOMPARE(law->geometry(), QRect(7, 7, 100, 100));
    QVERIFY(law->isActive());
    QVERIFY(law->isMaximized());
    QVERIFY(law->isOnAllDesktops());
}

void LastActiveWindowTest::invalidWindowIsRejected()
{
    Tracker::TrackedGeneralInfo info(m_wm->windowsTracker());
    info.setEnabled(true);
    auto *law = info.lastActiveWindow();

    // First make it valid.
    law->setInformation(makeShownWindow(3));
    QVERIFY(law->isValid());

    // An invalid window for the same id is not tracked, gets removed from
    // history, and the (now-empty) history selection drops validity.
    WindowInfoWrap bad;
    bad.setWid(QVariant(3));
    bad.setIsValid(false);
    law->setInformation(bad);

    QVERIFY(!law->isValid());
}

void LastActiveWindowTest::minimizedWindowIsNotShown()
{
    Tracker::TrackedGeneralInfo info(m_wm->windowsTracker());
    info.setEnabled(true);
    auto *law = info.lastActiveWindow();

    WindowInfoWrap w = makeShownWindow(11);
    w.setIsMinimized(true); // minimized => isTracking() false => removed, not adopted

    law->setInformation(w);
    QVERIFY(!law->isValid());
    QVERIFY(law->currentWinId().isNull() || law->currentWinId().toInt() != 11);
}

void LastActiveWindowTest::historyPrunesToBounds()
{
    Tracker::TrackedGeneralInfo info(m_wm->windowsTracker());
    info.setEnabled(true);
    auto *law = info.lastActiveWindow();

    // Feed 30 distinct shown windows. Each is prepended; once the list passes
    // MAXHISTORY (22) it is trimmed back to PREFHISTORY (14). Each window is also
    // registered with the WM so the re-selection path can resolve retained ids.
    const int total = 30;
    for (int i = 1; i <= total; ++i) {
        WindowInfoWrap w = makeShownWindow(i);
        m_wm->announce(w);
        law->setInformation(w);
    }

    QCOMPARE(law->currentWinId().toInt(), total); // newest is current

    // Walk back through history by removing the current window each time. The
    // tracker surfaces the next retained in-history window (resolved from the WM
    // registry), so the number of windows reachable this way equals the size the
    // history was pruned to. With PREFHISTORY = 14 and the most-recent window
    // never re-pruned, no more than 14 distinct windows can ever be reachable.
    QList<int> reached;
    int guard = 0;
    while (law->isValid() && guard < total + 5) {
        int cur = law->currentWinId().toInt();
        reached.append(cur);

        QMetaObject::invokeMethod(law, "windowRemoved",
                                  Q_ARG(Latte::WindowSystem::WindowId, WindowId(cur)));
        ++guard;

        if (law->currentWinId().toInt() == cur) {
            break; // selection did not advance; avoid spinning
        }
    }

    // cleanHistory() only trims once the list passes MAXHISTORY (22): it then
    // drops back to PREFHISTORY (14) and climbs again. So the count is always
    // bounded by 22, never PREFHISTORY exactly unless you stop right at a trim.
    QVERIFY2(reached.size() <= 22,
             qPrintable(QStringLiteral("history retained %1 windows, expected <= 22").arg(reached.size())));

    // Feeding 30: window 23 trips the trim to 14 (retaining ids 23..10), then 7
    // more (24..30) prepend, leaving 21 entries, newest-first 30..10.
    QCOMPARE(reached.size(), 21);
    QCOMPARE(reached.first(), total);     // newest is current
    QCOMPARE(reached.last(), 10);         // oldest retained survivor
    QVERIFY(reached.contains(23));        // boundary window kept
    QVERIFY(!reached.contains(9));        // pruned when the trim fired
    QVERIFY(!reached.contains(1));        // earliest window long gone
}

void LastActiveWindowTest::requestForwardersUseCurrentWindow()
{
    Tracker::TrackedGeneralInfo info(m_wm->windowsTracker());
    info.setEnabled(true);
    auto *law = info.lastActiveWindow();

    law->setInformation(makeShownWindow(42));
    QCOMPARE(law->currentWinId().toInt(), 42);

    law->requestActivate();
    QCOMPARE(m_wm->lastActivated.toInt(), 42);

    law->requestClose();
    QCOMPARE(m_wm->lastClosed.toInt(), 42);

    law->requestToggleMaximized();
    QCOMPARE(m_wm->lastToggledMaximized.toInt(), 42);

    law->requestToggleMinimized();
    QCOMPARE(m_wm->lastToggledMinimized.toInt(), 42);

    law->requestToggleKeepAbove();
    QCOMPARE(m_wm->lastToggledKeepAbove.toInt(), 42);

    law->requestToggleIsOnAllDesktops();
    QCOMPARE(m_wm->lastToggledAllDesktops.toInt(), 42);
}

void LastActiveWindowTest::canBeDraggedReflectsWm()
{
    Tracker::TrackedGeneralInfo info(m_wm->windowsTracker());
    info.setEnabled(true);
    auto *law = info.lastActiveWindow();

    law->setInformation(makeShownWindow(99));
    QCOMPARE(law->currentWinId().toInt(), 99);

    m_wm->draggable[QVariant(99)] = false;
    QVERIFY(!law->canBeDragged());

    m_wm->draggable[QVariant(99)] = true;
    QVERIFY(law->canBeDragged());
}

QTEST_MAIN(LastActiveWindowTest)
#include "lastactivewindowtest.moc"
