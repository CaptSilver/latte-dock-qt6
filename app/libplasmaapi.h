/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LIBPLASMAAPI_H
#define LIBPLASMAAPI_H

// Qt
#include <QtGlobal>

// Plasma
#include <plasma_version.h>

//! The libplasma API that Latte bridges changed in one release series, the 6.8 betas
//! (6.7.90): Corona and Containment use unsigned screen ids, Containment::setScreen
//! exists and Containment::screen() is stored state instead of a question to the corona,
//! PlasmaQuick::globalEngine() replaces SharedQmlEngine, and Corona::importLayout is gone.
//! This is the only place that compares the libplasma version; everything else asks this
//! macro, which is 1 for the new API and 0 for the older one.
#if PLASMA_VERSION >= QT_VERSION_CHECK(6, 7, 90)
#define LATTE_LIBPLASMA_6_8_API 1
#else
#define LATTE_LIBPLASMA_6_8_API 0
#endif

#endif
