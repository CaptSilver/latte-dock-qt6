/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.1

import org.kde.plasma.plasmoid 2.0

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents

Item{
    Component.onCompleted:  {
        v010_upgrade();
    }

    function v010_upgrade() {
        root.upgrader_v010_alignment();

        if (!Plasmoid.configuration.shadowsUpgraded) {
            if (Plasmoid.configuration.shadows > 0) {
                Plasmoid.configuration.appletShadowsEnabled = true;
            } else {
                Plasmoid.configuration.appletShadowsEnabled = false;
            }

            Plasmoid.configuration.shadowsUpgraded = true;
        }

        if (!Plasmoid.configuration.tasksUpgraded) {
            v010_tasksMigrateTimer.start();
        }

    }

    Item {
        id: v010_tasksUpgrader
        Repeater {
            id: v010_tasksRepeater
            model: latteView && !Plasmoid.configuration.tasksUpgraded ? latteView.extendedInterface.latteTasksModel : null
            Item {
                id: tasksApplet
                Component.onCompleted: {
                    if (index === 0) {
                        console.log(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! !!!!!!!!!!!!!!!!");
                        console.log(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! !!!!!!!!!!!!!!!!");
                        console.log(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! !!!!!!!!!!!!!!!!");
                        console.log(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! !!!!!!!!!!!!!!!!");
                        console.log(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! !!!!!!!!!!!!!!!!");
                        console.log(" Migrating Tasks Applet for v0.10...");
                        tasks.configuration.launchersGroup = Plasmoid.configuration.launchersGroup;
                        tasks.configuration.showWindowActions = Plasmoid.configuration.showWindowActions;
                        tasks.configuration.showWindowsOnlyFromLaunchers = Plasmoid.configuration.showWindowsOnlyFromLaunchers;
                        tasks.configuration.groupTasksByDefault = Plasmoid.configuration.groupTasksByDefault;
                        tasks.configuration.showOnlyCurrentScreen = Plasmoid.configuration.showOnlyCurrentScreen;
                        tasks.configuration.showOnlyCurrentDesktop = Plasmoid.configuration.showOnlyCurrentDesktop;
                        tasks.configuration.showOnlyCurrentActivity = Plasmoid.configuration.showOnlyCurrentActivity;
                        tasks.configuration.showInfoBadge = Plasmoid.configuration.showInfoBadge;
                        tasks.configuration.showProgressBadge = Plasmoid.configuration.showProgressBadge;
                        tasks.configuration.showAudioBadge = Plasmoid.configuration.showAudioBadge;
                        tasks.configuration.audioBadgeActionsEnabled = Plasmoid.configuration.audioBadgeActionsEnabled;
                        tasks.configuration.infoBadgeProminentColorEnabled = Plasmoid.configuration.infoBadgeProminentColorEnabled;
                        tasks.configuration.animationLauncherBouncing = Plasmoid.configuration.animationLauncherBouncing;
                        tasks.configuration.animationWindowInAttention = Plasmoid.configuration.animationWindowInAttention;
                        tasks.configuration.animationNewWindowSliding = Plasmoid.configuration.animationNewWindowSliding;
                        tasks.configuration.animationWindowAddedInGroup = Plasmoid.configuration.animationWindowAddedInGroup;
                        tasks.configuration.animationWindowRemovedFromGroup = Plasmoid.configuration.animationWindowRemovedFromGroup;
                        tasks.configuration.scrollTasksEnabled = Plasmoid.configuration.scrollTasksEnabled;
                        tasks.configuration.autoScrollTasksEnabled = Plasmoid.configuration.autoScrollTasksEnabled;
                        tasks.configuration.manualScrollTasksType = Plasmoid.configuration.manualScrollTasksType;
                        tasks.configuration.leftClickAction = Plasmoid.configuration.leftClickAction;
                        tasks.configuration.middleClickAction = Plasmoid.configuration.middleClickAction;
                        tasks.configuration.hoverAction = Plasmoid.configuration.hoverAction;
                        tasks.configuration.taskScrollAction = Plasmoid.configuration.taskScrollAction;
                        tasks.configuration.modifierClickAction = Plasmoid.configuration.modifierClickAction;
                        tasks.configuration.modifier = Plasmoid.configuration.modifier;
                        tasks.configuration.modifierClick = Plasmoid.configuration.modifierClick;
                        tasks.configuration.isDroppedLauncherAddedOnlyInCurrentTasks = Plasmoid.configuration.addLaunchersInTaskManager;
                        console.log("Migrating Tasks Applet for v0.10 succeeded ...");

                        Plasmoid.configuration.tasksUpgraded = true;
                    }
                }
            }
        }
    }


    //! v0.10 Timer to check that first-upgrade process ended
    //! when View does not have any Tasks plasmoid
    Timer {
        id: v010_tasksMigrateTimer
        interval: 10000
        onTriggered: {
            Plasmoid.configuration.tasksUpgraded = true;
            console.log("Tasks Migration ended....");
        }
    }
}
