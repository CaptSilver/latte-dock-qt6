/*
    SPDX-FileCopyrightText: 2021 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "syncedlaunchers.h"

// local
#include "../tools/qmlinvoke.h"

// Qt
#include <QQuickItem>

namespace Latte {
namespace Layouts {

namespace {

//! A synced-launcher client is a Latte plasmoid, so one that does not answer to an ability
//! signature is a QML-side breakage worth naming - unlike the speculative applet-child probes
//! in ContainmentInterface, which have to stay quiet.
template <typename... Args>
void invokeAbility(QQuickItem *client, const char *signature, Args &&...args)
{
    if (!invokeIfPresent(client, signature, std::forward<Args>(args)...)) {
        qDebug() << "Launchers Syncer Ability:" << signature << "was NOT found...";
    }
}

template <typename... Args>
void broadcastAbility(const QList<QQuickItem *> &clients, const char *signature, Args &&...args)
{
    for (auto *client : clients) {
        invokeAbility(client, signature, std::forward<Args>(args)...);
    }
}

}

SyncedLaunchers::SyncedLaunchers(QObject *parent)
    : QObject(parent)
{
}

SyncedLaunchers::~SyncedLaunchers()
{
}

void SyncedLaunchers::addAbilityClient(QQuickItem *client)
{
    if (m_clients.contains(client)) {
        return;
    }

    m_clients << client;

    connect(client, &QObject::destroyed, this, &SyncedLaunchers::removeClientObject);
}

void SyncedLaunchers::removeAbilityClient(QQuickItem *client)
{
    if (!m_clients.contains(client)) {
        return;
    }

    disconnect(client, &QObject::destroyed, this, &SyncedLaunchers::removeClientObject);
    m_clients.removeAll(client);
}

void SyncedLaunchers::removeClientObject(QObject *obj)
{
    //! QObject::destroyed fires from ~QObject(), after the QQuickItem subobject is
    //! already destroyed, so qobject_cast<QQuickItem*>(obj) returns nullptr here and
    //! the client was never removed — leaving a dangling pointer in m_clients that a
    //! later clients()/property() call dereferences and crashes on. Remove by pointer
    //! identity instead (a static_cast does not touch the object, and the signal's
    //! own connection is dropped automatically as the object is destroyed).
    m_clients.removeAll(static_cast<QQuickItem *>(obj));
}

QQuickItem *SyncedLaunchers::client(const int &id)
{
    if (id <= 0) {
        return nullptr;
    }

    for(const auto client: m_clients) {
        int clientid = client->property("clientId").toInt();
        if (clientid == id) {
            return client;
        }
    }

    return nullptr;
}

QList<QQuickItem *> SyncedLaunchers::clients(QString layoutName, QString groupId)
{
    QList<QQuickItem *> items;

    for(const auto client: m_clients) {
        QString cLayoutName = layoutName.isEmpty() ? QString() : client->property("layoutName").toString();
        QString gid = client->property("syncedGroupId").toString();
        if (cLayoutName == layoutName && gid == groupId) {
            items << client;
        }
    }

    return items;
}

QList<QQuickItem *> SyncedLaunchers::clients(QString layoutName, uint senderId, Latte::Types::LaunchersGroup launcherGroup, QString launcherGroupId)
{
    QList<QQuickItem *> temclients;

    if (launcherGroup == Types::UniqueLaunchers && launcherGroupId.isEmpty()) {
        //! on its own, single taskmanager
        auto c = client(senderId);
        if (c) {
            temclients << client(senderId);
        }
    } else {
        temclients << clients(layoutName, launcherGroupId);
    }

    return temclients;
}

QList<QQuickItem *> SyncedLaunchers::groupClients(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);
    //! only a layout group is scoped to one layout; for the others the name must not narrow the match
    QString lName = (group == Types::LayoutLaunchers) ? layoutName : QString();

    return clients(lName, senderId, group, launcherGroupId);
}

void SyncedLaunchers::addLauncher(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QString launcher)
{
    broadcastAbility(groupClients(layoutName, senderId, launcherGroup, launcherGroupId),
                     "addSyncedLauncher(QVariant,QVariant)",
                     Q_ARG(QVariant, launcherGroup),
                     Q_ARG(QVariant, launcher));
}

void SyncedLaunchers::removeLauncher(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QString launcher)
{
    broadcastAbility(groupClients(layoutName, senderId, launcherGroup, launcherGroupId),
                     "removeSyncedLauncher(QVariant,QVariant)",
                     Q_ARG(QVariant, launcherGroup),
                     Q_ARG(QVariant, launcher));
}

void SyncedLaunchers::addLauncherToActivity(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QString launcher, QString activity)
{
    broadcastAbility(groupClients(layoutName, senderId, launcherGroup, launcherGroupId),
                     "addSyncedLauncherToActivity(QVariant,QVariant,QVariant)",
                     Q_ARG(QVariant, launcherGroup),
                     Q_ARG(QVariant, launcher),
                     Q_ARG(QVariant, activity));
}

void SyncedLaunchers::removeLauncherFromActivity(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QString launcher, QString activity)
{
    broadcastAbility(groupClients(layoutName, senderId, launcherGroup, launcherGroupId),
                     "removeSyncedLauncherFromActivity(QVariant,QVariant,QVariant)",
                     Q_ARG(QVariant, launcherGroup),
                     Q_ARG(QVariant, launcher),
                     Q_ARG(QVariant, activity));
}

void SyncedLaunchers::urlsDropped(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QStringList urls)
{
    broadcastAbility(groupClients(layoutName, senderId, launcherGroup, launcherGroupId),
                     "dropSyncedUrls(QVariant,QVariant)",
                     Q_ARG(QVariant, launcherGroup),
                     Q_ARG(QVariant, urls));
}

void SyncedLaunchers::validateLaunchersOrder(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QStringList launchers)
{
    for (const auto c : groupClients(layoutName, senderId, launcherGroup, launcherGroupId)) {
        //! the sender is the client that just reordered, so sending its own order back to it is
        //! exactly what this guard avoids. The lookup stays inside the loop because an invoke
        //! runs its QML synchronously, and that QML can add or drop clients.
        if (c != client(senderId)) {
            invokeAbility(c,
                          "validateSyncedLaunchersOrder(QVariant,QVariant)",
                          Q_ARG(QVariant, launcherGroup),
                          Q_ARG(QVariant, launchers));
        }
    }
}

}
} //end of namespace
