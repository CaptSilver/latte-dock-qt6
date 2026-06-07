/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.kde.plasma.components 3.0 as PlasmaComponents
import org.kde.kirigami 2.20 as Kirigami

PlasmaComponents.SpinBox {
    id: spinBox
    implicitWidth: spinBoxMetrics.advanceWidth * 10

    TextMetrics {
        id: spinBoxMetrics
        text: "M"
        font: Kirigami.Theme.defaultFont
    }
}
