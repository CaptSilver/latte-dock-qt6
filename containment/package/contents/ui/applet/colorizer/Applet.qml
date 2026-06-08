/*
    SPDX-FileCopyrightText: 2018 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.7
import QtQuick.Effects

import org.kde.latte.components 1.0 as LatteComponents

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
    MultiEffect {
        id: colorizer
        anchors.fill: parent
        source: wrapper
        colorizationColor: colorizerManager.applyColor
        colorization: colorizerManager.applyColor.a
    }

    ///Shadow in applets
    Loader{
        id: colorizedAppletShadow
        anchors.fill: colorizer

        active: appletItem.environment.isGraphicsSystemAccelerated
                && Plasmoid.configuration.appletShadowsEnabled
                && (appletColorizer.opacity>0)

        sourceComponent: LatteComponents.ShadowedItem{
            anchors.fill: parent
            shadowColor: appletItem.myView.itemShadow.shadowColor
            source: colorizer
            shadowSizePx: shadowSize
            shadowVerticalOffset: forcedShadow ? 0 : 2

            readonly property int shadowSize : appletItem.myView.itemShadow.size

            readonly property bool forcedShadow: root.forceTransparentPanel
                                                 && Plasmoid.configuration.appletShadowsEnabled
                                                 && !appletItem.communicator.indexerIsSupported ? true : false
        }
    }
}
