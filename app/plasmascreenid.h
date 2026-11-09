/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PLASMASCREENID_H
#define PLASMASCREENID_H

// Qt
#include <QtGlobal>

// local
#include "libplasmaapi.h"

// C++
#include <climits>
#include <optional>
#include <utility>

// Plasma
#include <Plasma/Containment>

namespace Latte {

//! Latte keeps screen ids as signed ints and uses -1 for "no screen"
//! (ScreenPool::NOSCREENID). libplasma 6.8 uses unsigned ids and has no "no
//! screen" value of its own: a containment that was never placed holds UINT_MAX,
//! the -1 that createContainment() and loadLayout() pass into the unsigned
//! parameter. Older libplasma uses plain ints and -1. These functions are the only
//! place the domains meet, so a Plasma id never reaches Latte code unchecked and -1
//! never wraps into a plausible Plasma id.
constexpr int NOSCREEN = -1;

//! The type libplasma uses for screen ids: int before 6.8, uint from 6.8. Taken from
//! Containment::screen() so it follows whatever the installed headers declare.
using PlasmaScreenId = decltype(std::declval<const Plasma::Containment &>().screen());

//! What a containment that was never placed holds: -1 as a PlasmaScreenId, which is
//! UINT_MAX when ids are unsigned.
constexpr PlasmaScreenId PLASMA_UNPLACED_SCREEN = static_cast<PlasmaScreenId>(-1);

//! Plasma id as Latte sees it. The unplaced id and anything else that does not fit a
//! non-negative int is no screen.
constexpr int latteScreenId(PlasmaScreenId plasmaId)
{
    const qlonglong wide = plasmaId;
    return wide < 0 || wide > INT_MAX ? NOSCREEN : static_cast<int>(wide);
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
constexpr std::optional<PlasmaScreenId> plasmaScreenId(int latteId)
{
    return latteId < 0 ? std::nullopt : std::optional<PlasmaScreenId>(static_cast<PlasmaScreenId>(latteId));
}

//! The screen a containment holds without asking the corona: what it was last given. This
//! is the answer for code that runs inside Corona::screenForContainment, where asking
//! the containment would ask the corona again.
inline int heldScreenId(const Plasma::Containment *containment)
{
#if LATTE_LIBPLASMA_6_8_API
    return latteScreenId(containment->screen());
#else
    return latteScreenId(containment->lastScreen());
#endif
}

//! The screen a containment is on now. libplasma 6.8 stores it, so that is the held
//! screen. Older libplasma asks the corona; when the corona has no answer this falls
//! back to the last screen the containment was given.
inline int currentScreenIdOf(const Plasma::Containment *containment)
{
#if LATTE_LIBPLASMA_6_8_API
    return heldScreenId(containment);
#else
    const int current = latteScreenId(containment->screen());
    return current != NOSCREEN ? current : heldScreenId(containment);
#endif
}

//! Tells the containment which screen it is on.
//!
//! libplasma 6.8: Containment::screen() is stored state that only changes when someone
//! calls setScreen(); the view does not do it for us. No screen leaves the containment as
//! it is, because Plasma has no way to say "none".
//!
//! Older libplasma has no setScreen(): Containment::screen() asks the corona every time,
//! and Latte::Corona::screenForContainment answers from the views and the pending view
//! updates. There is nothing to push, so the containment only re-reads that answer
//! (reactToScreenChange), which also emits screenChanged and stores lastScreen.
inline void setContainmentScreen(Plasma::Containment *containment, int latteId)
{
#if LATTE_LIBPLASMA_6_8_API
    const std::optional<PlasmaScreenId> id = plasmaScreenId(latteId);

    if (containment && id) {
        containment->setScreen(*id);
    }
#else
    Q_UNUSED(latteId)

    if (containment) {
        containment->reactToScreenChange();
    }
#endif
}

}

#endif
