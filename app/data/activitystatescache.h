/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ACTIVITYSTATESCACHE_H
#define ACTIVITYSTATESCACHE_H

// local
#include "activitiesinfo.h"

// Qt
#include <QStringList>
#include <QVector>

// C++
#include <functional>
#include <utility>

namespace Latte {
namespace ActivitiesInfo {

//! Memoizes one activity-manager query so a single sync that needs the running
//! set more than once issues a single DBus round-trip. records() fetches on a cold
//! cache and returns the stored value afterwards; invalidate() drops it so the next
//! read refetches. The fetch callback is injected, so the memoization is unit-
//! testable without a live activity manager.
class StatesCache
{
public:
    using Fetch = std::function<QVector<Record>()>;

    explicit StatesCache(Fetch fetch)
        : m_fetch(std::move(fetch))
    {
    }

    const QVector<Record> &records()
    {
        if (!m_valid) {
            m_records = m_fetch ? m_fetch() : QVector<Record>();
            m_valid = true;
        }

        return m_records;
    }

    QStringList runningActivities()
    {
        return runningActivitiesFrom(records());
    }

    void invalidate()
    {
        m_valid = false;
        m_records.clear();
    }

private:
    Fetch m_fetch;
    QVector<Record> m_records;
    bool m_valid{false};
};

}
}

#endif
