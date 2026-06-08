/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ACTIVITIESINFO_H
#define ACTIVITIESINFO_H

// local
#include "activitydata.h"

// Qt
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Latte {

//! KActivities 6 dropped Consumer::runningActivities() and Info::State, collapsing
//! the running/stopped distinction Latte needs to assign layouts per activity and
//! to cycle only between live activities. These helpers read it straight from the
//! activity manager (org.kde.ActivityManager, ListActivitiesWithInformation).
namespace ActivitiesInfo {

//! One activity record, kept in the manager's order (cycling next/previous relies
//! on a stable order, so do not derive ordered lists from an unordered map).
struct Record
{
    QString id;
    Data::Activity::State state{Data::Activity::Invalid};
};

//! Live, ordered query of every activity and its state.
QVector<Record> list();

//! Live running activity ids in manager order.
QStringList runningActivities();

//! Live id -> state map for per-activity lookups.
QHash<QString, Data::Activity::State> states();

//! Running ids from records, order preserved. Pure, so the running/stopped
//! distinction is unit-testable without a live activity manager.
QStringList runningActivitiesFrom(const QVector<Record> &records);

//! Map an org.kde.ActivityManager state value onto Data::Activity::State by
//! meaning (their Starting/Stopping numbers differ, so a cast would be wrong).
Data::Activity::State stateFromManager(int managerState);

}
}

#endif
