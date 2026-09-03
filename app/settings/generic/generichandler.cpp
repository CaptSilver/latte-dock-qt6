/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "generichandler.h"

// local
#include "genericdialog.h"

namespace Latte {
namespace Settings {
namespace Handler {

Generic::Generic(Dialog::GenericDialog *parent)
    : QObject(parent),
      m_dialog(parent)
{
}

Generic::Generic(Dialog::GenericDialog *parentDialog, QObject *parent)
    : QObject(parent),
      m_dialog(parentDialog)
{
}


void Generic::setTwinProperty(QAction *action, const QString &property, QVariant value)
{
    if (!m_twinActions.contains(action)) {
        return;
    }

    if (property == QLatin1String(TWINVISIBLE)) {
        action->setVisible(value.toBool());
        m_twinActions[action]->setVisible(value.toBool());
    } else if (property == QLatin1String(TWINENABLED)) {
        action->setEnabled(value.toBool());
        m_twinActions[action]->setEnabled(value.toBool());
    } else if (property == QLatin1String(TWINCHECKED)) {
        action->setChecked(value.toBool());
        m_twinActions[action]->setChecked(value.toBool());
    }
}

void Generic::connectActionWithButton(QPushButton *button, QAction *action)
{
    button->setText(action->text());
    button->setToolTip(action->toolTip());
    button->setWhatsThis(action->whatsThis());
    button->setIcon(action->icon());
    button->setCheckable(action->isCheckable());
    button->setChecked(action->isChecked());

    m_twinActions[action] = button;

    connect(button, &QPushButton::clicked, action, &QAction::trigger);
}

QAction *Generic::addTwinAction(QMenu *menu,
                                QPushButton *button,
                                const QString &text,
                                const QString &iconName,
                                const QString &tooltip,
                                const QKeySequence &shortcut,
                                bool checkable)
{
    auto action = new QAction(text, this);
    action->setToolTip(tooltip);
    action->setIcon(QIcon::fromTheme(iconName));
    action->setShortcut(shortcut);

    if (checkable) {
        action->setCheckable(true);
    }

    if (menu) {
        menu->addAction(action);
    }

    //! Last, because the button copies text, tooltip and icon at this point and
    //! never resyncs -- wiring first is how a twin ends up with no tooltip.
    connectActionWithButton(button, action);

    return action;
}

void Generic::showInlineMessage(const QString &msg, const KMessageWidget::MessageType &type, const bool &isPersistent, QList<QAction *> actions)
{
    m_dialog->showInlineMessage(msg, type, isPersistent, actions);
}

}
}
}
