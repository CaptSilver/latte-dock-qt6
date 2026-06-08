/*
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "alternativeshelper.h"

// Qt
#include <QDebug>
#include <QQmlEngine>
#include <QQmlContext>
#include <QRectF>

// KDE
#include <KPackage/Package>

// Plasma
#include <Plasma/Containment>
#include <Plasma/PluginLoader>
#include <PlasmaQuick/AppletQuickItem>

AlternativesHelper::AlternativesHelper(Plasma::Applet *applet, QObject *parent)
    : QObject(parent),
      m_applet(applet)
{
}

AlternativesHelper::~AlternativesHelper()
{
}

QStringList AlternativesHelper::appletProvides() const
{
    return m_applet->pluginMetaData().value(QStringLiteral("X-Plasma-Provides"), QStringList());
}

QString AlternativesHelper::currentPlugin() const
{
    return m_applet->pluginMetaData().pluginId();
}

QQuickItem *AlternativesHelper::applet() const
{
    return PlasmaQuick::AppletQuickItem::itemForApplet(m_applet);
}

void AlternativesHelper::loadAlternative(const QString &plugin)
{
    if (plugin == currentPlugin() || m_applet->isContainment()) {
        return;
    }

    Plasma::Containment *cont = m_applet->containment();

    if (!cont) {
        return;
    }

    QQuickItem *appletItem = PlasmaQuick::AppletQuickItem::itemForApplet(m_applet);
    QQuickItem *contItem = PlasmaQuick::AppletQuickItem::itemForApplet(cont);

    if (!appletItem || !contItem) {
        return;
    }

    // ensure the global shortcut is moved to the new applet
    const QKeySequence &shortcut = m_applet->globalShortcut();
    m_applet->setGlobalShortcut(QKeySequence()); // need to unmap the old one first

    const QPointF newPos = appletItem->mapToItem(contItem, QPointF(0, 0));

    m_applet->destroy();

    connect(m_applet, &QObject::destroyed, [ = ]() {
        Plasma::Applet *newApplet = nullptr;
        // Plasma 6's createApplet takes a QRectF geometry hint, not a QPoint. A
        // mismatched arg type makes invokeMethod fail to resolve the method, so
        // the swap would silently no-op; pass a positioned rect and report any
        // failure instead of dropping it.
        const bool invoked = QMetaObject::invokeMethod(contItem, "createApplet",
                                                       Q_RETURN_ARG(Plasma::Applet *, newApplet),
                                                       Q_ARG(QString, plugin),
                                                       Q_ARG(QVariantList, QVariantList()),
                                                       Q_ARG(QRectF, QRectF(newPos, QSizeF(0, 0))));

        if (!invoked || !newApplet) {
            qWarning() << "Latte: could not swap applet for alternative" << plugin;
            return;
        }

        newApplet->setGlobalShortcut(shortcut);
    });
}

#include "moc_alternativeshelper.cpp"

