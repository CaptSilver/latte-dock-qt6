/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts 1.3

import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.kirigami 2.20 as Kirigami

PlasmaComponents.Label {
    Layout.alignment: Qt.AlignLeft
    Layout.topMargin: Kirigami.Units.smallSpacing
    Layout.bottomMargin: Kirigami.Units.smallSpacing
    color: Kirigami.Theme.textColor
    font.weight: Font.DemiBold
    font.letterSpacing: 1.05
    font.pixelSize: 1.2 * defaultFontMetrics.height

    TextMetrics {
        id: defaultFontMetrics
        text: "M"
        font: Kirigami.Theme.defaultFont
    }
}
