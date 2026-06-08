/*
    SPDX-FileCopyrightText: 2021 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "syncedlaunchers.h"

// local
#include "../lattecorona.h"
#include "../layout/centrallayout.h"
#include "../layouts/manager.h"
#include "../layouts/synchronizer.h"

// Qt
#include <QQuickItem>

// Plasma
#include <Plasma/Applet>
#include <Plasma/Containment>


namespace Latte {
namespace Layouts {

SyncedLaunchers::SyncedLaunchers(QObject *parent)
    : QObject(parent)
{
    m_manager = qobject_cast<Layouts::Manager *>(parent);
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

void SyncedLaunchers::addLauncher(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QString launcher)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);
    QString lName = (group == Types::LayoutLaunchers) ? layoutName : QString();

    for(const auto c : clients(lName, senderId, group, launcherGroupId)) {
        if (auto *metaObject = c->metaObject()) {
            int methodIndex = metaObject->indexOfMethod("addSyncedLauncher(QVariant,QVariant)");

            if (methodIndex == -1) {
                qDebug() << "Launchers Syncer Ability: addSyncedLauncher(QVariant,QVariant) was NOT found...";
                continue;
            }

            QMetaMethod method = metaObject->method(methodIndex);
            method.invoke(c, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, launcher));
        }
    }
}

void SyncedLaunchers::removeLauncher(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QString launcher)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);
    QString lName = (group == Types::LayoutLaunchers) ? layoutName : QString();

    for(const auto c : clients(lName, senderId, group, launcherGroupId)) {
        if (auto *metaObject = c->metaObject()) {
            int methodIndex = metaObject->indexOfMethod("removeSyncedLauncher(QVariant,QVariant)");

            if (methodIndex == -1) {
                qDebug() << "Launchers Syncer Ability: removeSyncedLauncher(QVariant,QVariant) was NOT found...";
                continue;
            }

            QMetaMethod method = metaObject->method(methodIndex);
            method.invoke(c, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, launcher));
        }
    }
}

void SyncedLaunchers::addLauncherToActivity(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QString launcher, QString activity)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);
    QString lName = (group == Types::LayoutLaunchers) ? layoutName : QString();

    for(const auto c : clients(lName, senderId, group, launcherGroupId)) {
        if (auto *metaObject = c->metaObject()) {
            int methodIndex = metaObject->indexOfMethod("addSyncedLauncherToActivity(QVariant,QVariant,QVariant)");

            if (methodIndex == -1) {
                qDebug() << "Launchers Syncer Ability: addSyncedLauncherToActivity(QVariant,QVariant,QVariant) was NOT found...";
                continue;
            }

            QMetaMethod method = metaObject->method(methodIndex);
            method.invoke(c, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, launcher), Q_ARG(QVariant, activity));
        }
    }
}

void SyncedLaunchers::removeLauncherFromActivity(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QString launcher, QString activity)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);
    QString lName = (group == Types::LayoutLaunchers) ? layoutName : QString();

    for(const auto c : clients(lName, senderId, group, launcherGroupId)) {
        if (auto *metaObject = c->metaObject()) {
            int methodIndex = metaObject->indexOfMethod("removeSyncedLauncherFromActivity(QVariant,QVariant,QVariant)");

            if (methodIndex == -1) {
                qDebug() << "Launchers Syncer Ability: removeSyncedLauncherFromActivity(QVariant,QVariant,QVariant) was NOT found...";
                continue;
            }

            QMetaMethod method = metaObject->method(methodIndex);
            method.invoke(c, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, launcher), Q_ARG(QVariant, activity));
        }
    }
}

void SyncedLaunchers::urlsDropped(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QStringList urls)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);
    QString lName = (group == Types::LayoutLaunchers) ? layoutName : QString();

    for(const auto c : clients(lName, senderId, group, launcherGroupId)) {
        if (auto *metaObject = c->metaObject()) {
            int methodIndex = metaObject->indexOfMethod("dropSyncedUrls(QVariant,QVariant)");

            if (methodIndex == -1) {
                qDebug() << "Launchers Syncer Ability: dropSyncedUrls(QVariant,QVariant) was NOT found...";
                continue;
            }

            QMetaMethod method = metaObject->method(methodIndex);
            method.invoke(c, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, urls));
        }
    }
}

void SyncedLaunchers::validateLaunchersOrder(QString layoutName, uint senderId, int launcherGroup, QString launcherGroupId, QStringList launchers)
{
    Types::LaunchersGroup group = static_cast<Types::LaunchersGroup>(launcherGroup);
    QString lName = (group == Types::LayoutLaunchers) ? layoutName : QString();

    for(const auto c : clients(lName, senderId, group, launcherGroupId)) {
        auto tc = client(senderId);

        if (c != tc) {
            if (auto *metaObject = c->metaObject()) {
                int methodIndex = metaObject->indexOfMethod("validateSyncedLaunchersOrder(QVariant,QVariant)");

                if (methodIndex == -1) {
                    qDebug() << "Launchers Syncer Ability: validateSyncedLaunchersOrder(QVariant,QVariant) was NOT found...";
                    continue;
                }

                QMetaMethod method = metaObject->method(methodIndex);
                method.invoke(c, Q_ARG(QVariant, launcherGroup), Q_ARG(QVariant, launchers));
            }
        }
    }
}

}
} //end of namespace
