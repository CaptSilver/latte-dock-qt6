/*
    SPDX-FileCopyrightText: 2026 Latte Dingo
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef WAYLANDLAYERSHELL_H
#define WAYLANDLAYERSHELL_H

// local
#include <coretypes.h>   // Latte::Types::{Visibility,Alignment}

// Qt
#include <QRect>
#include <QSize>

// Plasma
#include <Plasma/Plasma>  // Plasma::Types::Location

// LayerShellQt
#include <LayerShellQt/Window>

class QScreen;
class QWindow;

namespace Latte {
namespace WindowSystem {

/**
 * Configures a QWindow as a wlr-layer-shell surface via LayerShellQt, replacing
 * the deprecated plasma-shell PlasmaShellSurface path. The pure mapping helpers
 * carry no window state and are unit-tested; the configure helpers apply the
 * mapping to a live QWindow and MUST run before the window is first shown.
 */
namespace LayerShell {

//! Edge + alignment -> layer-shell anchor flags.
LayerShellQt::Window::Anchors anchorsFor(Plasma::Types::Location location,
                                         Latte::Types::Alignment alignment);

//! Latte visibility mode -> stacking layer.
LayerShellQt::Window::Layer layerFor(Latte::Types::Visibility mode);

//! The single edge the dock pins to (for setExclusiveEdge).
LayerShellQt::Window::Anchor edgeFor(Plasma::Types::Location location);

//! Perpendicular thickness (px) to reserve as the exclusive zone (struts).
int exclusiveZoneFor(const QRect &strutRect, Plasma::Types::Location location);

//! wlr-layer-shell rejects a surface whose size is 0 on an axis its anchors do not span. Given the
//! current window size and the screen size, returns a legal initial size for a surface anchored per
//! @p anchors at @p location: the length axis is seeded from the screen, the thickness axis to 1px,
//! and an axis the anchors already span is left untouched (the compositor stretches it). Pure;
//! unit-tested. configureView() applies the result before the window is shown.
QSize seededLayerSize(LayerShellQt::Window::Anchors anchors, Plasma::Types::Location location,
                      const QSize &currentSize, const QSize &screenSize);

//! Make @p window a layer surface anchored for @p location/@p alignment on @p screen.
//! MUST be called before @p window is first shown.
void configureView(QWindow *window, QScreen *screen,
                   Plasma::Types::Location location, Latte::Types::Alignment alignment);

//! Update the stacking layer of an (already layer-shell) window from its visibility mode.
void applyLayer(QWindow *window, Latte::Types::Visibility mode);

//! Set whether the layer surface accepts keyboard focus.
void setFocusPolicy(QWindow *window, bool takesFocus);

//! Reserve/release struts on a layer-shell window (0 releases).
void setExclusiveZone(QWindow *window, int zone);

} // namespace LayerShell
} // namespace WindowSystem
} // namespace Latte

#endif
