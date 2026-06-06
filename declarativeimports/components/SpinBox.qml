/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.0
import QtQuick.Controls 1.4 as Controls
import QtQuick.Controls.Styles.Plasma 2.0 as Styles
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.ksvg 1.0 as KSvg
import org.kde.kirigami 2.20 as Kirigami

Controls.SpinBox {
    id: spinBox
    implicitWidth: spinBoxMetrics.advanceWidth * 10

    TextMetrics {
        id: spinBoxMetrics
        text: "M"
        font: Kirigami.Theme.defaultFont
    }

    style: Styles.SpinBoxStyle {
        KSvg.Svg {
            id: arrowSvg
            imagePath: "widgets/arrows"
            colorSet: KSvg.Svg.Button
        }
        incrementControl: KSvg.SvgItem {
            implicitWidth: spinBoxMetrics.advanceWidth * 1.8
            anchors {
                centerIn: parent
                margins: 1
                leftMargin: 0
                rightMargin: 3
            }
            svg: arrowSvg
            elementId: "up-arrow"
            opacity: control.enabled ? (styleData.upPressed ? 1 : 0.6) : 0.5
        }
        decrementControl: KSvg.SvgItem {
            implicitWidth: spinBoxMetrics.advanceWidth * 1.8
            anchors {
                centerIn: parent
                margins: 1
                leftMargin: 0
                rightMargin: 3
            }
            svg: arrowSvg
            elementId: "down-arrow"
            opacity: control.enabled ? (styleData.upPressed ? 1 : 0.6) : 0.5
        }
    }
}
