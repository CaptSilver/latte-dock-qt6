/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tasksmodel.h"

// Qt
#include <QDebug>

// Plasma
#include <Plasma/Applet>
#include <PlasmaQuick/AppletQuickItem>

namespace Latte {
namespace ViewPart {

TasksModel::TasksModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TasksModel::count() const
{
    return m_tasks.count();
}

int TasksModel::rowCount(const QModelIndex &parent) const
{
    //! A list model has no children, so any valid parent has no rows.
    if (parent.isValid()) {
        return 0;
    }

    return m_tasks.count();
}

QVariant TasksModel::data(const QModelIndex &index, int role) const
{
    bool rowIsValid = (index.row()>=0 && index.row()<m_tasks.count());
    if (!rowIsValid) {
        return QVariant();
    }

    if (role == Qt::UserRole) {
        return QVariant::fromValue(m_tasks[index.row()]);
    }

    return QVariant();
}


QHash<int, QByteArray> TasksModel::roleNames() const{
    QHash<int, QByteArray> roles;
    roles[Qt::UserRole] = "tasks";
    return roles;
}

void TasksModel::addTask(PlasmaQuick::AppletQuickItem *plasmoid)
{
    //! An item sitting in the waiting list is still known to the model, just not
    //! shown: re-adding it there would append a second row for the same plasmoid
    //! and the later restore would append a third. A null one has to stop here
    //! too -- the applet connect below dereferences it.
    if (!plasmoid || m_tasks.contains(plasmoid) || m_tasksWaiting.contains(plasmoid)) {
        return;
    }

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_tasks << plasmoid;
    endInsertRows();

    connect(plasmoid, &QObject::destroyed, this, [&, plasmoid](){
        removeTask(plasmoid);
    });

    connect(plasmoid->applet(), &Plasma::Applet::destroyedChanged, this, [&, plasmoid](const bool &destroyed){
        if (destroyed) {
            moveIntoWaitingTasks(plasmoid);
        } else {
            restoreFromWaitingTasks(plasmoid);
        }
    });

    Q_EMIT countChanged();
}

void TasksModel::moveIntoWaitingTasks(PlasmaQuick::AppletQuickItem *plasmoid)
{
    if (plasmoid && !m_tasks.contains(plasmoid)) {
        return;
    }

    int tind = m_tasks.indexOf(plasmoid);

    if (tind >= 0) {
        beginRemoveRows(QModelIndex(), tind, tind);
        m_tasksWaiting << m_tasks.takeAt(tind);
        endRemoveRows();
        Q_EMIT countChanged();
    }
}

void TasksModel::restoreFromWaitingTasks(PlasmaQuick::AppletQuickItem *plasmoid)
{
    if (plasmoid && !m_tasksWaiting.contains(plasmoid)) {
        return;
    }

    int tind = m_tasksWaiting.indexOf(plasmoid);

    if (tind >= 0) {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_tasks << m_tasksWaiting.takeAt(tind);
        endInsertRows();
        Q_EMIT countChanged();
    }
}

void TasksModel::removeTask(PlasmaQuick::AppletQuickItem *plasmoid)
{
    if (!plasmoid) {
        return;
    }

    m_tasksWaiting.removeAll(plasmoid);

    int iex = m_tasks.indexOf(plasmoid);

    if (iex >= 0) {
        //! removeAt, not removeAll: the announced span is one row, and dropping
        //! more than that breaks the model contract for anything listening.
        beginRemoveRows(QModelIndex(), iex, iex);
        m_tasks.removeAt(iex);
        endRemoveRows();

        Q_EMIT countChanged();
    }
}

}
}
