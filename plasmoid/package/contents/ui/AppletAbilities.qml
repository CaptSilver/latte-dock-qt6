/*
    SPDX-FileCopyrightText: 2021 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.7

import org.kde.plasma.plasmoid 2.0

import org.kde.latte.core 0.2 as LatteCore
import org.kde.latte.abilities.client 0.1 as AbilityClient

import "abilities" as Ability

AbilityClient.AppletAbilities {
    id: _abilityContainer
    property QtObject tasksModel: null

    readonly property alias launchers: _launchers

    Ability.Launchers {
        id: _launchers
        bridge: _abilityContainer.bridge
        layout: _abilityContainer.layout
        tasksModel: _abilityContainer.tasksModel
        view: _abilityContainer.myView
    }

    //! local properties
    readonly property int maxIconSizeInPlasma: Math.max(Plasmoid.configuration.iconSize, 16)
    readonly property int panelThickness: (root.vertical ? root.width : root.height)

    //! Connections
    Connections {
        target: _abilityContainer.shortcuts
        onDisabledIsStealingGlobalPositionShortcuts: {
            Plasmoid.configuration.isPreferredForPositionShortcuts = false;
        }
    }

    Connections {
        target: _abilityContainer.launchers
        onDisabledIsStealingDroppedLaunchers: {
            Plasmoid.configuration.isPreferredForDroppedLaunchers = false;
        }
    }

    Connections {
        target: _abilityContainer.userRequests
        function onSglViewType(viewType) {
            if (viewType === LatteCore.Types.DockView) {
                Plasmoid.configuration.animationLauncherBouncing = true;
                Plasmoid.configuration.animationWindowInAttention = true;
                Plasmoid.configuration.animationWindowAddedInGroup = true;
            } else if (viewType === LatteCore.Types.PanelView) {
                Plasmoid.configuration.animationLauncherBouncing = false;
                Plasmoid.configuration.animationWindowInAttention = false;
                Plasmoid.configuration.animationWindowAddedInGroup = false;
            }
        }
    }
}
