/*
    SPDX-FileCopyrightText: 2016 Smith AR <audoban@openmailbox.org>
    SPDX-FileCopyrightText: 2016 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.7
import Qt5Compat.GraphicalEffects
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.3

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents

import org.kde.plasma.plasmoid 2.0

import org.kde.latte.core 0.2 as LatteCore
import org.kde.latte.private.containment 0.1 as LatteContainment
import org.kde.kirigami 2.20 as Kirigami

Grid {
    id: typeRow

    width: horizontal ? content.width - 4*Kirigami.Units.smallSpacing : 9 * Kirigami.Theme.defaultFont.pixelSize
    anchors.leftMargin: horizontal ? Kirigami.Units.smallSpacing : 0
    anchors.rightMargin: horizontal ? Kirigami.Units.smallSpacing : 0

    Layout.minimumWidth: width
    Layout.maximumWidth: width
    Layout.minimumHeight: height
    Layout.maximumHeight: height

    Layout.leftMargin: Kirigami.Units.smallSpacing * 2
    Layout.rightMargin: Kirigami.Units.smallSpacing * 2

    rows: horizontal ? 1 : 0
    columns: horizontal ? 0 : 1

    spacing: 1

    verticalItemAlignment: Grid.AlignVCenter

    property bool horizontal: false

    PlasmaComponents.Button {
        id: dockTypeButton
        width: horizontal ? (parent.width - parent.spacing)/ 2 : parent.width
        enabled: LatteCore.WindowSystem.compositingActive

        checkable: false
        checked: latteView.type === LatteCore.Types.DockView
        text: i18nc("dock type","Dock")
        QQC2.ToolTip.text: i18n("Change the behavior and appearance to Dock type")
        QQC2.ToolTip.visible: hovered

        onPressedChanged: {
            if (pressed && !checked) {
                latteView.userRequestedViewType(LatteCore.Types.DockView);

                latteView.visibility.mode = LatteCore.Types.DodgeActive;
                plasmoid.configuration.alignment = LatteCore.Types.Center;
                plasmoid.configuration.useThemePanel = true;
                plasmoid.configuration.solidPanel = false;
                plasmoid.configuration.panelSize = 5;
                plasmoid.configuration.appletShadowsEnabled = true;
                plasmoid.configuration.zoomLevel = 16;
                //plasmoid.configuration.autoDecreaseIconSize = true;

                //! Empty Areas
                plasmoid.configuration.dragActiveWindowEnabled = false;
                plasmoid.configuration.scrollAction = LatteContainment.Types.ScrollNone;

                //! Items
                plasmoid.configuration.autoSizeEnabled = true;

                //! Dynamic Background
                plasmoid.configuration.solidBackgroundForMaximized = false;
                plasmoid.configuration.colorizeTransparentPanels = false;
                plasmoid.configuration.backgroundOnlyOnMaximized = false;
                plasmoid.configuration.disablePanelShadowForMaximized = false;
                plasmoid.configuration.plasmaBackgroundForPopups = false;

                //! Floating
                plasmoid.configuration.floatingInternalGapIsForced = true;
            }
        }
    }

    PlasmaComponents.Button {
        id: panelTypeButton
        width: dockTypeButton.width
        enabled: LatteCore.WindowSystem.compositingActive

        checkable: false
        checked: latteView.type === LatteCore.Types.PanelView
        text: i18nc("panel type","Panel")
        QQC2.ToolTip.text: i18n("Change the behavior and appearance to Panel type")
        QQC2.ToolTip.visible: hovered

        onPressedChanged: {
            if (pressed && !checked) {
                latteView.userRequestedViewType(LatteCore.Types.PanelView);

                latteView.visibility.mode = LatteCore.Types.AlwaysVisible;
                plasmoid.configuration.alignment = LatteCore.Types.Justify;
                plasmoid.configuration.useThemePanel = true;
                plasmoid.configuration.solidPanel = false;
                plasmoid.configuration.panelSize = 100;
                plasmoid.configuration.panelShadows = true;
                plasmoid.configuration.appletShadowsEnabled = false;
                plasmoid.configuration.zoomLevel = 0;
                plasmoid.configuration.titleTooltips = false;
                //plasmoid.configuration.autoDecreaseIconSize = false;

                //! Empty Areas
                plasmoid.configuration.dragActiveWindowEnabled = true;

                //! Items
                plasmoid.configuration.autoSizeEnabled = false;

                //! Dynamic Background
                plasmoid.configuration.colorizeTransparentPanels = false;
                plasmoid.configuration.backgroundOnlyOnMaximized = false;
                plasmoid.configuration.disablePanelShadowForMaximized = false;
                plasmoid.configuration.plasmaBackgroundForPopups = true;

                //! Floating
                plasmoid.configuration.floatingInternalGapIsForced = false;

                //! Custom Background that overrides Plasma Theme metrics
                plasmoid.configuration.backgroundRadius = -1;
                plasmoid.configuration.backgroundShadowSize = -1;
            }
        }
    }
}

