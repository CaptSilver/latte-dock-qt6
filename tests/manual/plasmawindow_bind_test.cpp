/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QGuiApplication>
#include <QTimer>
#include <QDebug>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmawindowmanagement.h>

using namespace KWayland::Client;

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);
    // MUST match the indexed .desktop that declares X-KDE-Wayland-Interfaces:
    QGuiApplication::setDesktopFileName(QStringLiteral("org.kde.latte-dock"));

    ConnectionThread *connection = ConnectionThread::fromApplication(&app);
    if (!connection) {
        qWarning() << "FAIL: no Wayland connection";
        return 1;
    }

    Registry *registry = new Registry(&app);
    registry->create(connection);

    QObject::connect(registry, &Registry::plasmaWindowManagementAnnounced,
                     [registry](quint32 name, quint32 version) {
        PlasmaWindowManagement *pwm = registry->createPlasmaWindowManagement(name, version, registry);
        QObject::connect(pwm, &PlasmaWindowManagement::windowCreated, [](PlasmaWindow *w) {
            qInfo() << "  window:" << w->uuid() << w->appId() << w->title()
                    << "active=" << w->isActive() << "desktops=" << w->plasmaVirtualDesktops();
        });
        qInfo() << "PASS: org_kde_plasma_window_management bound (v" << version << ")."
                << "Existing windows:" << pwm->windows().size();
    });

    registry->setup();

    QTimer::singleShot(2000, &app, [&app]() {
        qWarning() << "(if no PASS above) FAIL: interface not advertised - check the .desktop"
                   << "X-KDE-Wayland-Interfaces + a KWin re-index (logout/in or kwin_wayland --replace).";
        app.quit();
    });

    return app.exec();
}
