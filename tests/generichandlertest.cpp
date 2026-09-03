/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Handler::Generic pairs a QAction with a QPushButton -- the "twin" -- and the
// button copies the action's text, tooltip and icon exactly once, at wire time.
// Sixteen call sites used to spell that sequence out by hand, and one of them
// set the tooltip on the wrong action, so the Import button showed a tooltip Qt
// had synthesised from its own label.
//
// addTwinAction is what makes that unrepresentable. This drives it directly:
// Handler::Generic only stores the dialog pointer, so a null one is enough to
// build the widgets headlessly.

#include "generichandler.h"

#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QPushButton>
#include <QtTest>

using namespace Latte::Settings;

namespace {

//! Minimal concrete handler: the base is abstract but none of these are reached.
class Probe : public Handler::Generic
{
public:
    Probe()
        : Handler::Generic(nullptr)
    {
    }

    bool hasChangedData() const override { return false; }
    bool inDefaultValues() const override { return true; }
    void reset() override {}
    void resetDefaults() override {}
    void save() override {}

    using Handler::Generic::addTwinAction;
};

}

class GenericHandlerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void twinButtonTakesTheActionsOwnTooltip();
    void twinActionKeepsItsPlaceInTheMenu();
    void twinCarriesCheckableThrough();
};

void GenericHandlerTest::twinButtonTakesTheActionsOwnTooltip()
{
    Probe probe;
    QPushButton button;

    const QString tooltip = QStringLiteral("Import dock or panel from local file");
    QAction *action = probe.addTwinAction(nullptr,
                                          &button,
                                          QStringLiteral("&Import..."),
                                          QStringLiteral("document-import"),
                                          tooltip,
                                          QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));

    QVERIFY(action);
    QCOMPARE(action->parent(), &probe);
    QCOMPARE(action->toolTip(), tooltip);

    // The bug this replaces: with no tooltip of its own the action falls back to
    // its stripped text, and the button silently shows "Import".
    QCOMPARE(button.toolTip(), tooltip);
    QVERIFY(button.toolTip() != QStringLiteral("Import"));

    QCOMPARE(button.text(), QStringLiteral("&Import..."));
    QCOMPARE(action->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
}

void GenericHandlerTest::twinActionKeepsItsPlaceInTheMenu()
{
    Probe probe;
    QPushButton button;
    QMenu menu;

    QAction *first = menu.addAction(QStringLiteral("Already here"));
    menu.addSeparator();

    QAction *action = probe.addTwinAction(&menu,
                                          &button,
                                          QStringLiteral("&Duplicate"),
                                          QStringLiteral("edit-copy"),
                                          QStringLiteral("Duplicate selected layout"),
                                          QKeySequence(Qt::CTRL | Qt::Key_D));

    // Appended, so the separators that group the Layout menu keep their meaning.
    QCOMPARE(menu.actions().count(), 3);
    QCOMPARE(menu.actions().at(0), first);
    QVERIFY(menu.actions().at(1)->isSeparator());
    QCOMPARE(menu.actions().at(2), action);
}

void GenericHandlerTest::twinCarriesCheckableThrough()
{
    Probe probe;
    QPushButton plain;
    QPushButton checkable;

    QAction *plainAction = probe.addTwinAction(nullptr,
                                               &plain,
                                               QStringLiteral("Switch"),
                                               QStringLiteral("user-identity"),
                                               QStringLiteral("Switch to selected layout"),
                                               QKeySequence(Qt::CTRL | Qt::Key_Tab));

    QVERIFY(!plainAction->isCheckable());
    QVERIFY(!plain.isCheckable());

    QAction *checkableAction = probe.addTwinAction(nullptr,
                                                   &checkable,
                                                   QStringLiteral("Ena&bled"),
                                                   QStringLiteral("edit-link"),
                                                   QStringLiteral("Assign in activities"),
                                                   QKeySequence(Qt::CTRL | Qt::Key_B),
                                                   true);

    QVERIFY(checkableAction->isCheckable());
    QVERIFY(checkable.isCheckable());
}

QTEST_MAIN(GenericHandlerTest)

#include "generichandlertest.moc"
