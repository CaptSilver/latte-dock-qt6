/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Regression test for the SyncedLaunchers dangling-client crash
// (app/layouts/syncedlaunchers.cpp:56). A client QQuickItem is tracked in m_clients
// and removed through a QObject::destroyed handler. The handler used
// qobject_cast<QQuickItem*>(obj), which returns nullptr while the object is being
// destroyed (destroyed fires from ~QObject, after the QQuickItem subobject is gone),
// so the client was never removed — leaving a dangling pointer that a later
// clients()/property() call dereferenced and crashed on (SIGSEGV when pinning a
// launcher). The fix removes by pointer identity. SyncedLaunchers itself cannot be
// linked headlessly (it pulls in Corona), so the lifecycle is mirrored over a real
// QQuickItem here.

#include <QGuiApplication>
#include <QList>
#include <QObject>
#include <QQuickItem>
#include <QtTest>

class SyncedLauncherClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void qobjectCastIsNullDuringDestruction();
    void removeByPointerIdentityClearsClient();
    void qobjectCastHandlerLeavesDanglingClient();
};

void SyncedLauncherClientTest::qobjectCastIsNullDuringDestruction()
{
    // The mechanism: in a destroyed() handler the QQuickItem part is already gone.
    auto *item = new QQuickItem();
    bool castWasNull = false;
    QObject::connect(item, &QObject::destroyed, [&castWasNull](QObject *obj) {
        castWasNull = (qobject_cast<QQuickItem *>(obj) == nullptr);
    });
    delete item;
    QVERIFY2(castWasNull, "qobject_cast<QQuickItem*> must be null during destruction");
}

void SyncedLauncherClientTest::removeByPointerIdentityClearsClient()
{
    // The fix: remove by pointer identity (static_cast does not touch the object).
    QList<QQuickItem *> clients;
    auto *item = new QQuickItem();
    clients << item;
    QObject::connect(item, &QObject::destroyed, [&clients](QObject *obj) {
        clients.removeAll(static_cast<QQuickItem *>(obj));
    });
    delete item;
    QCOMPARE(clients.size(), 0);
}

void SyncedLauncherClientTest::qobjectCastHandlerLeavesDanglingClient()
{
    // Characterizes the old bug: the qobject_cast guard never fires, so the client
    // pointer lingers in the list — exactly the dangling entry that later crashed.
    QList<QQuickItem *> clients;
    auto *item = new QQuickItem();
    clients << item;
    QObject::connect(item, &QObject::destroyed, [&clients](QObject *obj) {
        if (auto *casted = qobject_cast<QQuickItem *>(obj)) {
            clients.removeAll(casted);
        }
    });
    delete item;
    QCOMPARE(clients.size(), 1);
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    SyncedLauncherClientTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "syncedlaunchersclienttest.moc"
