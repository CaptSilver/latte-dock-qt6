/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.7
import QtQuick.Layouts 1.3

import org.kde.plasma.components 3.0 as PlasmaComponents

import "." as LatteComponents
import org.kde.kirigami 2.20 as Kirigami

//! A captioned slider that reads out its value as a percentage and commits only
//! when the handle is let go, so a drag does not write config on every frame.
RowLayout {
    id: row
    Layout.fillWidth: true
    spacing: Kirigami.Units.smallSpacing

    //! Caption text, translated by the consumer. The indicator packages extract
    //! their labels into their own catalogs, so these msgids cannot live here.
    property string label: ""

    property alias value: slider.value
    property alias from: slider.from
    property alias to: slider.to
    property alias stepSize: slider.stepSize

    //! Hands back the raw slider value rather than writing anything: the rounding
    //! on the way into config differs from row to row.
    signal released(real percent);

    TextMetrics {
        id: defaultFontMetrics
        text: "M"
        font: Kirigami.Theme.defaultFont
    }

    PlasmaComponents.Label {
        text: row.label
        horizontalAlignment: Text.AlignLeft
    }

    LatteComponents.Slider {
        id: slider
        Layout.fillWidth: true

        from: 0
        to: 100
        stepSize: 1
        wheelEnabled: false

        onPressedChanged: {
            if (!pressed) {
                row.released(value);
            }
        }
    }

    PlasmaComponents.Label {
        //! Name the domain: the indicator settings view loads its config UI in an
        //! engine set to latte_indicator_<pluginId>, but this string is extracted
        //! into latte-dock.pot, so a bare i18nc would miss it in every language.
        text: i18ndc("latte-dock", "number in percentage, e.g. 85 %", "%1 %", currentValue)
        horizontalAlignment: Text.AlignRight
        Layout.minimumWidth: defaultFontMetrics.advanceWidth * 4
        Layout.maximumWidth: defaultFontMetrics.advanceWidth * 4

        //! Kept integral on purpose: the slider's value is a real, and handing the
        //! raw one over reads out as "42.5 %".
        readonly property int currentValue: slider.value
    }
}
