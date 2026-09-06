/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SETTINGSGENERICHANDLER_H
#define SETTINGSGENERICHANDLER_H

//! Qt
#include <QAction>
#include <QObject>
#include <QMenu>
#include <QPushButton>

// KDE
#include <KMessageWidget>

namespace Latte {
namespace Settings {
namespace Dialog {
class GenericDialog;
}
}
}


namespace Latte {
namespace Settings {
namespace Handler {

//! Handlers are objects to handle the UI elements that semantically associate with specific
//! ui::tabs or different windows. They are responsible also to handle the user interaction
//! between controllers and views

class Generic : public QObject
{
    Q_OBJECT
public:
    static constexpr const char* TWINENABLED = "Enabled";
    static constexpr const char* TWINVISIBLE = "Visible";
    static constexpr const char* TWINCHECKED = "Checked";

    Generic(Dialog::GenericDialog *parent);
    Generic(Dialog::GenericDialog *parentDialog, QObject *parent);

    virtual bool hasChangedData() const = 0;
    virtual bool inDefaultValues() const = 0;

    void showInlineMessage(const QString &msg, const KMessageWidget::MessageType &type, const bool &isPersistent = false, QList<QAction *> actions = QList<QAction *>());

public Q_SLOTS:
    virtual void reset() = 0;
    virtual void resetDefaults() = 0;
    virtual void save() = 0;

Q_SIGNALS:
    void dataChanged();

protected:
    void setTwinProperty(QAction *action, const QString &property, QVariant value);
    void connectActionWithButton(QPushButton *button, QAction *action);

    //! Builds an action and its twin button in one step. iconName sits between the
    //! two text parameters on purpose, so swapping label and tooltip does not
    //! silently compile. Pass a null menu for an action that lives in no menu.
    QAction *addTwinAction(QMenu *menu,
                           QPushButton *button,
                           const QString &text,
                           const QString &iconName,
                           const QString &tooltip,
                           const QKeySequence &shortcut,
                           bool checkable = false);

private:
    //! Twin Actions bind QAction* behavior with QPushButton*
    //! for simplicity reasons
    QHash<QAction *, QPushButton *> m_twinActions;

    Dialog::GenericDialog *m_dialog{nullptr};


};

}
}
}

#endif
