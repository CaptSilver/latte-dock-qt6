/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// A right-click on the dock must raise Latte's context menu. The menu is driven by a
// ContainmentActions plugin registered for the "RightButton;NoModifier" trigger; the ContextMenu
// layer looks the plugin up by that exact trigger string. On Plasma 6 the corona config no longer
// carries an [ActionPlugins] group, so libplasma loads no action plugins, the containment's
// containmentActions() comes up empty, and right-click became a silent no-op. The View installs
// Latte's standard context menu as the default RightButton action so the menu works regardless of
// what the layout config carries.
//
// These tests pin the trigger contract (a right-click really does map to "RightButton;NoModifier")
// and that the shipped default and the View's fallback agree on the plugin id.

#include <QtTest>
#include <QMouseEvent>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <Plasma/ContainmentActions>

class ContextMenuDefaultTest : public QObject
{
    Q_OBJECT

private:
    QString readSource(const QString &path);

private Q_SLOTS:
    void rightClickMapsToExpectedTrigger();
    void shippedDefaultsDeclareLatteContextMenu();
    void viewInstallsDefaultRightButtonAction();
    void contextMenuPluginIsNamedAfterItsConfigId();
};

QString ContextMenuDefaultTest::readSource(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

//! The trigger the View registers must be exactly what a right-click produces, otherwise the
//! lookup in ContextMenuLayerQuickItem::mousePressEvent misses and no menu shows.
void ContextMenuDefaultTest::rightClickMapsToExpectedTrigger()
{
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(1, 1), QPointF(1, 1),
                      Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QCOMPARE(Plasma::ContainmentActions::eventToString(&press),
             QStringLiteral("RightButton;NoModifier"));
}

//! The shipped defaults must map RightButton to Latte's standard context menu plugin; the View's
//! hard fallback has to match this id.
void ContextMenuDefaultTest::shippedDefaultsDeclareLatteContextMenu()
{
    const QString src = readSource(QStringLiteral(LATTE_DEFAULTS_PATH));
    QVERIFY2(!src.isEmpty(), "shell defaults file must be readable.");
    QVERIFY2(src.contains(QStringLiteral("RightButton;NoModifier=org.kde.latte.contextmenu")),
             "shell defaults must map RightButton;NoModifier to org.kde.latte.contextmenu.");
}

//! The View must install that default action so the menu works even when the corona config carries
//! no [ActionPlugins] group (the Plasma 6 regression that left containmentActions() empty).
void ContextMenuDefaultTest::viewInstallsDefaultRightButtonAction()
{
    const QString src = readSource(QStringLiteral(VIEW_CPP_PATH));
    QVERIFY2(!src.isEmpty(), "view.cpp must be readable.");
    QVERIFY2(src.contains(QStringLiteral("setContainmentActions")),
             "View must register a containment action.");
    QVERIFY2(src.contains(QStringLiteral("RightButton;NoModifier")),
             "View must register the RightButton;NoModifier trigger.");
    QVERIFY2(src.contains(QStringLiteral("org.kde.latte.contextmenu")),
             "View must register Latte's standard context menu as the default.");
}

//! KF6 derives a plugin's id from its .so file name, not from the embedded KPlugin/Id field. The
//! configs, the shipped defaults and the View all refer to the plugin as "org.kde.latte.contextmenu",
//! so the built library must be named org.kde.latte.contextmenu.so or libplasma's lookup-by-id fails
//! and right-click loads no menu.
void ContextMenuDefaultTest::contextMenuPluginIsNamedAfterItsConfigId()
{
    QFileInfo plugin(QStringLiteral(LATTE_CONTEXTMENU_PLUGIN));
    QVERIFY2(plugin.exists(),
             qPrintable(QStringLiteral("The context-menu plugin must build as org.kde.latte.contextmenu.so "
                                       "(KF6 derives the plugin id from the file name). Expected: %1")
                            .arg(plugin.filePath())));
}

QTEST_MAIN(ContextMenuDefaultTest)
#include "contextmenudefaulttest.moc"
