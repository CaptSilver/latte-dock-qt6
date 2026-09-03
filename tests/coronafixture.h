/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LATTETESTS_CORONAFIXTURE_H
#define LATTETESTS_CORONAFIXTURE_H

// Bringing up a real Latte::Corona in a test also brings up ScreenPool and
// UniversalSettings, which open and write their rc files under the config
// location. Without a redirect that is the developer's own ~/.config.
//
// This moves the *writable* config dir only. The rest of the search path --
// ~/.config/kdedefaults and /etc/xdg -- is untouched, so reads still see host
// settings. It is not host isolation.
//
// QStandardPaths::setTestModeEnabled() is deliberately not used: it overrides
// XDG_CONFIG_HOME, so it would silently redirect the seven tests that already
// qputenv their own config root, and it writes to one ~/.qttest shared by every
// Qt test binary on the machine, which survives between runs.

#include "../app/lattecorona.h"

#include <QByteArray>
#include <QString>
#include <QTemporaryDir>

struct CoronaSandbox
{
    QTemporaryDir dir;

    //! Returns false rather than asserting so the caller can QVERIFY it.
    bool arm()
    {
        return dir.isValid() && qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
    }
};

//! The Corona owns a large graph; deleting it headlessly re-enters teardown
//! paths that assume a live shell, so every caller leaks it deliberately for
//! the process lifetime rather than risk an at-exit crash that masks results.
inline Latte::Corona *buildHeadlessCorona()
{
    return new Latte::Corona(false, QString(), QString(), 0, nullptr);
}

#endif
