/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.7
import org.kde.plasma.plasmoid 2.0

import org.kde.latte.core 0.2 as LatteCore

import "./privates" as Ability

Ability.AnimationsPrivate {
    //! Public Properties
    active: Plasmoid.configuration.animationsEnabled && LatteCore.WindowSystem.compositingActive

    duration.large: LatteCore.Environment.longDuration
    duration.proposed: speedFactor.current * 2.8 * duration.large
    duration.small: LatteCore.Environment.shortDuration

    speedFactor.normal: 1.0
    speedFactor.current: {
        if (!active || Plasmoid.configuration.durationTime === 0) {
            return 0;
        }

        if (Plasmoid.configuration.durationTime === 1 ) {
            return 0.75;
        } else if (Plasmoid.configuration.durationTime === 2) {
            return speedFactor.normal;
        } else if (Plasmoid.configuration.durationTime === 3) {
            return 1.15;
        }

        return speedFactor.normal;
    }

    //! do not update during dragging/moving applets inConfigureAppletsMode
    updateIsBlocked: (root.dragOverlay && root.dragOverlay.pressed)
                     || layouter.appletsInParentChange
}
