/*
    SPDX-FileCopyrightText: 2018 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.7
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.3

import org.kde.plasma.components 3.0 as PlasmaComponents

import org.kde.latte.components 1.0 as LatteComponents
import org.kde.kirigami 2.20 as Kirigami

ColumnLayout {
    id: root
    Layout.fillWidth: true

    //! QQC2's CheckBox declares an `indicator` property of its own -- the tick delegate --
    //! so an unqualified `indicator` inside a CheckBox block resolves to that item instead
    //! of the settings view's indicator object, and the configuration read comes back
    //! undefined. Reach the map through the page root, where the name is not shadowed.
    readonly property QtObject indicatorConfig: indicator.configuration

    TextMetrics {
        id: defaultFontMetrics
        text: "M"
        font: Kirigami.Theme.defaultFont
    }

    LatteComponents.SubHeader {
        text: i18nc("indicator style","Style")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 2

        property int indicatorType: indicator.configuration.activeStyle

        readonly property int buttonsCount: 2
        readonly property int buttonSize: (dialog.optionsWidth - (spacing * (buttonsCount - 1))) / buttonsCount

        PlasmaComponents.Button {
            Layout.minimumWidth: parent.buttonSize
            Layout.maximumWidth: Layout.minimumWidth
            text: i18nc("line indicator","Line")
            checked: parent.indicatorType === indicatorType
            checkable: false
            QQC2.ToolTip.text: i18n("Show a line indicator for active items")
            QQC2.ToolTip.visible: hovered

            readonly property int indicatorType: 0 /*Line*/

            onClicked: {
                root.indicatorConfig.activeStyle = indicatorType;
            }
        }

        PlasmaComponents.Button {
            Layout.minimumWidth: parent.buttonSize
            Layout.maximumWidth: Layout.minimumWidth
            text: i18nc("dots indicator", "Dots")
            checked: parent.indicatorType === indicatorType
            checkable: false
            QQC2.ToolTip.text: i18n("Show a dot indicator for active items")
            QQC2.ToolTip.visible: hovered

            readonly property int indicatorType: 1 /*Dot*/

            onClicked: {
                root.indicatorConfig.activeStyle = indicatorType;
            }
        }
    }

    LatteComponents.PercentSliderRow {
        label: i18n("Thickness")
        value: Math.round(indicator.configuration.size * 100)
        from: 3
        to: 25

        onReleased: (percent) => {
            indicator.configuration.size = Number(percent / 100).toFixed(2);
        }
    }

    LatteComponents.PercentSliderRow {
        label: i18n("Position")
        value: Math.round(indicator.configuration.thickMargin * 100)
        from: 0
        to: 30

        onReleased: (percent) => {
            indicator.configuration.thickMargin = percent / 100;
        }
    }

    LatteComponents.PercentSliderRow {
        label: i18n("Padding")
        value: Math.round(indicator.configuration.lengthPadding * 100)
        from: 0
        to: 80

        onReleased: (percent) => {
            indicator.configuration.lengthPadding = percent / 100;
        }
    }

    LatteComponents.PercentSliderRow {
        label: i18n("Corner Margin")
        value: Math.round(indicator.configuration.backgroundCornerMargin * 100)
        from: 0
        to: 100

        onReleased: (percent) => {
            indicator.configuration.backgroundCornerMargin = percent / 100;
        }
    }

    LatteComponents.HeaderSwitch {
        id: glowEnabled
        Layout.fillWidth: true
        Layout.minimumHeight: implicitHeight
        Layout.bottomMargin: Kirigami.Units.smallSpacing

        checked: indicator.configuration.glowEnabled
        level: 2
        text: i18n("Glow")
        tooltip: i18n("Enable/disable indicator glow")

        onPressed: {
            indicator.configuration.glowEnabled = !indicator.configuration.glowEnabled;
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 2
        enabled: indicator.configuration.glowEnabled

        property int option: indicator.configuration.glowApplyTo

        readonly property int buttonsCount: 2
        readonly property int buttonSize: (dialog.optionsWidth - (spacing * (buttonsCount - 1))) / buttonsCount

        PlasmaComponents.Button {
            Layout.minimumWidth: parent.buttonSize
            Layout.maximumWidth: Layout.minimumWidth
            text: i18nc("glow only to active task/applet indicators","On Active")
            checked: parent.option === option
            checkable: false
            QQC2.ToolTip.text: i18n("Add glow only to active task/applet indicator")
            QQC2.ToolTip.visible: hovered

            readonly property int option: 1 /*OnActive*/

            onClicked: {
                root.indicatorConfig.glowApplyTo = option;
            }
        }

        PlasmaComponents.Button {
            Layout.minimumWidth: parent.buttonSize
            Layout.maximumWidth: Layout.minimumWidth
            text: i18nc("glow to all task/applet indicators","All")
            checked: parent.option === option
            checkable: false
            QQC2.ToolTip.text: i18n("Add glow to all task/applet indicators")
            QQC2.ToolTip.visible: hovered

            readonly property int option: 2 /*All*/

            onClicked: {
                root.indicatorConfig.glowApplyTo = option;
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 2

        enabled: indicator.configuration.glowEnabled

        PlasmaComponents.Label {
            Layout.minimumWidth: implicitWidth
            horizontalAlignment: Text.AlignLeft
            Layout.rightMargin: Kirigami.Units.smallSpacing
            text: i18n("Opacity")
        }

        LatteComponents.Slider {
            id: glowOpacitySlider
            Layout.fillWidth: true

            leftPadding: 0
            value: indicator.configuration.glowOpacity * 100
            from: 0
            to: 100
            stepSize: 5
            wheelEnabled: false

            function updateGlowOpacity() {
                if (!pressed)
                    indicator.configuration.glowOpacity = value/100;
            }

            onPressedChanged: {
                updateGlowOpacity();
            }

            Component.onCompleted: {
                valueChanged.connect(updateGlowOpacity);
            }

            Component.onDestruction: {
                valueChanged.disconnect(updateGlowOpacity);
            }
        }

        PlasmaComponents.Label {
            text: i18nc("number in percentage, e.g. 85 %","%1 %", glowOpacitySlider.value)
            horizontalAlignment: Text.AlignRight
            Layout.minimumWidth: defaultFontMetrics.advanceWidth * 4
            Layout.maximumWidth: defaultFontMetrics.advanceWidth * 4
        }
    }

    ColumnLayout {
        spacing: 0
        visible: indicator.latteTasksArePresent

        LatteComponents.SubHeader {
            enabled: indicator.configuration.glowApplyTo!==0/*None*/
            text: i18n("Tasks")
        }

        LatteComponents.CheckBoxesColumn {
            LatteComponents.CheckBox {
                Layout.maximumWidth: dialog.optionsWidth
                text: i18n("Different color for minimized windows")
                bindTarget: root.indicatorConfig
                bindProperty: "minimizedTaskColoredDifferently"
            }

            LatteComponents.CheckBox {
                Layout.maximumWidth: dialog.optionsWidth
                text: i18n("Show an extra dot for grouped windows when active")
                tooltip: i18n("Grouped windows show both a line and a dot when one of them is active and the Line Active Indicator is enabled")
                enabled: root.indicatorConfig.activeStyle === 0 /*Line*/
                bindTarget: root.indicatorConfig
                bindProperty: "extraDotOnActive"
            }
        }
    }

    LatteComponents.SubHeader {
        enabled: indicator.configuration.glowApplyTo!==0/*None*/
        text: i18n("Options")
    }

    LatteComponents.CheckBox {
        Layout.maximumWidth: dialog.optionsWidth
        text: i18n("Show indicators for applets")
        tooltip: i18n("Indicators are shown for applets")
        bindTarget: root.indicatorConfig
        bindProperty: "enabledForApplets"
    }

    LatteComponents.CheckBox {
        Layout.maximumWidth: dialog.optionsWidth
        text: i18n("Reverse indicator style")
        bindTarget: root.indicatorConfig
        bindProperty: "reversed"
    }
}
