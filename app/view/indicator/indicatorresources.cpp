/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indicatorresources.h"
#include "indicator.h"

// Qt
#include <QDebug>
#include <QFileInfo>

// Plasma
#include <KSvg/Svg>

namespace Latte {
namespace ViewPart {
namespace IndicatorPart {

Resources::Resources(Indicator *parent) :
    QObject(parent),
    m_indicator(parent)
{
}

Resources::~Resources()
{
}

QList<QObject *> Resources::svgs() const
{
    return m_svgs;
}

void Resources::setSvgImagePaths(QStringList paths)
{
    if (m_svgImagePaths == paths) {
        return;
    }

    while (!m_svgs.isEmpty()) {
        auto svg = m_svgs[0];
        m_svgs.removeFirst();
        svg->deleteLater();
    }

    for(const auto &relPath : paths) {
        if (!relPath.isEmpty()) {
            KSvg::Svg *svg = new KSvg::Svg(this);

            bool isLocalFile = relPath.contains(QLatin1Char('.')) && !relPath.startsWith(QLatin1String("file:"));

            QString adjustedPath = isLocalFile ? m_indicator->uiPath() + QLatin1Char('/') + relPath : relPath;

            if ( !isLocalFile
                 || (isLocalFile && QFileInfo(adjustedPath).exists()) ) {
                svg->setImagePath(adjustedPath);
                m_svgs << svg;
            }
        }
    }

    Q_EMIT svgsChanged();
}

}
}
}
