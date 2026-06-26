/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "panelbackground.h"

// local
#include "panelbackgroundscan.h"
#include "theme.h"

// Qt
#include <QDebug>
#include <QImage>
#include <QtGlobal>

#define CENTERWIDTH 100
#define CENTERHEIGHT 50

#define BASELINESHADOWTHRESHOLD 5

namespace Latte {
namespace PlasmaExtended {

PanelBackground::PanelBackground(Plasma::Types::Location edge, Theme *parent)
    : QObject(parent),
      m_location(edge),
      m_parentTheme(parent)
{
}

PanelBackground::~PanelBackground()
{
}

bool PanelBackground::hasMask(KSvg::Svg *svg) const
{
    if (!svg) {
        return false;
    }

    return svg->hasElement(QStringLiteral("mask-topleft"));
}

int PanelBackground::paddingTop() const
{
    return m_paddingTop;
}

int PanelBackground::paddingLeft() const
{
    return m_paddingLeft;
}

int PanelBackground::paddingBottom() const
{
    return m_paddingBottom;
}

int PanelBackground::paddingRight() const
{
    return m_paddingRight;
}

int PanelBackground::roundness() const
{
    return m_roundness;
}

int PanelBackground::shadowSize() const
{
    return m_shadowSize;
}

float PanelBackground::maxOpacity() const
{
    return m_maxOpacity;
}

QColor PanelBackground::shadowColor() const
{
    return m_shadowColor;
}

QString PanelBackground::prefixed(const QString &id)
{
    if (m_location == Plasma::Types::TopEdge) {
        return QStringLiteral("north-") + id;
    } else if (m_location == Plasma::Types::LeftEdge) {
        return QStringLiteral("west-") + id;
    } else if (m_location == Plasma::Types::BottomEdge) {
        return QStringLiteral("south-") + id;
    } else if (m_location == Plasma::Types::RightEdge) {
        return QStringLiteral("east-") + id;
    }

    return id;
}

QString PanelBackground::element(KSvg::Svg *svg, const QString &id)
{
    if (!svg) {
        return QString();
    }

    if (svg->hasElement(prefixed(id))) {
        return prefixed(id);
    }

    if (svg->hasElement(id)) {
        return id;
    }

    return QString();
}

void PanelBackground::updateMaxOpacity(KSvg::Svg *svg)
{
    if (!svg) {
        return;
    }

    QImage center = svg->image(QSize(CENTERWIDTH, CENTERHEIGHT), element(svg, QStringLiteral("center")));
    m_maxOpacity = PanelBackgroundScan::maxOpacityFromCenter(center);
    Q_EMIT maxOpacityChanged();
}

void PanelBackground::updatePaddings(KSvg::Svg *svg)
{
    if (!svg) {
        return;
    }

    m_paddingTop = svg->elementSize(element(svg, QStringLiteral("top"))).height();
    m_paddingLeft = svg->elementSize(element(svg, QStringLiteral("left"))).width();
    m_paddingBottom = svg->elementSize(element(svg, QStringLiteral("bottom"))).height();
    m_paddingRight = svg->elementSize(element(svg, QStringLiteral("right"))).width();

    Q_EMIT paddingsChanged();
}

void PanelBackground::updateRoundnessFromMask(KSvg::Svg *svg)
{
    if (!svg) {
        return;
    }

    bool topLeftCorner = (m_location == Plasma::Types::BottomEdge || m_location == Plasma::Types::RightEdge);
    QString cornerId = (topLeftCorner ? QStringLiteral("mask-topleft") : QStringLiteral("mask-bottomright"));
    QImage corner = svg->image(svg->elementSize(cornerId).toSize(), cornerId);
    m_roundness = PanelBackgroundScan::roundnessFromMaskCorner(corner, topLeftCorner);
    Q_EMIT roundnessChanged();
}

void PanelBackground::updateRoundnessFromShadows(KSvg::Svg *svg)
{
    if (!svg) {
        return;
    }

    bool topLeftCorner = (m_location == Plasma::Types::BottomEdge || m_location == Plasma::Types::RightEdge);
    QString cornerId = (topLeftCorner ? QStringLiteral("shadow-topleft") : QStringLiteral("shadow-bottomright"));
    QImage corner = svg->image(svg->elementSize(cornerId).toSize(), cornerId);
    m_roundness = PanelBackgroundScan::roundnessFromShadowCorner(corner, topLeftCorner);
    Q_EMIT roundnessChanged();
}

void PanelBackground::updateRoundnessFallback(KSvg::Svg *svg)
{
    if (!svg) {
        return;
    }

    QString cornerId = element(svg, (m_location == Plasma::Types::LeftEdge ? QStringLiteral("bottomright") : QStringLiteral("topleft")));
    QImage corner = svg->image(svg->elementSize(cornerId).toSize(), cornerId);

    if (corner.format() != QImage::Format_ARGB32_Premultiplied) {
        corner.convertTo(QImage::Format_ARGB32_Premultiplied);
    }

    int discovRow = (m_location == Plasma::Types::LeftEdge ? corner.height()-1 : 0);
    int discovCol{0};
    //int discovCol = (m_location == Plasma::Types::LeftEdge ? corner.width()-1 : 0);
    int round{0};

    int minOpacity = m_maxOpacity * 255;

    if (m_location == Plasma::Types::BottomEdge || m_location == Plasma::Types::RightEdge || m_location == Plasma::Types::TopEdge) {
        //! TOPLEFT corner
        //! first LEFT pixel found
        QRgb *line = (QRgb *)corner.scanLine(discovRow);

        for (int col=0; col<corner.width() - 1; ++col) {
            QRgb pixelData = line[col];

            if (qAlpha(pixelData) < minOpacity) {
                discovCol++;
                round++;
            } else {
                break;
            }
        }
    } else if (m_location == Plasma::Types::LeftEdge) {
        //! it should be TOPRIGHT corner in that case
        //! first RIGHT pixel found
        QRgb *line = (QRgb *)corner.scanLine(discovRow);
        for (int col=corner.width()-1; col>0; --col) {
            QRgb pixelData = line[col];

            if (qAlpha(pixelData) < minOpacity) {
                discovCol--;
                round++;
            } else {
                break;
            }
        }
    }

    m_roundness = round;
    Q_EMIT roundnessChanged();
}

void PanelBackground::updateShadow(KSvg::Svg *svg)
{
    if (!svg) {
        return;
    }

    const int oldShadowSize = m_shadowSize;
    const QColor oldShadowColor = m_shadowColor;

    if (!m_parentTheme->hasShadow()) {
        m_shadowSize = 0;
        m_shadowColor = Qt::black;

        if (oldShadowSize != m_shadowSize) {
            Q_EMIT shadowSizeChanged();
        }
        if (oldShadowColor != m_shadowColor) {
            Q_EMIT shadowColorChanged();
        }
        return;
    }

    bool horizontal = (m_location == Plasma::Types::BottomEdge || m_location == Plasma::Types::TopEdge);

    QString borderId{QStringLiteral("shadow-top")};

    if  (m_location == Plasma::Types::TopEdge) {
        borderId = QStringLiteral("shadow-bottom");
    } else if (m_location == Plasma::Types::LeftEdge) {
        borderId = QStringLiteral("shadow-right");
    } else if (m_location == Plasma::Types::RightEdge) {
        borderId = QStringLiteral("shadow-left");
    }

    QImage border = svg->image(svg->elementSize(borderId).toSize(), borderId);

    //! find shadow size through, plasma theme
    int themeshadowsize{0};

    if  (m_location == Plasma::Types::TopEdge) {
        themeshadowsize = svg->elementSize(element(svg, QStringLiteral("shadow-hint-bottom-margin"))).height();
    } else if (m_location == Plasma::Types::LeftEdge) {
        themeshadowsize = svg->elementSize(element(svg, QStringLiteral("shadow-hint-right-margin"))).width();
    } else if (m_location == Plasma::Types::RightEdge) {
        themeshadowsize = svg->elementSize(element(svg, QStringLiteral("shadow-hint-left-margin"))).width();
    } else {
        themeshadowsize = svg->elementSize(element(svg, QStringLiteral("shadow-hint-top-margin"))).height();
    }

    auto s = PanelBackgroundScan::shadowFromBorder(border, horizontal);
    m_shadowSize = qMax(themeshadowsize, s.discoveredSize);
    if (s.color.isValid()) {
        m_shadowColor = s.color;
    }

    if (oldShadowSize != m_shadowSize) {
        Q_EMIT shadowSizeChanged();
    }
    if (oldShadowColor != m_shadowColor) {
        Q_EMIT shadowColorChanged();
    }
}


void PanelBackground::updateRoundness(KSvg::Svg *svg)
{
    if (!svg) {
        return;
    }

    if (hasMask(svg)) {
        qDebug() << "PLASMA THEME, calculating roundness from mask...";
        updateRoundnessFromMask(svg);
    } else if (m_parentTheme->hasShadow()) {
        qDebug() << "PLASMA THEME, calculating roundness from shadows...";
        updateRoundnessFromShadows(svg);
    } else {
        qDebug() << "PLASMA THEME, calculating roundness from fallback code...";
        updateRoundnessFallback(svg);
    }
}

void PanelBackground::update()
{
    KSvg::Svg *backSvg = new KSvg::Svg(this);
    backSvg->setImagePath(QStringLiteral("widgets/panel-background"));
    backSvg->resize();

    updateMaxOpacity(backSvg);
    updatePaddings(backSvg);
    updateRoundness(backSvg);
    updateShadow(backSvg);

    qDebug() << " PLASMA THEME EXTENDED :: " << m_location << " | roundness:" << m_roundness << " center_max_opacity:" << m_maxOpacity;
    qDebug() << " PLASMA THEME EXTENDED :: " << m_location
             << " | padtop:" << m_paddingTop << " padleft:" << m_paddingLeft
             << " padbottom:" << m_paddingBottom << " padright:" << m_paddingRight;
    qDebug() << " PLASMA THEME EXTENDED :: " << m_location << " | shadowsize:" << m_shadowSize << " shadowcolor:" << m_shadowColor;

    backSvg->deleteLater();
}

}
}
