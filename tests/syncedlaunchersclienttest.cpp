/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Regression test for the SyncedLaunchers dangling-client crash
// (SyncedLaunchers::removeClientObject). A client QQuickItem is tracked in m_clients
// and removed through a QObject::destroyed handler. The handler used
// qobject_cast<QQuickItem*>(obj), which returns nullptr while the object is being
// destroyed (destroyed fires from ~QObject, after the QQuickItem subobject is gone),
// so the client was never removed — leaving a dangling pointer that a later
// clients()/property() call dereferenced and crashed on (SIGSEGV when pinning a
// launcher). The fix removes by pointer identity. The first three cases mirror that
// lifecycle over a bare QQuickItem, since the crash is in QObject destruction order
// rather than in SyncedLaunchers itself.
//
// The rest drive the real SyncedLaunchers, whose broadcast rules are otherwise only
// observable on a live dock: every client in the group hears addLauncher, including the
// one that sent it, while validateLaunchersOrder deliberately skips the sender. A client
// that does not offer the ability is logged and stepped over, never allowed to cut the
// broadcast short.

#include "layouts/syncedlaunchers.h"

#include <QGuiApplication>
#include <QList>
#include <QLoggingCategory>
#include <QObject>
#include <QQuickItem>
#include <QStringList>
#include <QVariant>
#include <QtTest>

//! A Latte plasmoid, as far as the syncer can tell: the ability functions are QML `function`s
//! there, which reach the metaobject as QVariant arguments.
class LauncherClient : public QQuickItem
{
    Q_OBJECT

public:
    QStringList added;
    QList<QStringList> validated;

    Q_INVOKABLE void addSyncedLauncher(QVariant group, QVariant launcher)
    {
        Q_UNUSED(group)
        added << launcher.toString();
    }

    Q_INVOKABLE void validateSyncedLaunchersOrder(QVariant group, QVariant launchers)
    {
        Q_UNUSED(group)
        validated << launchers.toStringList();
    }
};

class SyncedLauncherClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void qobjectCastIsNullDuringDestruction();
    void removeByPointerIdentityClearsClient();
    void qobjectCastHandlerLeavesDanglingClient();
    void addLauncherReachesEveryClientIncludingTheSender();
    void validateLaunchersOrderSkipsTheSender();
    void aClientWithoutTheAbilityIsLoggedAndSteppedOver();
    void aGlobalLauncherCrossesLayouts();
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

void SyncedLauncherClientTest::aGlobalLauncherCrossesLayouts()
{
    // Only a layout group is scoped to one layout. clients() matches across layouts only
    // when the name it is given is empty, so a global launcher must reach a client sitting in a
    // different layout -- passing layoutName straight through would silently confine it to its own.
    Latte::Layouts::SyncedLaunchers syncer(nullptr);

    LauncherClient sender;
    LauncherClient elsewhere;
    sender.setProperty("layoutName", QStringLiteral("Home"));
    elsewhere.setProperty("layoutName", QStringLiteral("Work"));
    sender.setProperty("clientId", 1);
    elsewhere.setProperty("clientId", 2);

    syncer.addAbilityClient(&sender);
    syncer.addAbilityClient(&elsewhere);

    syncer.addLauncher(QStringLiteral("Home"), 1, Latte::Types::GlobalLaunchers, QString(), QStringLiteral("kate.desktop"));

    QCOMPARE(sender.added, QStringList() << QStringLiteral("kate.desktop"));
    QCOMPARE(elsewhere.added, QStringList() << QStringLiteral("kate.desktop"));
}

void SyncedLauncherClientTest::addLauncherReachesEveryClientIncludingTheSender()
{
    Latte::Layouts::SyncedLaunchers syncer(nullptr);

    LauncherClient sender;
    LauncherClient peer;
    sender.setProperty("layoutName", QStringLiteral("Home"));
    peer.setProperty("layoutName", QStringLiteral("Home"));
    sender.setProperty("clientId", 1);
    peer.setProperty("clientId", 2);

    syncer.addAbilityClient(&sender);
    syncer.addAbilityClient(&peer);

    syncer.addLauncher(QStringLiteral("Home"), 1, Latte::Types::LayoutLaunchers, QString(), QStringLiteral("kate.desktop"));

    QCOMPARE(sender.added, QStringList() << QStringLiteral("kate.desktop"));
    QCOMPARE(peer.added, QStringList() << QStringLiteral("kate.desktop"));
}

void SyncedLauncherClientTest::validateLaunchersOrderSkipsTheSender()
{
    Latte::Layouts::SyncedLaunchers syncer(nullptr);

    LauncherClient sender;
    LauncherClient peer;
    sender.setProperty("layoutName", QStringLiteral("Home"));
    peer.setProperty("layoutName", QStringLiteral("Home"));
    sender.setProperty("clientId", 1);
    peer.setProperty("clientId", 2);

    syncer.addAbilityClient(&sender);
    syncer.addAbilityClient(&peer);

    const QStringList order = QStringList() << QStringLiteral("kate.desktop") << QStringLiteral("konsole.desktop");
    syncer.validateLaunchersOrder(QStringLiteral("Home"), 1, Latte::Types::LayoutLaunchers, QString(), order);

    // The sender already has this order; echoing it back is what the reordering guard exists
    // to prevent.
    QCOMPARE(sender.validated.count(), 0);
    QCOMPARE(peer.validated, QList<QStringList>() << order);
}

void SyncedLauncherClientTest::aClientWithoutTheAbilityIsLoggedAndSteppedOver()
{
    Latte::Layouts::SyncedLaunchers syncer(nullptr);

    QQuickItem mute;
    LauncherClient peer;
    mute.setProperty("layoutName", QStringLiteral("Home"));
    peer.setProperty("layoutName", QStringLiteral("Home"));
    mute.setProperty("clientId", 1);
    peer.setProperty("clientId", 2);

    syncer.addAbilityClient(&mute);
    syncer.addAbilityClient(&peer);

    // Distributions ship a qtlogging.ini with *.debug=false, and a filtered qDebug never reaches
    // a message handler at all, so the line has to be switched back on to be observable here.
    QLoggingCategory::setFilterRules(QStringLiteral("default.debug=true"));
    QTest::ignoreMessage(QtDebugMsg, "Launchers Syncer Ability: addSyncedLauncher(QVariant,QVariant) was NOT found...");

    syncer.addLauncher(QStringLiteral("Home"), 1, Latte::Types::LayoutLaunchers, QString(), QStringLiteral("kate.desktop"));

    QLoggingCategory::setFilterRules(QString());

    QCOMPARE(peer.added, QStringList() << QStringLiteral("kate.desktop"));
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    SyncedLauncherClientTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "syncedlaunchersclienttest.moc"
