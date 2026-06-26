/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ICORONAHOST_H
#define ICORONAHOST_H

// Qt
#include <QList>
#include <QString>

class KConfig;
class KConfigGroup;

namespace Plasma {
class Containment;
}
namespace KPackage {
class Package;
}

namespace Latte {

class Corona;

//! Back-reference the Plasma::Corona shell implements, so the engine can reach the handful of
//! base-class calls (and emit the base's screen-level signals) without BEING a Plasma::Corona.
//! The host* prefix avoids colliding with the same-named non-virtual Plasma::Corona methods
//! when Corona multiply-derives from Plasma::Corona and ICoronaHost.
class ICoronaHost
{
public:
    virtual ~ICoronaHost() = default;

    virtual Latte::Corona *corona() = 0;                              //! the real shell, for collaborators that need it
    virtual QList<Plasma::Containment *> hostContainments() const = 0;
    virtual KConfig *hostConfig() const = 0;
    virtual KPackage::Package hostKPackage() const = 0;
    virtual void hostLoadLayout(const QString &layoutName) = 0;
    virtual void hostImportLayout(const KConfigGroup &group) = 0;
    virtual void emitAvailableScreenRectChanged(int screenId) = 0;
    virtual void emitAvailableScreenRegionChanged(int screenId) = 0;
};

}

#endif
