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

class AppletRemovalTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void removesContainerWhenAppletAlreadyDestroyed();
};

void AppletRemovalTest::removesContainerWhenAppletAlreadyDestroyed()
{
    auto *corona = new TestCorona();
    Plasma::Containment *cont = corona->createContainment(QStringLiteral("org.kde.plasma.desktopcontainment"));
    if (!cont) {
        QSKIP("desktop containment package not installed; cannot build the real stack.");
    }
    corona->setImmutability(Plasma::Types::Mutable);
    cont->setImmutability(Plasma::Types::Mutable);

    Plasma::Applet *applet = cont->createApplet(QStringLiteral("org.kde.plasma.minimizeall"));
    if (!applet) {
        QSKIP("test applet not installed; cannot build the real stack.");
    }
    auto *graphicItem = PlasmaQuick::AppletQuickItem::itemForApplet(applet);
    QVERIFY2(graphicItem, "itemForApplet returned null for a real applet.");

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
    QGuiApplication app(argc, argv);
    AppletRemovalTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "appletremovaltest.moc"
