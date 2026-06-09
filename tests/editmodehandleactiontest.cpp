/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The edit-mode applet handle (ConfigOverlay.qml) shows Configure and Remove buttons over a hovered
// applet. On Plasma 5 the handle reached the applet's standard actions with applet.action("remove").
// On Plasma 6 that method is gone: the QML "applet" object is a PlasmoidItem whose .action() does not
// exist, and the real Applet (and its standard actions) hangs off the .plasmoid compatibility
// property via internalAction(name). Calling the dead .action() throws a TypeError that aborts the
// whole visibility handler and the click handler, so the Remove button never works — you cannot
// delete a widget even in edit mode.
//
// This pins the contract: the handle must resolve standard actions through applet.plasmoid
// .internalAction(name) and must never call the removed applet.action() API.

#include <QtTest>
#include <QFile>
#include <QObject>

class EditModeHandleActionTest : public QObject
{
    Q_OBJECT

private:
    QString readSource(const QString &path);

private Q_SLOTS:
    void handleDoesNotUseRemovedAppletActionApi();
    void handleResolvesRemoveThroughInternalAction();
    void handleResolvesConfigureThroughInternalAction();
};

QString EditModeHandleActionTest::readSource(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

//! applet.action(name) does not exist on a Plasma 6 PlasmoidItem; calling it throws and breaks the
//! handle's Configure/Remove buttons. The handle must not use it anywhere.
void EditModeHandleActionTest::handleDoesNotUseRemovedAppletActionApi()
{
    const QString src = readSource(QStringLiteral(CONFIGOVERLAY_QML_PATH));
    QVERIFY2(!src.isEmpty(), "ConfigOverlay.qml must be readable.");
    QVERIFY2(!src.contains(QStringLiteral("applet.action(")),
             "The edit-mode handle must not call the Plasma 5 applet.action() API (removed in "
             "Plasma 6); use applet.plasmoid.internalAction(name) instead.");
}

//! Remove must be resolved through the Plasma 6 compatibility chain applet.plasmoid.internalAction.
void EditModeHandleActionTest::handleResolvesRemoveThroughInternalAction()
{
    const QString src = readSource(QStringLiteral(CONFIGOVERLAY_QML_PATH));
    QVERIFY2(!src.isEmpty(), "ConfigOverlay.qml must be readable.");
    QVERIFY2(src.contains(QStringLiteral("applet.plasmoid.internalAction(\"remove\")")),
             "The Remove button must resolve via applet.plasmoid.internalAction(\"remove\").");
}

//! Same contract for the Configure button, which shared the broken applet.action() call.
void EditModeHandleActionTest::handleResolvesConfigureThroughInternalAction()
{
    const QString src = readSource(QStringLiteral(CONFIGOVERLAY_QML_PATH));
    QVERIFY2(!src.isEmpty(), "ConfigOverlay.qml must be readable.");
    QVERIFY2(src.contains(QStringLiteral("applet.plasmoid.internalAction(\"configure\")")),
             "The Configure button must resolve via applet.plasmoid.internalAction(\"configure\").");
}

QTEST_MAIN(EditModeHandleActionTest)
#include "editmodehandleactiontest.moc"
