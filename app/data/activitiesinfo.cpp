/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "activitiesinfo.h"

// Qt
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QVariant>

namespace Latte {
namespace ActivitiesInfo {

Data::Activity::State stateFromManager(int managerState)
{
    //! org.kde.ActivityManager / KActivities::Info::State values:
    //!   Invalid=0, Unknown=1, Running=2, Starting=3, Stopped=4, Stopping=5.
    //! Data::Activity::State numbers Starting/Stopping differently, so map by
    //! meaning instead of casting the integer across.
    switch (managerState) {
    case 2: return Data::Activity::Running;
    case 3: return Data::Activity::Starting;
    case 4: return Data::Activity::Stopped;
    case 5: return Data::Activity::Stopping;
    default: return Data::Activity::Invalid;
    }
}

QStringList runningActivitiesFrom(const QVector<Record> &records)
{
    QStringList ids;

    for (const auto &record : records) {
        if (record.state == Data::Activity::Running) {
            ids << record.id;
        }
    }

    return ids;
}

QVector<Record> list()
{
    QVector<Record> records;

    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.ActivityManager"),
                                                       QStringLiteral("/ActivityManager/Activities"),
                                                       QStringLiteral("org.kde.ActivityManager.Activities"),
                                                       QStringLiteral("ListActivitiesWithInformation"));

    const QDBusMessage reply = QDBusConnection::sessionBus().call(call);

    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return records;
    }

    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();

    //! reply signature a(ssssi): id, name, icon, description, state
    arg.beginArray();
    while (!arg.atEnd()) {
        arg.beginStructure();
        QString id;
        QString name;
        QString icon;
        QString description;
        int state = 0;
        arg >> id >> name >> icon >> description >> state;
        arg.endStructure();

        records.append({id, stateFromManager(state)});
    }
    arg.endArray();

    return records;
}

QStringList runningActivities()
{
    return runningActivitiesFrom(list());
}

QHash<QString, Data::Activity::State> states()
{
    QHash<QString, Data::Activity::State> result;

    const QVector<Record> records = list();
    for (const auto &record : records) {
        result.insert(record.id, record.state);
    }

    return result;
}

}
}
