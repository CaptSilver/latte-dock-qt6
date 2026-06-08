/*
    SPDX-FileCopyrightText: 2016 Smith AR <audoban@openmailbox.org>
    SPDX-FileCopyrightText: 2016 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.0
import QtQuick.Layouts 1.0
import Qt5Compat.GraphicalEffects

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.kirigami 2.20 as Kirigami
import org.kde.latte.components 1.0 as LatteComponents

Item {
    id: mainItem

    width: childrenRect.width
    height: childrenRect.height

    property bool vertical: (plasmoid.formFactor == PlasmaCore.Types.Vertical)

    property alias cfg_showBarLine: showBarLine.checked
    property alias cfg_useThemePanel: useThemePanel.checked
    property alias cfg_panelSize: panelSize.value
    property alias cfg_transparentPanel: transparentPanel.checked
    property alias cfg_isInNowDockPanel: mainItem.isInNowDockPanel

    property bool isInNowDockPanel

    ColumnLayout {

        id:mainColumn
        spacing: 15
        Layout.fillWidth: true

        GridLayout{
            enabled: !mainItem.isInNowDockPanel
            Layout.fillWidth: true
            columns: 3
            property bool panelConfigEnabled: showBarLine.checked && useThemePanel.checked

            PlasmaComponents.Label{}

            PlasmaComponents.CheckBox {
                id: showBarLine
                Layout.columnSpan: 3
                text: i18n("Show bar line for tasks")
                enabled: true
            }

            PlasmaComponents.CheckBox {
                id: useThemePanel
                Layout.columnSpan: 3
                text: i18n("Use plasma theme panel")
                enabled: showBarLine.checked
            }

            PlasmaComponents.CheckBox {
                id: transparentPanel
                Layout.columnSpan: 3
                text: i18n("Use transparency in the panel")
                enabled: parent.panelConfigEnabled
            }


            PlasmaComponents.Label {
                id: panelLabel
                text: i18n("Size: ")
                enabled: parent.panelConfigEnabled
            }

            PlasmaComponents.Slider {
                id: panelSize
                enabled: parent.panelConfigEnabled
                Layout.fillWidth: true
                from: 0
                to: 256
                stepSize: 2
            }

            PlasmaComponents.Label {
                enabled: parent.panelConfigEnabled
                Layout.minimumWidth: metricsLabel.width
                Layout.maximumWidth: metricsLabel.width
                Layout.alignment: Qt.AlignRight
                horizontalAlignment: Text.AlignRight

                text: ( panelSize.value + " px." )

                PlasmaComponents.Label{
                    id:metricsLabel
                    visible: false
                    text: panelSize.to+" px."
                }
            }

            /*    PlasmaComponents.Label{
                Layout.columnSpan: 3
                Layout.fillWidth: false
                Layout.alignment: Qt.AlignRight
                Layout.maximumWidth: zoomLevel.width + zoomLevelText.width + panelLabel.width
                horizontalAlignment: Text.AlignRight
                text: i18n("in panel placement, themes that have set a <b>specific</b> panel transparent work better")
                wrapMode: Text.WordWrap
                font.italic: true
                enabled: parent.panelConfigEnabled
            }*/

            /////
            //spacer to set a minimumWidth for sliders
            //Layout.minimumWidth didn't work
            PlasmaComponents.Label{}
            PlasmaComponents.Label{Layout.minimumWidth: 280}
            PlasmaComponents.Label{}

        }
    }

    LatteComponents.ShadowedItem {
        id:shadowText
        anchors.fill: inNowDockLabel
        enabled: isInNowDockPanel
        shadowSizePx: 3
        shadowColor: "#cc080808"
        source: inNowDockLabel

        shadowVerticalOffset: 2
        shadowHorizontalOffset: -1
        visible: isInNowDockPanel
    }


    PlasmaComponents.Label {
        id:inNowDockLabel
        anchors.horizontalCenter: mainItem.horizontalCenter
        anchors.verticalCenter: mainColumn.verticalCenter
      //  anchors.verticalCenterOffset:  (mainColumn.height / 4)

        width: 0.85 * mainItem.width
        text: i18n("For the disabled settings you should use the Latte Dock Configuration Window")
        visible: mainItem.isInNowDockPanel

        horizontalAlignment: Text.AlignHCenter
        //  font.bold: true
        font.italic: true
        font.pointSize: 1.2 * Kirigami.Theme.defaultFont.pointSize

        wrapMode: Text.WordWrap
    }

}
