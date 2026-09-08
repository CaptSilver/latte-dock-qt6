/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Removing a widget from the dock goes Containment::appletRemoved -> LayoutManager::removeAppletItem,
// which must delete the applet's container item. On Plasma 5 that signal arrived with the applet not
// yet marked destroyed(), so removeAppletItem deleted the container immediately. Plasma 6 flipped the
// timing: appletRemoved now fires with destroyed()==true. The old two-phase code treated that as
// "park it and wait for a second call to finish", but nothing ever makes that second call — so the
// container was parked forever and the widget could never be removed.
//
// This builds the real stack (Corona -> Containment -> Applet -> graphic item), drives removeAppletItem
// exactly as Containment.onAppletRemoved does — with the applet already destroyed() — and asserts the
// container is actually taken out of its layout. No mocks of the Plasma API: if the removal regresses,
// this fails.

#include <QTemporaryDir>
#include <QStandardPaths>
#include <QtTest>
#include <QGuiApplication>
#include <QObject>
#include <QQuickItem>

#include "layoutmanager.h"

#include <Plasma/Applet>
#include <Plasma/Containment>
#include <Plasma/Corona>
#include <PlasmaQuick/AppletQuickItem>

class TestCorona : public Plasma::Corona
{
public:
    using Plasma::Corona::Corona;
    QRect screenGeometry(int) const override { return QRect(0, 0, 1920, 1080); }
};

//! Throwaway config home, armed in main() before QGuiApplication exists.
static QTemporaryDir s_xdgConfig;

class AppletRemovalTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void removesContainerWhenAppletAlreadyDestroyed();
};

void AppletRemovalTest::removesContainerWhenAppletAlreadyDestroyed()
{
    QVERIFY2(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation).startsWith(s_xdgConfig.path()),
             "config home escaped the sandbox");

    // Both packages below come from plasma-desktop, and a missing one does not show up as a null
    // pointer: createContainment() documents that an unresolvable plugin still yields a containment,
    // just with invalid metadata, and loadApplet() "should never return nullptr". pluginMetaData()
    // is the only thing that says whether the package resolved. Skip rather than fail — a build
    // without plasma-desktop installed has nothing to say about widget removal.
    const QString containmentId = QStringLiteral("org.kde.desktopcontainment");
    const QString appletId = QStringLiteral("org.kde.plasma.minimizeall");

    auto *corona = new TestCorona();
    Plasma::Containment *cont = corona->createContainment(containmentId);
    if (!cont->pluginMetaData().isValid()) {
        QSKIP("plasma-desktop's org.kde.desktopcontainment is not installed; cannot build the real stack.");
    }
    //! The substitution is silent, so pin down that we got the containment we asked for rather than
    //! a nameless placeholder standing in for a misspelt id.
    QCOMPARE(cont->pluginMetaData().pluginId(), containmentId);

    corona->setImmutability(Plasma::Types::Mutable);
    cont->setImmutability(Plasma::Types::Mutable);

    Plasma::Applet *applet = cont->createApplet(appletId);
    //! An unresolved applet keeps the requested pluginId, so isValid() is the only thing that
    //! distinguishes a real widget from a placeholder here.
    if (!applet->pluginMetaData().isValid()) {
        QSKIP("plasma-desktop's org.kde.plasma.minimizeall is not installed; cannot build the real stack.");
    }
    auto *graphicItem = PlasmaQuick::AppletQuickItem::itemForApplet(applet);
    QVERIFY2(graphicItem, "the applet package resolved but Plasma built no graphic item for it.");

    // The layout tree LayoutManager edits, with the applet's container parked in the main layout.
    QQuickItem root, startLayout, mainLayout, endLayout;
    auto *container = new QQuickItem(&mainLayout);
    container->setProperty("applet", QVariant::fromValue<QObject *>(graphicItem));
    container->setProperty("isInternalViewSplitter", false);

    Latte::Containment::LayoutManager lm;
    lm.setPlasmoid(cont); // gives save() a real configuration map
    lm.setRootItem(&root);
    lm.setStartLayout(&startLayout);
    lm.setMainLayout(&mainLayout);
    lm.setEndLayout(&endLayout);

    QCOMPARE(container->parentItem(), &mainLayout);

    // Reproduce Plasma 6's timing: the applet is already destroyed() when appletRemoved fires.
    applet->destroy();
    QVERIFY2(applet->destroyed(),
             "Precondition: on Plasma 6 the applet is marked destroyed() before appletRemoved/removeAppletItem.");

    // Exactly what Containment.onAppletRemoved invokes.
    lm.removeAppletItem(applet);

    // A removed widget's container must be taken out of its layout (reparented to root, deleteLater'd),
    // not left parked in the main layout.
    QVERIFY2(container->parentItem() != &mainLayout,
             "removeAppletItem left the container in its layout — the widget cannot be removed.");
    QCOMPARE(container->parentItem(), &root);
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // The real Plasma::Corona writes its applets rc under the config home; keep
    // that out of the developer's own ~/.config. Only XDG_DATA_* drives package
    // resolution, so the two QSKIP guards above are unaffected.
    qputenv("XDG_CONFIG_HOME", s_xdgConfig.path().toUtf8());
    //! Latte resolves packages through XDG_DATA_HOME too, so a shell package installed under
    //! ~/.local/share/plasma/shells would shadow the staged one and fail this test for reasons
    //! unrelated to the code. Redirect it alongside the config dir.
    qputenv("XDG_DATA_HOME", s_xdgConfig.path().toUtf8());
    QGuiApplication app(argc, argv);
    AppletRemovalTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "appletremovaltest.moc"
