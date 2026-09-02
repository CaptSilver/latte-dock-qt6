/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Behavioural test for the Activities cell editor
// (app/settings/settingsdialog/delegates/activitiesdelegate.cpp).
//
// createEditor() builds a QPushButton whose PersistentMenu holds one checkable
// QAction per activity, and Reset has to put the menu back exactly the way the
// model handed it over. The menu is not a 1:1 image of the activity list
// though: rows the delegate skips leave no action behind, and the separator
// after "[Current Activity]" plus the Ok/Cancel/Reset widget action add entries
// no activity ever produced. So the snapshot Reset restores from has to name
// the actions themselves, not their position in either list.
//
// A hand-built model is enough - the delegate reads four roles and writes one.

#include "delegates/activitiesdelegate.h"
#include "layoutsmodel.h"

#include "data/activitydata.h"
#include "data/layoutdata.h"

#include <QAbstractTableModel>
#include <QAction>
#include <QDialogButtonBox>
#include <QMenu>
#include <QPushButton>
#include <QStyleOptionViewItem>
#include <QtTest>

using namespace Latte;
using LModel = Settings::Model::Layouts;

//! Answers only the roles Activities::createEditor()/setModelData() touch and
//! records the assignment that setModelData() writes back.
class FakeLayoutsModel : public QAbstractTableModel
{
public:
    Latte::Data::ActivitiesTable activitiesTable;
    QStringList sortedActivities;
    QStringList assignedActivities;

    QStringList written;
    bool didWrite{false};

    int rowCount(const QModelIndex & = QModelIndex()) const override
    {
        return 1;
    }

    int columnCount(const QModelIndex & = QModelIndex()) const override
    {
        return 1;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid()) {
            return QVariant();
        }

        switch (role) {
        case LModel::ISACTIVEROLE:
            return true;
        case LModel::ALLACTIVITIESSORTEDROLE:
            return sortedActivities;
        case LModel::ALLACTIVITIESDATAROLE:
            return QVariant::fromValue(activitiesTable);
        case LModel::ORIGINALASSIGNEDACTIVITIESROLE:
        case Qt::UserRole:
            return assignedActivities;
        default:
            return QVariant();
        }
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        if (!index.isValid() || role != Qt::UserRole) {
            return false;
        }

        written = value.toStringList();
        didWrite = true;
        return true;
    }
};

class ActivitiesDelegateTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void resetRestoresTheAssignedActivity();
    void resetUndoesAUserEditWithoutCrossAssigning();
    void resetThenOkWritesTheOriginalAssignment();
    void resetRestoresTheCurrentActivityRow();
    void resetRestoresTheAssignedActivityWhenSomeRowsAreSkipped();
    void separatorFollowsTheCurrentActivityRow();

private:
    //! The three pseudo rows the layouts model always puts in front of the real
    //! activities, followed by two real ones. `specialsAreValid` mirrors whether
    //! the model gave the pseudo rows a state - the delegate drops rows whose
    //! Activity::isValid() is false, so that flag decides how many actions the
    //! menu ends up with.
    static void fillModel(FakeLayoutsModel &model, const QString &currentRealActivityId = QString(), bool specialsAreValid = true);
    static Latte::Data::Activity makeActivity(const QString &id, const QString &name, Latte::Data::Activity::State state, bool isCurrent = false);

    static QAction *actionFor(QMenu *menu, const QString &id);
    static QPushButton *dialogButton(QMenu *menu, QDialogButtonBox::StandardButton which);
};

Latte::Data::Activity ActivitiesDelegateTest::makeActivity(const QString &id, const QString &name, Latte::Data::Activity::State state, bool isCurrent)
{
    Latte::Data::Activity activity;
    activity.id = id;
    activity.name = name;
    activity.icon = QStringLiteral("activities");
    activity.state = state;
    activity.isCurrent = isCurrent;
    return activity;
}

void ActivitiesDelegateTest::fillModel(FakeLayoutsModel &model, const QString &currentRealActivityId, bool specialsAreValid)
{
    const Latte::Data::Activity::State specialState = specialsAreValid ? Latte::Data::Activity::Stopped : Latte::Data::Activity::Invalid;

    model.activitiesTable.clear();
    model.activitiesTable << makeActivity(QLatin1String(Data::Layout::ALLACTIVITIESID), QStringLiteral("[ All Activities ]"), specialState);
    model.activitiesTable << makeActivity(QLatin1String(Data::Layout::FREEACTIVITIESID), QStringLiteral("[ Free Activities ]"), specialState);
    model.activitiesTable << makeActivity(QLatin1String(Data::Layout::CURRENTACTIVITYID), QStringLiteral("[ Current Activity ]"), specialState);
    model.activitiesTable << makeActivity(QStringLiteral("act-A"), QStringLiteral("Alpha"), Latte::Data::Activity::Running, currentRealActivityId == QLatin1String("act-A"));
    model.activitiesTable << makeActivity(QStringLiteral("act-B"), QStringLiteral("Beta"), Latte::Data::Activity::Running, currentRealActivityId == QLatin1String("act-B"));

    model.sortedActivities = QStringList{QLatin1String(Data::Layout::ALLACTIVITIESID),
                                         QLatin1String(Data::Layout::FREEACTIVITIESID),
                                         QLatin1String(Data::Layout::CURRENTACTIVITYID),
                                         QStringLiteral("act-A"),
                                         QStringLiteral("act-B")};
}

QAction *ActivitiesDelegateTest::actionFor(QMenu *menu, const QString &id)
{
    const auto actions = menu->actions();

    for (QAction *action : actions) {
        if (action->data().toString() == id) {
            return action;
        }
    }

    return nullptr;
}

QPushButton *ActivitiesDelegateTest::dialogButton(QMenu *menu, QDialogButtonBox::StandardButton which)
{
    QDialogButtonBox *box = menu->findChild<QDialogButtonBox *>();
    return box ? box->button(which) : nullptr;
}

//! Reset means "put back what the model gave me", so pressing it without having
//! touched anything has to be a no-op.
void ActivitiesDelegateTest::resetRestoresTheAssignedActivity()
{
    FakeLayoutsModel model;
    fillModel(model);
    model.assignedActivities = QStringList{QStringLiteral("act-B")};

    Settings::Layout::Delegate::Activities delegate(nullptr);

    QWidget parent;
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 200, 24);

    QPushButton *button = qobject_cast<QPushButton *>(delegate.createEditor(&parent, option, model.index(0, 0)));
    QVERIFY(button);

    QMenu *menu = button->menu();
    QVERIFY(menu);
    QVERIFY(actionFor(menu, QStringLiteral("act-B"))->isChecked());

    QPushButton *reset = dialogButton(menu, QDialogButtonBox::Reset);
    QVERIFY(reset);
    reset->click();

    QVERIFY2(actionFor(menu, QStringLiteral("act-B"))->isChecked(), "Reset dropped the activity the layout was assigned to.");
    QVERIFY2(!actionFor(menu, QStringLiteral("act-A"))->isChecked(), "Reset checked an activity the layout was never assigned to.");
}

void ActivitiesDelegateTest::resetUndoesAUserEditWithoutCrossAssigning()
{
    FakeLayoutsModel model;
    fillModel(model);
    model.assignedActivities = QStringList{QStringLiteral("act-B")};

    Settings::Layout::Delegate::Activities delegate(nullptr);

    QWidget parent;
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 200, 24);

    QPushButton *button = qobject_cast<QPushButton *>(delegate.createEditor(&parent, option, model.index(0, 0)));
    QVERIFY(button);
    QMenu *menu = button->menu();

    actionFor(menu, QStringLiteral("act-A"))->setChecked(true);
    actionFor(menu, QStringLiteral("act-B"))->setChecked(false);

    dialogButton(menu, QDialogButtonBox::Reset)->click();

    QVERIFY2(actionFor(menu, QStringLiteral("act-B"))->isChecked(), "Reset did not restore the activity the edit removed.");
    QVERIFY2(!actionFor(menu, QStringLiteral("act-A"))->isChecked(), "Reset left the edit's activity checked.");
}

//! What Reset shows has to be what Ok commits, otherwise the layout silently
//! ends up bound to an activity nobody picked.
void ActivitiesDelegateTest::resetThenOkWritesTheOriginalAssignment()
{
    FakeLayoutsModel model;
    fillModel(model);
    model.assignedActivities = QStringList{QStringLiteral("act-B")};

    Settings::Layout::Delegate::Activities delegate(nullptr);

    QWidget parent;
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 200, 24);

    QPushButton *button = qobject_cast<QPushButton *>(delegate.createEditor(&parent, option, model.index(0, 0)));
    QVERIFY(button);
    QMenu *menu = button->menu();

    dialogButton(menu, QDialogButtonBox::Reset)->click();
    dialogButton(menu, QDialogButtonBox::Ok)->click();

    delegate.setModelData(button, &model, model.index(0, 0));

    QVERIFY(model.didWrite);
    QCOMPARE(model.written, QStringList{QStringLiteral("act-B")});
}

//! Restoring "[Current Activity]" re-enters masterIndexChanged, which unchecks
//! every other row before the current activity gets re-checked. Reset has to
//! survive that round trip.
void ActivitiesDelegateTest::resetRestoresTheCurrentActivityRow()
{
    FakeLayoutsModel model;
    fillModel(model, QStringLiteral("act-B"));
    model.assignedActivities = QStringList{QStringLiteral("act-B")};

    Settings::Layout::Delegate::Activities delegate(nullptr);

    QWidget parent;
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 200, 24);

    QPushButton *button = qobject_cast<QPushButton *>(delegate.createEditor(&parent, option, model.index(0, 0)));
    QVERIFY(button);
    QMenu *menu = button->menu();

    QAction *currentRow = actionFor(menu, QLatin1String(Data::Layout::CURRENTACTIVITYID));
    QVERIFY(currentRow);
    QVERIFY(currentRow->isChecked());

    currentRow->setChecked(false);
    QVERIFY(!actionFor(menu, QStringLiteral("act-B"))->isChecked());

    dialogButton(menu, QDialogButtonBox::Reset)->click();

    QVERIFY2(actionFor(menu, QLatin1String(Data::Layout::CURRENTACTIVITYID))->isChecked(), "Reset did not restore the current-activity row.");
    QVERIFY2(actionFor(menu, QStringLiteral("act-B"))->isChecked(), "Reset did not restore the real activity behind the current-activity row.");
    QVERIFY2(!actionFor(menu, QStringLiteral("act-A"))->isChecked(), "Reset checked an activity the layout was never assigned to.");
}

//! The layouts model can hand over pseudo rows the delegate drops, which pulls
//! every surviving action forward in the menu. Reset must not care.
void ActivitiesDelegateTest::resetRestoresTheAssignedActivityWhenSomeRowsAreSkipped()
{
    FakeLayoutsModel model;
    fillModel(model, QString(), false);
    model.assignedActivities = QStringList{QStringLiteral("act-B")};

    Settings::Layout::Delegate::Activities delegate(nullptr);

    QWidget parent;
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 200, 24);

    QPushButton *button = qobject_cast<QPushButton *>(delegate.createEditor(&parent, option, model.index(0, 0)));
    QVERIFY(button);
    QMenu *menu = button->menu();

    QVERIFY(!actionFor(menu, QLatin1String(Data::Layout::ALLACTIVITIESID)));
    QVERIFY(actionFor(menu, QStringLiteral("act-B"))->isChecked());

    dialogButton(menu, QDialogButtonBox::Reset)->click();

    QVERIFY2(actionFor(menu, QStringLiteral("act-B"))->isChecked(), "Reset dropped the activity the layout was assigned to.");
    QVERIFY2(!actionFor(menu, QStringLiteral("act-A"))->isChecked(), "Reset checked an activity the layout was never assigned to.");
}

//! Pins the menu shape the delegate builds so a future layout change has to be
//! deliberate rather than quietly re-breaking a positional assumption.
void ActivitiesDelegateTest::separatorFollowsTheCurrentActivityRow()
{
    FakeLayoutsModel model;
    fillModel(model);
    model.assignedActivities = QStringList{QStringLiteral("act-B")};

    Settings::Layout::Delegate::Activities delegate(nullptr);

    QWidget parent;
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 200, 24);

    QPushButton *button = qobject_cast<QPushButton *>(delegate.createEditor(&parent, option, model.index(0, 0)));
    QVERIFY(button);

    const auto actions = button->menu()->actions();
    QVERIFY(actions.count() >= 6);

    QCOMPARE(actions.at(0)->data().toString(), QLatin1String(Data::Layout::ALLACTIVITIESID));
    QCOMPARE(actions.at(1)->data().toString(), QLatin1String(Data::Layout::FREEACTIVITIESID));
    QCOMPARE(actions.at(2)->data().toString(), QLatin1String(Data::Layout::CURRENTACTIVITYID));
    QVERIFY(actions.at(3)->isSeparator());
    QCOMPARE(actions.at(4)->data().toString(), QStringLiteral("act-A"));
    QCOMPARE(actions.at(5)->data().toString(), QStringLiteral("act-B"));
}

QTEST_MAIN(ActivitiesDelegateTest)

#include "activitiesdelegatetest.moc"
