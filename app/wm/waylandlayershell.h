/*
    SPDX-FileCopyrightText: 2026 Latte Dingo
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef WAYLANDLAYERSHELL_H
#define WAYLANDLAYERSHELL_H

// local
#include <coretypes.h>   // Latte::Types::{Visibility,Alignment}

// Qt
#include <QMargins>
#include <QRect>
#include <QRegion>
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

//! Re-apply ONLY the anchors, exclusive edge, screen and seeded size of an already-configured
//! layer surface for a new @p location/@p alignment — without touching its stacking layer or
//! keyboard policy. The compositor places layer surfaces by anchors, not setPosition(), so this is
//! what actually moves the dock when the user changes its edge or alignment at runtime. Re-running
//! the full configureView() would reset the layer (cover modes use LayerBottom) and focus policy,
//! so this narrower path is used for live re-anchoring; configureView() is built on top of it.
void updateAnchoring(QWindow *window, QScreen *screen,
                     Plasma::Types::Location location, Latte::Types::Alignment alignment);

//! Update the stacking layer of an (already layer-shell) window from its visibility mode.
void applyLayer(QWindow *window, Latte::Types::Visibility mode);

//! Set whether the layer surface accepts keyboard focus.
void setFocusPolicy(QWindow *window, bool takesFocus);

//! Reserve/release struts on a layer-shell window (0 releases).
void setExclusiveZone(QWindow *window, int zone);

//! Drop all anchors (and the exclusive edge/margins) on a layer surface so the compositor centres
//! it on its output. Used for the settings windows: anchoring them to the dock's edge welds them to
//! whichever edge the dock had when they opened, so they get stuck there when the dock is moved.
void setUnanchored(QWindow *window);

//! The anchors + margins that make a layer surface overlay the edit-mode canvas exactly.
struct CanvasPlacement {
    LayerShellQt::Window::Anchors anchors;
    QMargins margins;
};

//! Map a dock's canvasGeometry (from Positioner::updateCanvasGeometry) to the layer-shell anchors and
//! margins that reproduce it EXACTLY, so the edit-mode grid overlays the dock instead of landing at
//! the compositor's default centred spot. Unlike a config view, the canvas sits ON the edge (no
//! offset). Horizontal docks span the full screen width -> anchor the edge + both length edges, zero
//! margin. Vertical docks start at the available area's top (e.g. below a top panel), not the screen
//! top -> anchor the edge + top and push down with a top margin = canvasGeometry.y() - screen.y();
//! the surface's explicit height carries the rest. A margin only bites on an anchored edge, so the
//! top anchor is required for the vertical offset. Pure mapping, unit-tested.
CanvasPlacement canvasPlacement(Plasma::Types::Location location,
                                const QRect &canvasGeometry, const QRect &screenGeometry);

//! Anchor @p window to overlay @p canvasGeometry exactly (see canvasPlacement()). Replaces the
//! setPosition() the compositor ignores for the edit-mode canvas view on Wayland.
void applyCanvasPlacement(QWindow *window, Plasma::Types::Location location,
                          const QRect &canvasGeometry, const QRect &screenGeometry);

//! The input region (window-local, for QWindow::setMask) the edit-mode canvas should catch pointer
//! events in. The canvas surface overlays the dock exactly and sits above it, so a full input region
//! eats every right-click/drag/wheel the dock's widgets need. In configure-applets mode the dock's
//! area MUST be click-through so those events reach the dock beneath; only the interactive chrome
//! (the max-length ruler / header) keeps grabbing — an empty @p interactiveChrome makes the whole
//! canvas click-through. In plain edit mode the whole canvas catches input (wheel->background opacity,
//! ruler, context menu). Pure mapping, unit-tested. CanvasConfigView applies the result via setMask().
QRegion canvasInputRegion(bool inConfigureAppletsMode, const QSize &canvasSize,
                          const QRect &interactiveChrome);

} // namespace LayerShell
} // namespace WindowSystem
} // namespace Latte

#endif
