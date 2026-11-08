/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PLASMASCREENID_H
#define PLASMASCREENID_H

// Qt
#include <QtGlobal>

// C++
#include <climits>
#include <optional>

// Plasma
#include <Plasma/Containment>

namespace Latte {

//! Latte keeps screen ids as signed ints and uses -1 for "no screen"
//! (ScreenPool::NOSCREENID). libplasma 6.8 uses unsigned ids and has no "no
//! screen" value of its own: a containment that was never placed holds UINT_MAX,
//! the -1 that createContainment() and loadLayout() pass into the unsigned
//! parameter. These two functions are the only place the domains meet, so a
//! Plasma id never reaches Latte code unchecked and -1 never wraps into a
//! plausible Plasma id.
constexpr int NOSCREEN = -1;

//! Plasma id as Latte sees it. UINT_MAX and anything else that does not fit an int
//! is no screen.
constexpr int latteScreenId(uint plasmaId)
{
    return plasmaId > static_cast<uint>(INT_MAX) ? NOSCREEN : static_cast<int>(plasmaId);
}

//! A screen id as it sits in a config file. Latte has always written it as a signed int,
//! -1 for none; libplasma 6.8 writes the unsigned value, so a containment that was never
//! placed reads 4294967295. KConfig happens to wrap that into -1 when asked for an int, but
//! only by accident of the conversion. Read the entry as qlonglong and pass it through here
//! so anything that is not a Latte screen id is no screen on purpose.
constexpr int storedScreenId(qlonglong stored)
{
    return stored < 0 || stored > INT_MAX ? NOSCREEN : static_cast<int>(stored);
}

//! Latte id as Plasma wants it, or nothing when Latte has no screen to offer.
constexpr std::optional<uint> plasmaScreenId(int latteId)
{
    return latteId < 0 ? std::nullopt : std::optional<uint>(static_cast<uint>(latteId));
}

//! Tells the containment which screen it is on. Containment::screen() is stored state
//! in libplasma 6.8 and only changes when someone calls setScreen(); the view does
//! not do it for us. No screen leaves the containment as it is, because Plasma has no
//! way to say "none".
inline void setContainmentScreen(Plasma::Containment *containment, int latteId)
{
    const std::optional<uint> id = plasmaScreenId(latteId);

    if (containment && id) {
        containment->setScreen(*id);
    }
}

}

#endif
