/*
    SPDX-FileCopyrightText: 2018 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "canvasconfigview.h"

// local
#include "primaryconfigview.h"
#include "../panelshadows_p.h"
#include "../view.h"
#include "../../lattecorona.h"
#include "../../settings/universalsettings.h"
#include "../../wm/abstractwindowinterface.h"
#include "../../wm/waylandlayershell.h"

// Qt
#include <QQuickItem>
#include <QScreen>

// KDE
#include <KWindowSystem>
#include <KWayland/Client/plasmashell.h>

// Plasma
#include <KPackage/Package>

namespace Latte {
namespace ViewPart {

CanvasConfigView::CanvasConfigView(Latte::View *view, PrimaryConfigView *parent)
    : SubConfigView(view, QStringLiteral("#canvasconfigview#"), false),
      m_parent(parent)
{
    setResizeMode(QQuickView::SizeRootObjectToView);

    setParentView(view);
    init();
}

void CanvasConfigView::init()
{
    SubConfigView::init();

    QByteArray tempFilePath = "canvasconfigurationui";

    updateEnabledBorders();

    auto source = QUrl::fromLocalFile(m_latteView->containment()->corona()->kPackage().filePath(tempFilePath));
    setSource(source);
    syncGeometry();

    //! Re-carve the input region whenever the rearrange (configure-applets) mode flips or the canvas
    //! is resized; a resize resets the mask, and the mode change must add/remove the click-through band.
    if (m_corona && m_corona->universalSettings()) {
        connect(m_corona->universalSettings(), &Latte::UniversalSettings::inConfigureAppletsModeChanged,
                this, &CanvasConfigView::updateInputRegion);
    }
    connect(this, &QQuickView::widthChanged, this, &CanvasConfigView::updateInputRegion);
    connect(this, &QQuickView::heightChanged, this, &CanvasConfigView::updateInputRegion);

    if (m_parent && KWindowSystem::isPlatformX11()) {
        m_parent->requestActivate();
    }
}

QRect CanvasConfigView::geometryWhenVisible() const
{
    return m_geometryWhenVisible;
}

void CanvasConfigView::initParentView(Latte::View *view)
{
    SubConfigView::initParentView(view);

    rootContext()->setContextProperty(QStringLiteral("primaryConfigView"), m_parent);

    updateEnabledBorders();
    syncGeometry();
}

void CanvasConfigView::syncGeometry()
{
    if (!m_latteView || !m_latteView->layout() || !m_latteView->containment() || !m_parent || !rootObject()) {
        return;
    }

    updateEnabledBorders();

    auto geometry = m_latteView->positioner()->canvasGeometry();

    if (m_geometryWhenVisible == geometry) {
        return;
    }

    m_geometryWhenVisible = geometry;

    if (KWindowSystem::isPlatformWayland()) {
        //! layer-shell ignores setPosition(); the canvas is configured Center-anchored by
        //! SubConfigView so it would land centred on top of the dock. Anchor it to overlay the
        //! dock's canvasGeometry exactly instead. The canvas then covers the dock, so in
        //! configure-applets mode updateInputRegion() carves its input region down to the chrome
        //! strip (and the blueprint goes transparent) to let pointer events reach the widgets.
        Latte::WindowSystem::LayerShell::applyCanvasPlacement(this, m_latteView->location(), geometry, m_latteView->screenGeometry());
    } else {
        setPosition(geometry.topLeft());
    }

    setMaximumSize(geometry.size());
    setMinimumSize(geometry.size());
    resize(geometry.size());

    updateInputRegion();

    //! after placement request to activate the main config window in order to avoid
    //! rare cases of closing settings window from secondaryConfigView->focusOutEvent
    if (m_parent && KWindowSystem::isPlatformX11()) {
        m_parent->requestActivate();
    }
}

void CanvasConfigView::updateInputRegion()
{
    if (!m_latteView) {
        return;
    }

    //! Never touch the surface mask before the wayland surface exists (same guard as the sibling
    //! views' updateEffects, https://bugs.kde.org/show_bug.cgi?id=392890).
    if (KWindowSystem::isPlatformWayland() && !isVisible()) {
        return;
    }

    const bool configuring = m_corona && m_corona->universalSettings()
            && m_corona->universalSettings()->inConfigureAppletsMode();

    //! Configure-applets (rearrange) mode: carve the canvas click-through over the widgets, but keep the
    //! rearrange/exit toggle's rect interactive so it can still be clicked (Escape also exits). The QML
    //! surfaces the toggle's canvas-local rect; everything else falls through to the dock and its tooltips.
    QRect toggleRect;
    if (configuring && rootObject()) {
        toggleRect = rootObject()->property("rearrangeToggleRect").toRect();
    }

    if (m_latteView) {
        m_latteView->debugLog(QStringLiteral("CanvasInputRegion cfg=%1 size=%2x%3 toggleRect=(%4,%5 %6x%7)")
                              .arg(configuring ? 1 : 0).arg(size().width()).arg(size().height())
                              .arg(toggleRect.x()).arg(toggleRect.y()).arg(toggleRect.width()).arg(toggleRect.height()));
    }

    //! Carve disabled while diagnosing the flicker: full click-through in configure mode (empty chrome ->
    //! off-surface region). The toggleRect mapping above is logged-only until it is made reliable.
    setMask(Latte::WindowSystem::LayerShell::canvasInputRegion(configuring, size(), QRect()));
}

bool CanvasConfigView::event(QEvent *e)
{
    bool result = SubConfigView::event(e);

    switch (e->type()) {
    case QEvent::Enter:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
        if (m_parent) {
            m_parent->requestActivate();
        }
        break;
    default:
        break;
    }

    return result;
}

void CanvasConfigView::showEvent(QShowEvent *ev)
{
    SubConfigView::showEvent(ev);

    if (!m_latteView) {
        return;
    }

    syncGeometry();

    //! syncGeometry() short-circuits when the geometry is unchanged, and its earlier (init-time) run
    //! happened before the wayland surface existed, so carve the input region explicitly now that the
    //! surface is up.
    updateInputRegion();

    //! show Canvas on top of all other panels/docks and show
    //! its parent view on top afterwards
    m_corona->wm()->setViewExtraFlags(this, true);

    QTimer::singleShot(100, [this]() {
        //! delay execution in order to take influence after last Canvas on top call
        if (m_parent) {
            m_parent->requestActivate();
        }
    });

    m_screenSyncTimer.start();
    QTimer::singleShot(400, this, &CanvasConfigView::syncGeometry);

    Q_EMIT showSignal();
}

void CanvasConfigView::focusOutEvent(QFocusEvent *ev)
{
    Q_UNUSED(ev);

    if (!m_latteView) {
        return;
    }

    const auto *focusWindow = qGuiApp->focusWindow();

    if (focusWindow && (focusWindow->flags().testFlag(Qt::Popup)
                         || focusWindow->flags().testFlag(Qt::ToolTip))) {
        return;
    }

    const auto parent = qobject_cast<PrimaryConfigView *>(m_parent);

    if (!parent->hasFocus()) {
        parent->hideConfigWindow();
    }
}

void CanvasConfigView::hideConfigWindow()
{
    if (KWindowSystem::isPlatformWayland()) {
        //!NOTE: Avoid crash in wayland environment with qt5.9
        close();
    } else {
        hide();
    }
}

//!BEGIN borders
void CanvasConfigView::updateEnabledBorders()
{
    if (!this->screen()) {
        return;
    }

    KSvg::FrameSvg::EnabledBorders borders = KSvg::FrameSvg::TopBorder;

    switch (m_latteView->location()) {
    case Plasma::Types::TopEdge:
        borders = KSvg::FrameSvg::BottomBorder;
        break;

    case Plasma::Types::LeftEdge:
        borders = KSvg::FrameSvg::RightBorder;
        break;

    case Plasma::Types::RightEdge:
        borders = KSvg::FrameSvg::LeftBorder;
        break;

    case Plasma::Types::BottomEdge:
        borders = KSvg::FrameSvg::TopBorder;
        break;

    default:
        break;
    }

    if (m_enabledBorders != borders) {
        m_enabledBorders = borders;
        m_corona->dialogShadows()->addWindow(this, m_enabledBorders);

        Q_EMIT enabledBordersChanged();
    }
}

//!END borders

}
}

