/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "interfaces.h"

#include <PlasmaQuick/AppletQuickItem>

#include <QFile>
#include <QStandardPaths>

#include <cstdio>
#include <fcntl.h>

namespace Latte{

void Interfaces::debugLog(const QString &msg) const
{
    if (!qEnvironmentVariableIsSet("LATTE_DEBUG_EDITMODE")) {
        return;
    }

    //! Open once in the user-private runtime dir (XDG_RUNTIME_DIR, mode 0700), O_NOFOLLOW/0600 to dodge
    //! the predictable-temp-path symlink hazard. Same file as View::debugLog (O_APPEND) so C++ and QML
    //! traces interleave.
    static FILE *logfile = []() -> FILE * {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (dir.isEmpty()) {
            dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        }
        const QString path = dir + QStringLiteral("/latte-editmode.log");
        const int fd = open(QFile::encodeName(path).constData(),
                            O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600);
        return fd >= 0 ? fdopen(fd, "a") : nullptr;
    }();

    if (logfile) {
        fprintf(logfile, "LATTE-DBG %s\n", qPrintable(msg));
        fflush(logfile);
    }
    fprintf(stderr, "LATTE-DBG %s\n", qPrintable(msg));
    fflush(stderr);
}

Interfaces::Interfaces(QObject *parent)
    : QObject(parent)
{
}

QObject *Interfaces::globalShortcuts() const
{
    return m_globalShortcuts;
}

void Interfaces::setGlobalShortcuts(QObject *shortcuts)
{
    if (m_globalShortcuts == shortcuts) {
        return;
    }

    m_globalShortcuts = shortcuts;

    if (m_globalShortcuts) {
        connect(m_globalShortcuts, &QObject::destroyed, this, [&]() {
            setGlobalShortcuts(nullptr);
        });
    }

    Q_EMIT globalShortcutsChanged();
}

QObject *Interfaces::layoutsManager() const
{
    return m_layoutsManager;
}

void Interfaces::setLayoutsManager(QObject *manager)
{
    if (m_layoutsManager == manager) {
        return;
    }

    m_layoutsManager = manager;

    if (m_layoutsManager) {
        connect(m_layoutsManager, &QObject::destroyed, this, [&]() {
            setLayoutsManager(nullptr);
        });
    }

    Q_EMIT layoutsManagerChanged();
}

QObject *Interfaces::themeExtended() const
{
    return m_themeExtended;
}

void Interfaces::setThemeExtended(QObject *theme)
{
    if (m_themeExtended == theme) {
        return;
    }

    m_themeExtended = theme;

    if (m_themeExtended) {
        connect(m_themeExtended, &QObject::destroyed, this, [&]() {
            setThemeExtended(nullptr);
        });
    }

    Q_EMIT themeExtendedChanged();
}

QObject *Interfaces::universalSettings() const
{
    return m_universalSettings;
}

void Interfaces::setUniversalSettings(QObject *settings)
{
    if (m_universalSettings == settings) {
        return;
    }

    m_universalSettings = settings;

    if (m_universalSettings) {
        connect(m_universalSettings, &QObject::destroyed, this, [&]() {
            setUniversalSettings(nullptr);
        });
    }

    Q_EMIT universalSettingsChanged();
}

void Interfaces::updateInterfaces()
{
    //! On Plasma 6 the Latte::View C++ wrapper injects the _latte_* objects onto the graphic
    //! object after this Interfaces was already bound, so the initial read in setPlasmoidInterface
    //! saw nulls. Re-read everything once the View has injected the real objects.
    if (m_plasmoid) {
        setGlobalShortcuts(m_plasmoid->property("_latte_globalShortcuts_object").value<QObject *>());
        setLayoutsManager(m_plasmoid->property("_latte_layoutsManager_object").value<QObject *>());
        setThemeExtended(m_plasmoid->property("_latte_themeExtended_object").value<QObject *>());
        setUniversalSettings(m_plasmoid->property("_latte_universalSettings_object").value<QObject *>());
        setView(m_plasmoid->property("_latte_view_object").value<QObject *>());
    }
}

QObject *Interfaces::view() const
{
    return m_view;
}

void Interfaces::setView(QObject *view)
{
    if (m_view == view) {
        return;
    }

    m_view = view;

    if (m_view) {
        connect(m_view, &QObject::destroyed, this, [&]() {
            setView(nullptr);
        });
    }

    Q_EMIT viewChanged();
}

QObject *Interfaces::plasmoidInterface() const
{
    return m_plasmoid;
}

void Interfaces::setPlasmoidInterface(QObject *interface)
{
    PlasmaQuick::AppletQuickItem *plasmoid = qobject_cast<PlasmaQuick::AppletQuickItem *>(interface);

    if (plasmoid && m_plasmoid != plasmoid) {
        m_plasmoid = plasmoid;

        updateInterfaces();

        Q_EMIT interfaceChanged();
    }
}

}
