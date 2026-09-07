/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

//! Both custom menu-item widgets ask QStyle::sizeFromContents(CT_MenuItem, ...) how wide
//! their row in the popup has to be. A style is free to ignore the size passed in and
//! rebuild the width from the option instead -- Breeze does exactly that for a Normal item,
//! reading opt.text and opt.font -- so what actually matters is whether the widget fills the
//! option in at all.
//!
//! ctest has no Plasma style configured and lands on Fusion, which honours the passed size
//! and therefore hides the bug entirely. So the assertions here are on the option the widget
//! hands the style, captured through a proxy: that holds under any style. The Breeze pixel
//! checks are extra and skip when Breeze is not installed.

#include "../app/settings/viewsdialog/delegates/custommenuitemwidget.h"
#include "../containmentactions/contextmenu/layoutmenuitemwidget.h"

#include <QAction>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QProxyStyle>
#include <QScopedPointer>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOptionMenuItem>
#include <QWidget>
#include <QtTest>

using Latte::Settings::View::Widget::CustomMenuItemWidget;

static const QString SHORTNAME = QStringLiteral("A");
static const QString LONGNAME = QStringLiteral("My Rather Long Layout Name");

//! Captures the QStyleOptionMenuItem a widget hands to the style, so a test can assert on
//! what the widget said about itself instead of on the pixels one particular style returns.
class RecordingStyle : public QProxyStyle
{
public:
    explicit RecordingStyle(QStyle *base)
        : QProxyStyle(base)
    {
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const override
    {
        if (type == CT_MenuItem) {
            m_calls++;
            m_isMenuItemOption = false;

            if (const auto *menuitem = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
                m_seen = *menuitem;
                m_isMenuItemOption = true;
            }
        }

        return QProxyStyle::sizeFromContents(type, option, contentsSize, widget);
    }

    void reset() const { m_calls = 0; }
    int calls() const { return m_calls; }
    bool isMenuItemOption() const { return m_isMenuItemOption; }
    const QStyleOptionMenuItem &seen() const { return m_seen; }

private:
    mutable int m_calls{0};
    mutable bool m_isMenuItemOption{false};
    mutable QStyleOptionMenuItem m_seen;
};

//! Deliberately unlike the application font. A default-constructed QStyleOptionMenuItem already
//! carries the application font, so only a widget font that differs from it can catch a hint
//! that never told the style which font to measure with.
static QFont oversizedFont()
{
    QFont font = QApplication::font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() > 0 ? font.pointSizeF() * 2.0 : 20.0);
    return font;
}

//! The assertions shared by both widgets: the style has to be told what it is measuring.
static void verifyOptionDescribesWidget(const RecordingStyle &recorder, const QWidget *widget, const QString &text)
{
    QCOMPARE(recorder.calls(), 1);
    QVERIFY2(recorder.isMenuItemOption(), "CT_MenuItem was measured with something other than a QStyleOptionMenuItem");

    const QStyleOptionMenuItem &opt = recorder.seen();

    QCOMPARE(opt.text, text);
    QCOMPARE(opt.font, widget->font());
    QCOMPARE(opt.fontMetrics, widget->fontMetrics());
    QCOMPARE(opt.menuItemType, QStyleOptionMenuItem::Normal);
    QCOMPARE(opt.maxIconWidth, widget->style()->pixelMetric(QStyle::PM_SmallIconSize, &opt, widget));

    //! both widgets always reserve a radio column when they paint, so the measured item has one too
    QVERIFY(opt.menuHasCheckableItems);
    QCOMPARE(opt.checkType, QStyleOptionMenuItem::Exclusive);
    QCOMPARE(opt.checked, true);

    //! initFrom() carries the widget's own geometry and state across
    QCOMPARE(opt.rect, widget->rect());
    QVERIFY(opt.state & QStyle::State_Enabled);
}

//! A checkable, checked action holding the given name, i.e. what the layouts menu builds.
static void setupAction(QAction &action, const QString &text)
{
    action.setText(text);
    action.setCheckable(true);
    action.setChecked(true);
}

static QStyle *createBreezeOrSkip()
{
    if (!QStyleFactory::keys().contains(QStringLiteral("Breeze"))) {
        return nullptr;
    }

    return QStyleFactory::create(QStringLiteral("Breeze"));
}

class MenuItemSizeHintTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void layoutMenuItemDescribesItselfToTheStyle();
    void customMenuItemDescribesItselfToTheStyle();
    void layoutMenuItemWidthFollowsTextUnderBreeze();
    void customMenuItemWidthFollowsTextUnderBreeze();
};

void MenuItemSizeHintTest::layoutMenuItemDescribesItselfToTheStyle()
{
    RecordingStyle recorder(QStyleFactory::create(QStringLiteral("Fusion")));

    QAction action;
    setupAction(action, LONGNAME);

    LayoutMenuItemWidget widget(&action, nullptr);
    widget.setStyle(&recorder);
    widget.setFont(oversizedFont());
    QVERIFY2(widget.font() != QApplication::font(), "the font assertion needs a widget font the style cannot get for free");

    recorder.reset();
    widget.minimumSizeHint();

    verifyOptionDescribesWidget(recorder, &widget, LONGNAME);
}

void MenuItemSizeHintTest::customMenuItemDescribesItselfToTheStyle()
{
    RecordingStyle recorder(QStyleFactory::create(QStringLiteral("Fusion")));

    QAction action;
    setupAction(action, LONGNAME);

    CustomMenuItemWidget widget(&action, nullptr);
    widget.setStyle(&recorder);
    widget.setFont(oversizedFont());
    QVERIFY2(widget.font() != QApplication::font(), "the font assertion needs a widget font the style cannot get for free");

    recorder.reset();
    widget.minimumSizeHint();

    verifyOptionDescribesWidget(recorder, &widget, LONGNAME);
}

void MenuItemSizeHintTest::layoutMenuItemWidthFollowsTextUnderBreeze()
{
    QScopedPointer<QStyle> breeze(createBreezeOrSkip());

    if (breeze.isNull()) {
        QSKIP("Breeze is not installed here, so the style that rebuilds the item width from the option cannot be exercised");
    }

    QAction shortaction;
    setupAction(shortaction, SHORTNAME);
    QAction longaction;
    setupAction(longaction, LONGNAME);

    LayoutMenuItemWidget shortwidget(&shortaction, nullptr);
    LayoutMenuItemWidget longwidget(&longaction, nullptr);

    const QFont font = oversizedFont();

    for (QWidget *widget : {static_cast<QWidget *>(&shortwidget), static_cast<QWidget *>(&longwidget)}) {
        widget->setStyle(breeze.data());
        widget->setFont(font);
    }

    const int shortwidth = shortwidget.minimumSizeHint().width();
    const int longwidth = longwidget.minimumSizeHint().width();

    const QFontMetrics metrics(font);
    const int textgrowth = metrics.horizontalAdvance(LONGNAME) - metrics.horizontalAdvance(SHORTNAME);

    QVERIFY2(longwidth - shortwidth >= textgrowth,
             qPrintable(QStringLiteral("Breeze hint grew %1px where the text grew %2px (short %3px, long %4px)")
                            .arg(longwidth - shortwidth)
                            .arg(textgrowth)
                            .arg(shortwidth)
                            .arg(longwidth)));
    QVERIFY2(longwidth > metrics.horizontalAdvance(LONGNAME),
             qPrintable(QStringLiteral("Breeze hint %1px cannot hold %2px of text").arg(longwidth).arg(metrics.horizontalAdvance(LONGNAME))));
}

void MenuItemSizeHintTest::customMenuItemWidthFollowsTextUnderBreeze()
{
    QScopedPointer<QStyle> breeze(createBreezeOrSkip());

    if (breeze.isNull()) {
        QSKIP("Breeze is not installed here, so the style that rebuilds the item width from the option cannot be exercised");
    }

    QAction shortaction;
    setupAction(shortaction, SHORTNAME);
    QAction longaction;
    setupAction(longaction, LONGNAME);

    CustomMenuItemWidget shortwidget(&shortaction, nullptr);
    CustomMenuItemWidget longwidget(&longaction, nullptr);

    const QFont font = oversizedFont();

    for (QWidget *widget : {static_cast<QWidget *>(&shortwidget), static_cast<QWidget *>(&longwidget)}) {
        widget->setStyle(breeze.data());
        widget->setFont(font);
    }

    const int shortwidth = shortwidget.minimumSizeHint().width();
    const int longwidth = longwidget.minimumSizeHint().width();

    const QFontMetrics metrics(font);
    const int textgrowth = metrics.horizontalAdvance(LONGNAME) - metrics.horizontalAdvance(SHORTNAME);

    QVERIFY2(longwidth - shortwidth >= textgrowth,
             qPrintable(QStringLiteral("Breeze hint grew %1px where the text grew %2px (short %3px, long %4px)")
                            .arg(longwidth - shortwidth)
                            .arg(textgrowth)
                            .arg(shortwidth)
                            .arg(longwidth)));
    QVERIFY2(longwidth > metrics.horizontalAdvance(LONGNAME),
             qPrintable(QStringLiteral("Breeze hint %1px cannot hold %2px of text").arg(longwidth).arg(metrics.horizontalAdvance(LONGNAME))));
}

QTEST_MAIN(MenuItemSizeHintTest)
#include "menuitemsizehinttest.moc"
