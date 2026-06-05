/*
    SPDX-FileCopyrightText: 2016 Smith AR <audoban@openmailbox.org>
    SPDX-FileCopyrightText: 2016 Michail Vourlakos <mvourlakos@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lattepackage.h"

// Qt
#include <QDebug>
#include <QLatin1String>

// KDE
#include <KPackage/PackageLoader>

namespace Latte {

Package::Package(QObject *parent, const QVariantList &args)
    : KPackage::PackageStructure(parent, args)
{
}

Package::~Package()
{
}

void Package::initPackage(KPackage::Package *package)
{
    auto fallback = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/Shell"), QStringLiteral("org.kde.plasma.desktop"));
    package->setDefaultPackageRoot(QStringLiteral("plasma/shells/"));
    package->setPath(QStringLiteral("org.kde.latte.shell"));
    package->addFileDefinition(QByteArrayLiteral("defaults"), QStringLiteral("defaults"));
    package->addFileDefinition(QByteArrayLiteral("lattedockui"), QStringLiteral("views/Panel.qml"));
    package->addFileDefinition(QByteArrayLiteral("widgetexplorerui"), QStringLiteral("views/WidgetExplorer.qml"));
    //Configuration
    package->addFileDefinition(QByteArrayLiteral("lattedockconfigurationui"), QStringLiteral("configuration/LatteDockConfiguration.qml"));
    package->addFileDefinition(QByteArrayLiteral("lattedocksecondaryconfigurationui"), QStringLiteral("configuration/LatteDockSecondaryConfiguration.qml"));
    package->addFileDefinition(QByteArrayLiteral("canvasconfigurationui"), QStringLiteral("configuration/CanvasConfiguration.qml"));
    package->addFileDefinition(QByteArrayLiteral("configmodel"), QStringLiteral("configuration/config.qml"));
    package->addFileDefinition(QByteArrayLiteral("splitter"), QStringLiteral("images/splitter.svgz"));
    package->addFileDefinition(QByteArrayLiteral("trademark"), QStringLiteral("images/trademark.svgz"));
    package->addFileDefinition(QByteArrayLiteral("trademarkicon"), QStringLiteral("images/trademarkicon.svgz"));
    package->addFileDefinition(QByteArrayLiteral("infoviewui"), QStringLiteral("views/InfoView.qml"));

    package->addFileDefinition(QByteArrayLiteral("layout1"), QStringLiteral("layouts/Default.latterc"));
    package->addFileDefinition(QByteArrayLiteral("layout2"), QStringLiteral("layouts/Plasma.latterc"));
    package->addFileDefinition(QByteArrayLiteral("layout3"), QStringLiteral("layouts/Unity.latterc"));
    package->addFileDefinition(QByteArrayLiteral("layout4"), QStringLiteral("layouts/Extended.latterc"));

    package->addFileDefinition(QByteArrayLiteral("templates"), QStringLiteral("templates"));

    package->addFileDefinition(QByteArrayLiteral("preset1"), QStringLiteral("presets/Default.layout.latte"));
    package->addFileDefinition(QByteArrayLiteral("preset2"), QStringLiteral("presets/Plasma.layout.latte"));
    package->addFileDefinition(QByteArrayLiteral("preset3"), QStringLiteral("presets/Unity.layout.latte"));
    package->addFileDefinition(QByteArrayLiteral("preset4"), QStringLiteral("presets/Extended.layout.latte"));
    package->addFileDefinition(QByteArrayLiteral("preset10"), QStringLiteral("presets/multiple-layouts_hidden.layout.latte"));

    //! applets
    package->addFileDefinition(QByteArrayLiteral("compactapplet"), QStringLiteral("applet/CompactApplet.qml"));

    package->setFallbackPackage(fallback);
    qDebug() << "package is valid" << package->isValid();
}

void Package::pathChanged(KPackage::Package *package)
{
    if (!package->metadata().isValid())
        return;

    const QString pluginName = package->metadata().pluginId();

    if (!pluginName.isEmpty() && pluginName != QLatin1String("org.kde.latte.shell")) {
        auto fallback = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/Shell"), QStringLiteral("org.kde.latte.shell"));
        package->setFallbackPackage(fallback);
    } else if (pluginName.isEmpty() || pluginName == QLatin1String("org.kde.latte.shell")) {
        package->setFallbackPackage(KPackage::Package());
    }
}

}
