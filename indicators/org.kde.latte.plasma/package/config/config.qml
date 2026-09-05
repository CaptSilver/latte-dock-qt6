/*
    SPDX-FileCopyrightText: 2018 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.7
import QtQuick.Layouts 1.3

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

    LatteComponents.SubHeader {
        text: i18n("Style")
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

    LatteComponents.SubHeader {
        text: i18n("Options")
    }

    LatteComponents.CheckBoxesColumn {
        Layout.topMargin: 1.5 * Kirigami.Units.smallSpacing

       /* LatteComponents.CheckBox {
            Layout.maximumWidth: dialog.optionsWidth
            text: i18n("Reverse indicator style")
            value: indicator.configuration.reversed

            onClicked: {
                indicator.configuration.reversed = !indicator.configuration.reversed;
            }
        }*/

        LatteComponents.CheckBox {
            Layout.maximumWidth: dialog.optionsWidth
            text: i18n("Growing circle animation when clicked")
            bindTarget: root.indicatorConfig
            bindProperty: "clickedAnimationEnabled"
        }

      /*  LatteComponents.CheckBox {
            Layout.maximumWidth: dialog.optionsWidth
            text: i18n("Show indicators for applets")
            tooltip: i18n("Indicators are shown for applets")
            value: indicator.configuration.enabledForApplets

            onClicked: {
                indicator.configuration.enabledForApplets = !indicator.configuration.enabledForApplets;
            }
        }*/
    }
}
