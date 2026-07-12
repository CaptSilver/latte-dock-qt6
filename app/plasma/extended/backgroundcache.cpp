/*
    SPDX-FileCopyrightText: 2018 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "backgroundcache.h"

// local
#include "../../tools/commontools.h"
#include "backgroundimageinfo.h"

// Qt
#include <QDebug>
#include <QFileInfo>
#include <QImage>
#include <QList>
#include <QRgb>
#include <QtMath>
#include <QLatin1String>

// Plasma
#include <Plasma/Plasma>

// KDE
#include <KConfigGroup>
#include <KDirWatch>

#define MAXHASHSIZE 300

#define PLASMACONFIG QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc")
#define DEFAULTWALLPAPER QStringLiteral("wallpapers/Next/contents/images/1920x1080.png")

namespace Latte{
namespace PlasmaExtended {

BackgroundCache::BackgroundCache(QObject *parent)
    : QObject(parent),
      m_initialized(false),
      m_plasmaConfig(KSharedConfig::openConfig(PLASMACONFIG))
{
    const auto configFile = QStandardPaths::writableLocation(
                QStandardPaths::GenericConfigLocation) +
            QLatin1Char('/') + PLASMACONFIG;

    m_defaultWallpaperPath = Latte::standardPath(DEFAULTWALLPAPER);

    qDebug() << "Default Wallpaper path ::: " << m_defaultWallpaperPath;

    KDirWatch::self()->addFile(configFile);

    connect(KDirWatch::self(), &KDirWatch::dirty, this, &BackgroundCache::settingsFileChanged);
    connect(KDirWatch::self(), &KDirWatch::created, this, &BackgroundCache::settingsFileChanged);

    if (!m_pool) {
        m_pool = new ScreenPool(this);
        connect(m_pool, &ScreenPool::idsChanged, this, &BackgroundCache::reload);
    }

    reload();
}

BackgroundCache::~BackgroundCache()
{   
    if (m_pool) {
        m_pool->deleteLater();
    }
}

BackgroundCache *BackgroundCache::self()
{
    static BackgroundCache cache;
    return &cache;
}

void BackgroundCache::settingsFileChanged(const QString &file) {
    if (!file.endsWith(PLASMACONFIG)) {
        return;
    }

    if (m_initialized) {
        m_plasmaConfig->reparseConfiguration();
        reload();
    }
}

QString BackgroundCache::backgroundFromConfig(const KConfigGroup &config, QString wallpaperPlugin) const
{
    auto wallpaperConfig = config.group(QStringLiteral("Wallpaper")).group(wallpaperPlugin).group(QStringLiteral("General"));

    if (wallpaperConfig.hasKey("Image")) {
        // Trying for the wallpaper
        auto wallpaper = wallpaperConfig.readEntry("Image", QString());

        if (!wallpaper.isEmpty()) {
            return wallpaper;
        }
    }

    if (wallpaperConfig.hasKey("Color")) {
        auto backgroundColor = wallpaperConfig.readEntry("Color", QColor(0, 0, 0));
        return backgroundColor.name();
    }

    return QString();
}

bool BackgroundCache::isDesktopContainment(const KConfigGroup &containment) const
{
    const auto type = containment.readEntry("plugin", QString());

    if (type == QLatin1String("org.kde.desktopcontainment") || type == QLatin1String("org.kde.plasma.folder") ) {
        return true;
    }

    return false;
}

void BackgroundCache::reload()
{
    // Traversing through all containments in search for
    // containments that define activities in plasma
    KConfigGroup plasmaConfigContainments = m_plasmaConfig->group(QStringLiteral("Containments"));

    //!activityId and screen names for which their background was updated
    QHash<QString, QList<QString>> updates;

    for (const auto &containmentId : plasmaConfigContainments.groupList()) {
        const auto containment = plasmaConfigContainments.group(containmentId);
        const auto wallpaperPlugin = containment.readEntry("wallpaperplugin", QString());
        const auto lastScreen  = containment.readEntry("lastScreen", 0);
        const auto activity    = containment.readEntry("activityId", QString());

        //! Ignore the containment if the activity is not defined or
        //! the containment is not a plasma desktop
        if (activity.isEmpty() || !isDesktopContainment(containment)) continue;

        const auto returnedBackground = backgroundFromConfig(containment, wallpaperPlugin);

        QString background = returnedBackground;

        if (background.startsWith(QStringLiteral("file://"))) {
            background = returnedBackground.mid(7);
        }

        QString screenName = m_pool->connector(lastScreen);

        //! Take case of broadcasted backgrounds, when their plugin is changed they should be disabled
        if (pluginExistsFor(activity,screenName)
                && m_plugins[activity][screenName] != wallpaperPlugin
                && backgroundIsBroadcasted(activity, screenName)){
            //! in such case the Desktop changed wallpaper plugin and the broadcasted wallpapers should be removed
            setBroadcastedBackgroundsEnabled(activity, screenName, false);
        }

        m_plugins[activity][screenName] = wallpaperPlugin;

        if (background.isEmpty() || backgroundIsBroadcasted(activity, screenName)) {
            continue;
        }

        if(!m_backgrounds.contains(activity)
                || !m_backgrounds[activity].contains(screenName)
                || m_backgrounds[activity][screenName] != background) {

            updates[activity].append(screenName);
        }

        m_backgrounds[activity][screenName] = background;
    }

    m_initialized = true;

    for (const auto &activity : updates.keys()) {
        for (const auto &screen : updates[activity]) {
            Q_EMIT backgroundChanged(activity, screen);
        }
    }
}

QString BackgroundCache::background(QString activity, QString screen) const
{
    if (m_backgrounds.contains(activity) && m_backgrounds[activity].contains(screen)) {
        return m_backgrounds[activity][screen];
    } else {
        return m_defaultWallpaperPath;
    }
}

bool BackgroundCache::busyFor(QString activity, QString screen, Plasma::Types::Location location)
{
    QString assignedBackground = background(activity, screen);

    if (!assignedBackground.isEmpty()) {
        return busyForFile(assignedBackground, location);
    }

    return false;
}

float BackgroundCache::brightnessFor(QString activity, QString screen, Plasma::Types::Location location)
{
    QString assignedBackground = background(activity, screen);

    if (!assignedBackground.isEmpty()) {
        return brightnessForFile(assignedBackground, location);
    }

    return -1000;
}

//! Loads the image and delegates the tiling/brightness maths to the pure
//! BackgroundImageInfo helper, then caches the resulting hints per edge.
void BackgroundCache::updateImageCalculations(QString imageFile, Plasma::Types::Location location)
{
    if (m_hintsCache.size() > MAXHASHSIZE) {
        cleanupHashes();
    }

    //! if it is a local image
    QImage image(imageFile);

    if (image.format() == QImage::Format_Invalid) {
        return;
    }

    const BackgroundImageInfo::EdgeHints hints = BackgroundImageInfo::edgeHints(image, location);

    if (!m_hintsCache.contains(imageFile)) {
        m_hintsCache[imageFile] = EdgesHash();
    }

    if (!m_hintsCache[imageFile].contains(location)) {
        imageHints iHints;
        iHints.brightness = hints.brightness;
        iHints.busy = hints.busy;
        m_hintsCache[imageFile].insert(location, iHints);
    } else {
        m_hintsCache[imageFile][location].brightness = hints.brightness;
        m_hintsCache[imageFile][location].busy = hints.busy;
    }
}

float BackgroundCache::brightnessForFile(QString imageFile, Plasma::Types::Location location)
{
    if (m_hintsCache.contains(imageFile)) {
        if (m_hintsCache[imageFile].contains(location)) {
            return m_hintsCache[imageFile][location].brightness;
        }
    }

    //! if it is a color
    if (imageFile.startsWith(QLatin1Char('#'))) {
        return Latte::colorBrightness(QColor(imageFile));
    }

    updateImageCalculations(imageFile, location);

    if (m_hintsCache.contains(imageFile)) {
        return m_hintsCache[imageFile][location].brightness;
    }

    return -1000;
}

bool BackgroundCache::busyForFile(QString imageFile, Plasma::Types::Location location)
{
    if (m_hintsCache.contains(imageFile)) {
        if (m_hintsCache[imageFile].contains(location)) {
            return m_hintsCache[imageFile][location].busy;
        }
    }

    //! if it is a color
    if (imageFile.startsWith(QLatin1Char('#'))) {
        return false;
    }

    updateImageCalculations(imageFile, location);

    if (m_hintsCache.contains(imageFile)) {
        return m_hintsCache[imageFile][location].busy;
    }

    return false;
}

void BackgroundCache::cleanupHashes()
{
    if (m_hintsCache.count() <= MAXHASHSIZE) {
        return;
    }

    m_hintsCache.clear();
}

void BackgroundCache::setBackgroundFromBroadcast(QString activity, QString screen, QString filename)
{
    if (QFileInfo(filename).exists()) {
        setBroadcastedBackgroundsEnabled(activity, screen, true);
        m_backgrounds[activity][screen] = filename;
        Q_EMIT backgroundChanged(activity, screen);
    }
}

void BackgroundCache::setBroadcastedBackgroundsEnabled(QString activity, QString screen, bool enabled)
{
    if (enabled && !backgroundIsBroadcasted(activity, screen)) {
        if (!m_broadcasted.contains(activity)) {
            m_broadcasted[activity] = QList<QString>();
        }

        m_broadcasted[activity].append(screen);
    } else if (!enabled && backgroundIsBroadcasted(activity, screen)) {
        m_broadcasted[activity].removeAll(screen);

        if (m_broadcasted[activity].isEmpty()) {
            m_broadcasted.remove(activity);
        }

        reload();
    }
}

bool BackgroundCache::backgroundIsBroadcasted(QString activity, QString screenName) const
{
    return m_broadcasted.contains(activity) && m_broadcasted[activity].contains(screenName);
}

bool BackgroundCache::pluginExistsFor(QString activity, QString screenName) const
{
    return m_plugins.contains(activity) && m_plugins[activity].contains(screenName);
}

}
}
