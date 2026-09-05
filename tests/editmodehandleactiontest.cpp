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

#include "sourcereader.h"

#include <QtTest>
#include <QObject>
#include <QRegularExpression>

using namespace LatteTest;

static QString configOverlaySource()
{
    return readRepoFile(QStringLiteral("containment/package/contents/ui/editmode/ConfigOverlay.qml"));
}

class EditModeHandleActionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void handleDoesNotUseRemovedAppletActionApi();
    void handleResolvesRemoveThroughInternalAction();
    void handleResolvesConfigureThroughInternalAction();
    void handleButtonsCarryNoAttachedToolTip();
};

//! applet.action(name) does not exist on a Plasma 6 PlasmoidItem; calling it throws and breaks the
//! handle's Configure/Remove buttons. The handle must not use it anywhere.
void EditModeHandleActionTest::handleDoesNotUseRemovedAppletActionApi()
{
    const QString src = configOverlaySource();
    QVERIFY2(!src.isEmpty(), "ConfigOverlay.qml must be readable.");
    QVERIFY2(!src.contains(QStringLiteral("applet.action(")),
             "The edit-mode handle must not call the Plasma 5 applet.action() API (removed in "
             "Plasma 6); use applet.plasmoid.internalAction(name) instead.");
}

//! Remove must be resolved through the Plasma 6 compatibility chain applet.plasmoid.internalAction.
void EditModeHandleActionTest::handleResolvesRemoveThroughInternalAction()
{
    const QString src = configOverlaySource();
    QVERIFY2(!src.isEmpty(), "ConfigOverlay.qml must be readable.");
    QVERIFY2(src.contains(QStringLiteral("applet.plasmoid.internalAction(\"remove\")")),
             "The Remove button must resolve via applet.plasmoid.internalAction(\"remove\").");
}

//! Same contract for the Configure button, which shared the broken applet.action() call.
void EditModeHandleActionTest::handleResolvesConfigureThroughInternalAction()
{
    const QString src = configOverlaySource();
    QVERIFY2(!src.isEmpty(), "ConfigOverlay.qml must be readable.");
    QVERIFY2(src.contains(QStringLiteral("applet.plasmoid.internalAction(\"configure\")")),
             "The Configure button must resolve via applet.plasmoid.internalAction(\"configure\").");
}

//! ConfigOverlay's applet tooltip is a PlasmaCore.Dialog -- its own Wayland surface -- whose
//! visibility is gated by a hideTimer plus the wrapping MouseArea's containsMouse. A handle button
//! carrying an attached QQC2.ToolTip pops a *second* surface at the cursor, so the compositor sends
//! a leave to the button AND to the wrapping MouseArea, the ToolTip hides, the cursor re-enters, and
//! it loops at roughly 20 Hz -- the flicker that made lock/colorize/delete unclickable. Offscreen
//! QPA never surfaces popups, so there is no headless repro; the fix is pinned at the source. If
//! hints are wanted, drive the in-Dialog label instead of a popup.
void EditModeHandleActionTest::handleButtonsCarryNoAttachedToolTip()
{
    const QString src = configOverlaySource();
    QVERIFY2(!src.isEmpty(), "ConfigOverlay.qml must be readable.");

    // Leading [^/\n]* means only a real attached-property line matches; the explanatory comment
    // above the handle Row mentions QQC2.ToolTip and must not trip the guard. The \n in the negated
    // class matters -- unlike line-based grep, a QRegularExpression class would otherwise span lines.
    static const QRegularExpression attachedToolTip(QStringLiteral("^[^/\\n]*QQC2\\.ToolTip\\.(visible|text)"),
                                                    QRegularExpression::MultilineOption);
    QVERIFY2(!attachedToolTip.match(src).hasMatch(),
             "The edit-mode handle buttons must not use an attached QQC2.ToolTip: on Wayland the "
             "popup surface steals hover from the tooltip Dialog and the handle flickers.");
}

QTEST_MAIN(EditModeHandleActionTest)
#include "editmodehandleactiontest.moc"
