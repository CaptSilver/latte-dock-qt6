/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.3

import org.kde.plasma.plasmoid 2.0
import org.kde.kirigami 2.20 as Kirigami

Label {
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
