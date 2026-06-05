/*
    SPDX-FileCopyrightText: 2018 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "theme.h"

// local
#include "lattecorona.h"
#include "panelbackground.h"
#include "../../layouts/importer.h"
#include "../../view/panelshadows_p.h"
#include "../../wm/schemecolors.h"
#include "../../tools/commontools.h"

// Qt
#include <QDebug>
#include <QDir>
#include <QPainter>

// KDE
#include <KDirWatch>
#include <KConfigGroup>
#include <KSharedConfig>

// X11
#include <KWindowSystem>

#define DEFAULTCOLORSCHEME QStringLiteral("default.colors")
#define REVERSEDCOLORSCHEME QStringLiteral("reversed.colors")

namespace Latte {
namespace PlasmaExtended {

Theme::Theme(KSharedConfig::Ptr config, QObject *parent) :
    QObject(parent),
    m_themeGroup(KConfigGroup(config, QStringLiteral("PlasmaThemeExtended"))),
    m_backgroundTopEdge(new PanelBackground(Plasma::Types::TopEdge, this)),
    m_backgroundLeftEdge(new PanelBackground(Plasma::Types::LeftEdge, this)),
    m_backgroundBottomEdge(new PanelBackground(Plasma::Types::BottomEdge, this)),
    m_backgroundRightEdge(new PanelBackground(Plasma::Types::RightEdge, this))
{
    qmlRegisterTypes();

    m_corona = qobject_cast<Latte::Corona *>(parent);

    //! compositing tracking — Wayland always composites
    m_compositing = true;
    //!

    loadConfig();

    connect(this, &Theme::compositingChanged, this, &Theme::updateBackgrounds);
    connect(this, &Theme::outlineWidthChanged, this, &Theme::saveConfig);

    connect(&m_theme, &KSvg::ImageSet::imageSetChanged, this, [this](const QString &) { load(); });
    connect(&m_theme, &KSvg::ImageSet::imageSetChanged, this, [this](const QString &) { Q_EMIT themeChanged(); });
}

void Theme::load()
{
    loadThemePaths();
    updateBackgrounds();
    updateMarginsAreaValues();
}

Theme::~Theme()
{
    saveConfig();

    m_defaultScheme->deleteLater();
    m_reversedScheme->deleteLater();
}

bool Theme::hasShadow() const
{
    return m_hasShadow;
}

bool Theme::isLightTheme() const
{
    return m_isLightTheme;
}

bool Theme::isDarkTheme() const
{
    return !m_isLightTheme;
}

int Theme::outlineWidth() const
{
    return m_outlineWidth;
}

void Theme::setOutlineWidth(int width)
{
    if (m_outlineWidth == width) {
        return;
    }

    m_outlineWidth = width;
    Q_EMIT outlineWidthChanged();
}

int Theme::marginsAreaTop() const
{
    return m_marginsAreaTop;
}

int Theme::marginsAreaLeft() const
{
    return m_marginsAreaLeft;
}

int Theme::marginsAreaBottom() const
{
    return m_marginsAreaBottom;
}

int Theme::marginsAreaRight() const
{
    return m_marginsAreaRight;
}


PanelBackground *Theme::backgroundTopEdge() const
{
    return m_backgroundTopEdge;
}

PanelBackground *Theme::backgroundLeftEdge() const
{
    return m_backgroundLeftEdge;
}

PanelBackground *Theme::backgroundBottomEdge() const
{
    return m_backgroundBottomEdge;
}

PanelBackground *Theme::backgroundRightEdge() const
{
    return m_backgroundRightEdge;
}

WindowSystem::SchemeColors *Theme::defaultTheme() const
{
    return m_defaultScheme;
}

WindowSystem::SchemeColors *Theme::lightTheme() const
{
    return m_isLightTheme ? m_defaultScheme : m_reversedScheme;
}

WindowSystem::SchemeColors *Theme::darkTheme() const
{
    return !m_isLightTheme ? m_defaultScheme : m_reversedScheme;
}


void Theme::setOriginalSchemeFile(const QString &file)
{
    if (m_originalSchemePath == file) {
        return;
    }

    m_originalSchemePath = file;

    qDebug() << "plasma theme original colors ::: " << m_originalSchemePath;

    updateDefaultScheme();
    updateReversedScheme();

    loadThemeLightness();

    Q_EMIT themeChanged();
}

//! WM records need to be updated based on the colors that
//! plasma will use in order to be consistent. Such an example
//! are the Breeze color schemes that have different values for
//! WM and the plasma theme records
void Theme::updateDefaultScheme()
{
    QString defaultFilePath = m_extendedThemeDir.path() + QLatin1Char('/') + DEFAULTCOLORSCHEME;
    if (QFileInfo(defaultFilePath).exists()) {
        QFile(defaultFilePath).remove();
    }

    QFile(m_originalSchemePath).copy(defaultFilePath);
    m_defaultSchemePath = defaultFilePath;

    updateDefaultSchemeValues();

    if (m_defaultScheme) {
        disconnect(m_defaultScheme, &WindowSystem::SchemeColors::colorsChanged, this, &Theme::loadThemeLightness);
        m_defaultScheme->deleteLater();
    }

    m_defaultScheme = new WindowSystem::SchemeColors(this, m_defaultSchemePath, true);
    connect(m_defaultScheme, &WindowSystem::SchemeColors::colorsChanged, this, &Theme::loadThemeLightness);

    qDebug() << "plasma theme default colors ::: " << m_defaultSchemePath;
}

void Theme::updateDefaultSchemeValues()
{
    //! update WM values based on original scheme
    KSharedConfigPtr originalPtr = KSharedConfig::openConfig(m_originalSchemePath);
    KSharedConfigPtr defaultPtr = KSharedConfig::openConfig(m_defaultSchemePath);

    if (originalPtr && defaultPtr) {
        KConfigGroup normalWindowGroup(originalPtr, QStringLiteral("Colors:Window"));
        KConfigGroup defaultWMGroup(defaultPtr, QStringLiteral("WM"));

        defaultWMGroup.writeEntry(QStringLiteral("activeBackground"), normalWindowGroup.readEntry(QStringLiteral("BackgroundNormal"), QColor()));
        defaultWMGroup.writeEntry(QStringLiteral("activeForeground"), normalWindowGroup.readEntry(QStringLiteral("ForegroundNormal"), QColor()));

        defaultWMGroup.sync();
    }
}

void Theme::updateReversedScheme()
{
    QString reversedFilePath = m_extendedThemeDir.path() + QLatin1Char('/') + REVERSEDCOLORSCHEME;

    if (QFileInfo(reversedFilePath).exists()) {
        QFile(reversedFilePath).remove();
    }

    QFile(m_originalSchemePath).copy(reversedFilePath);
    m_reversedSchemePath = reversedFilePath;

    updateReversedSchemeValues();

    if (m_reversedScheme) {
        m_reversedScheme->deleteLater();
    }

    m_reversedScheme = new WindowSystem::SchemeColors(this, m_reversedSchemePath, true);

    qDebug() << "plasma theme reversed colors ::: " << m_reversedSchemePath;
}

void Theme::updateReversedSchemeValues()
{
    //! reverse values based on original scheme
    KSharedConfigPtr originalPtr = KSharedConfig::openConfig(m_originalSchemePath);
    KSharedConfigPtr reversedPtr = KSharedConfig::openConfig(m_reversedSchemePath);

    if (originalPtr && reversedPtr) {
        for (const auto &groupName : reversedPtr->groupList()) {
            if (groupName != QLatin1String("Colors:Button") && groupName != QLatin1String("Colors:Selection")) {
                KConfigGroup reversedGroup(reversedPtr, groupName);

                if (reversedGroup.keyList().contains(QStringLiteral("BackgroundNormal"))
                        && reversedGroup.keyList().contains(QStringLiteral("ForegroundNormal"))) {
                    //! reverse usual text/background values
                    KConfigGroup originalGroup(originalPtr, groupName);

                    reversedGroup.writeEntry(QStringLiteral("BackgroundNormal"), originalGroup.readEntry(QStringLiteral("ForegroundNormal"), QColor()));
                    reversedGroup.writeEntry(QStringLiteral("ForegroundNormal"), originalGroup.readEntry(QStringLiteral("BackgroundNormal"), QColor()));

                    reversedGroup.sync();
                }
            }
        }

        //! update WM group
        KConfigGroup reversedWMGroup(reversedPtr, QStringLiteral("WM"));
        KConfigGroup normalWindowGroup(originalPtr, QStringLiteral("Colors:Window"));

        if (reversedWMGroup.keyList().contains(QStringLiteral("activeBackground"))
                && reversedWMGroup.keyList().contains(QStringLiteral("activeForeground"))
                && reversedWMGroup.keyList().contains(QStringLiteral("inactiveBackground"))
                && reversedWMGroup.keyList().contains(QStringLiteral("inactiveForeground"))) {
            //! reverse usual wm titlebar values
            KConfigGroup originalGroup(originalPtr, QStringLiteral("WM"));
            reversedWMGroup.writeEntry(QStringLiteral("activeBackground"), normalWindowGroup.readEntry(QStringLiteral("ForegroundNormal"), QColor()));
            reversedWMGroup.writeEntry(QStringLiteral("activeForeground"), normalWindowGroup.readEntry(QStringLiteral("BackgroundNormal"), QColor()));
            reversedWMGroup.writeEntry(QStringLiteral("inactiveBackground"), originalGroup.readEntry(QStringLiteral("inactiveForeground"), QColor()));
            reversedWMGroup.writeEntry(QStringLiteral("inactiveForeground"), originalGroup.readEntry(QStringLiteral("inactiveBackground"), QColor()));
            reversedWMGroup.sync();
        }

        if (reversedWMGroup.keyList().contains(QStringLiteral("activeBlend"))
                && reversedWMGroup.keyList().contains(QStringLiteral("inactiveBlend"))) {
            KConfigGroup originalGroup(originalPtr, QStringLiteral("WM"));
            reversedWMGroup.writeEntry(QStringLiteral("activeBlend"), originalGroup.readEntry(QStringLiteral("inactiveBlend"), QColor()));
            reversedWMGroup.writeEntry(QStringLiteral("inactiveBlend"), originalGroup.readEntry(QStringLiteral("activeBlend"), QColor()));
            reversedWMGroup.sync();
        }

        //! update scheme name
        QString originalSchemeName = WindowSystem::SchemeColors::schemeName(m_originalSchemePath);
        KConfigGroup generalGroup(reversedPtr, QStringLiteral("General"));
        generalGroup.writeEntry(QStringLiteral("Name"), QString(originalSchemeName + QStringLiteral("_reversed")));
        generalGroup.sync();
    }
}

void Theme::updateBackgrounds()
{
    updateHasShadow();

    m_backgroundTopEdge->update();
    m_backgroundLeftEdge->update();
    m_backgroundBottomEdge->update();
    m_backgroundRightEdge->update();
}

void Theme::updateHasShadow()
{
    KSvg::Svg *svg = new KSvg::Svg(this);
    svg->setImagePath(QStringLiteral("widgets/panel-background"));
    svg->resize();

    QString cornerId = QStringLiteral("shadow-topleft");
    QImage corner = svg->image(svg->elementSize(cornerId).toSize(), cornerId);

    int fullTransparentPixels = 0;

    for(int c=0; c<corner.width(); ++c) {
        for(int r=0; r<corner.height(); ++r) {
            QRgb *line = (QRgb *)corner.scanLine(r);
            QRgb point = line[c];

            if (qAlpha(point) == 0) {
                fullTransparentPixels++;
            }
        }
    }

    int pixels = (corner.width() * corner.height());

    m_hasShadow = (fullTransparentPixels != pixels );
    Q_EMIT hasShadowChanged();

    qDebug() << "  PLASMA THEME TOPLEFT SHADOW :: pixels : " << pixels << "  transparent pixels" << fullTransparentPixels << " | HAS SHADOWS :" << m_hasShadow;

    svg->deleteLater();
}

void Theme::loadThemePaths()
{
    m_themePath = Layouts::Importer::standardPath(QStringLiteral("plasma/desktoptheme/") + m_theme.imageSetName());

    if (QDir(m_themePath + QStringLiteral("/widgets")).exists()) {
        m_themeWidgetsPath = m_themePath + QStringLiteral("/widgets");
    } else {
        m_themeWidgetsPath = Layouts::Importer::standardPath(QStringLiteral("plasma/desktoptheme/default/widgets"));
    }

    qDebug() << "current plasma theme ::: " << m_theme.imageSetName();
    qDebug() << "theme path ::: " << m_themePath;
    qDebug() << "theme widgets path ::: " << m_themeWidgetsPath;

    //! clear kde connections
    for (auto &c : m_kdeConnections) {
        disconnect(c);
    }

    //! assign color schemes
    QString themeColorScheme = m_themePath + QStringLiteral("/colors");

    if (QFileInfo(themeColorScheme).exists()) {
        setOriginalSchemeFile(themeColorScheme);
    } else {
        //! when plasma theme uses the kde colors
        //! we track when kde color scheme is changing
        QString kdeSettingsFile = Latte::configPath() + QStringLiteral("/kdeglobals");

        KDirWatch::self()->addFile(kdeSettingsFile);

        m_kdeConnections[0] = connect(KDirWatch::self(), &KDirWatch::dirty, this, [ &, kdeSettingsFile](const QString & path) {
            if (path == kdeSettingsFile) {
                this->setOriginalSchemeFile(WindowSystem::SchemeColors::possibleSchemeFile(QStringLiteral("kdeglobals")));
            }
        });

        m_kdeConnections[1] = connect(KDirWatch::self(), &KDirWatch::created, this, [ &, kdeSettingsFile](const QString & path) {
            if (path == kdeSettingsFile) {
                this->setOriginalSchemeFile(WindowSystem::SchemeColors::possibleSchemeFile(QStringLiteral("kdeglobals")));
            }
        });

        setOriginalSchemeFile(WindowSystem::SchemeColors::possibleSchemeFile(QStringLiteral("kdeglobals")));
    }
}

void Theme::loadThemeLightness()
{
    float textColorLum = Latte::colorLumina(m_defaultScheme->textColor());
    float backColorLum = Latte::colorLumina(m_defaultScheme->backgroundColor());

    if (backColorLum > textColorLum) {
        m_isLightTheme = true;
    } else {
        m_isLightTheme = false;
    }

    if (m_isLightTheme) {
        qDebug() << "Plasma theme is light...";
    } else {
        qDebug() << "Plasma theme is dark...";
    }
}

const CornerRegions &Theme::cornersMask(const int &radius)
{
    if (m_cornerRegions.contains(radius)) {
        return m_cornerRegions[radius];
    }

    qDebug() << radius;
    CornerRegions corners;

    int axis = (2 * radius) + 2;
    QImage cornerimage(axis, axis, QImage::Format_ARGB32);
    QPainter painter(&cornerimage);
    //!does not provide valid masks ?
    painter.setRenderHints(QPainter::Antialiasing);

    QPen pen(Qt::black);
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(1);
    painter.setPen(pen);

    QRect rectArea(0,0,axis,axis);
    painter.fillRect(rectArea, Qt::white);
    painter.drawRoundedRect(rectArea, axis, axis);

    QRegion topleft;
    for(int y=0; y<radius; ++y) {
        QRgb *line = (QRgb *)cornerimage.scanLine(y);

        QString bits;
        int width{0};
        for(int x=0; x<radius; ++x) {
            QRgb point = line[x];

            if (QColor(point) != Qt::white) {
                bits = bits + QStringLiteral("1 ");
                width = qMax(0, x);
                break;
            } else {
                bits = bits + QStringLiteral("0 ");
            }
        }

        if (width>0) {
            topleft += QRect(0, y, width, 1);
        }

        qDebug()<< "  " << bits;
    }
    corners.topLeft = topleft;

    QTransform transform;
    transform.rotate(90);
    corners.topRight = transform.map(corners.topLeft);
    corners.topRight.translate(corners.topLeft.boundingRect().width(), 0);

    corners.bottomRight = transform.map(corners.topRight);
    corners.bottomRight.translate(corners.topLeft.boundingRect().width(), 0);

    corners.bottomLeft = transform.map(corners.bottomRight);
    corners.bottomLeft.translate(corners.topLeft.boundingRect().width(), 0);

    //qDebug() << " reg top;: " << corners.topLeft;
    //qDebug() << " reg topr: " << corners.topRight;
    //qDebug() << " reg bottomr: " << corners.bottomRight;
    //qDebug() << " reg bottoml: " << corners.bottomLeft;

    m_cornerRegions[radius] = corners;
    return m_cornerRegions[radius];
}

void Theme::updateMarginsAreaValues()
{
    m_marginsAreaTop = 0;
    m_marginsAreaLeft = 0;
    m_marginsAreaBottom = 0;
    m_marginsAreaRight = 0;

    KSvg::Svg *svg = new KSvg::Svg(this);
    svg->setImagePath(QStringLiteral("widgets/panel-background"));

    bool hasThickSeparatorMargins = svg->hasElement(QStringLiteral("thick-center"));

    if (hasThickSeparatorMargins) {
        int topMargin = svg->hasElement(QStringLiteral("hint-top-margin")) ? svg->elementSize(QStringLiteral("hint-top-margin")).height() : 0;
        int leftMargin = svg->hasElement(QStringLiteral("hint-left-margin")) ? svg->elementSize(QStringLiteral("hint-left-margin")).width() : 0;
        int bottomMargin = svg->hasElement(QStringLiteral("hint-bottom-margin")) ? svg->elementSize(QStringLiteral("hint-bottom-margin")).height() : 0;
        int rightMargin = svg->hasElement(QStringLiteral("hint-right-margin")) ? svg->elementSize(QStringLiteral("hint-right-margin")).width() : 0;

        int thickTopMargin = svg->hasElement(QStringLiteral("thick-hint-top-margin")) ? svg->elementSize(QStringLiteral("thick-hint-top-margin")).height() : 0;
        int thickLeftMargin = svg->hasElement(QStringLiteral("thick-hint-left-margin")) ? svg->elementSize(QStringLiteral("thick-hint-left-margin")).width() : 0;
        int thickBottomMargin = svg->hasElement(QStringLiteral("thick-hint-bottom-margin")) ? svg->elementSize(QStringLiteral("thick-hint-bottom-margin")).height() : 0;
        int thickRightMargin = svg->hasElement(QStringLiteral("thick-hint-right-margin")) ? svg->elementSize(QStringLiteral("thick-hint-right-margin")).width() : 0;

        m_marginsAreaTop = qMax(0, thickTopMargin - topMargin);
        m_marginsAreaLeft = qMax(0, thickLeftMargin - leftMargin);
        m_marginsAreaBottom = qMax(0, thickBottomMargin - bottomMargin);
        m_marginsAreaRight = qMax(0, thickRightMargin - rightMargin);
    }

    qDebug() << "PLASMA THEME MARGINS AREA ::" <<
                m_marginsAreaTop << m_marginsAreaLeft <<
                m_marginsAreaBottom << m_marginsAreaRight;

    svg->deleteLater();

    Q_EMIT marginsAreaChanged();
}

void Theme::loadConfig()
{
    setOutlineWidth(m_themeGroup.readEntry("outlineWidth", 1));
}

void Theme::saveConfig()
{
    m_themeGroup.writeEntry("outlineWidth", m_outlineWidth);
}

void Theme::qmlRegisterTypes()
{
    qmlRegisterAnonymousType<Latte::PlasmaExtended::Theme>("latte-dock", 1);
    qmlRegisterAnonymousType<Latte::PlasmaExtended::PanelBackground>("latte-dock", 1);
}

}
}
