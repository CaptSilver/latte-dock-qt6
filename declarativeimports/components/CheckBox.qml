/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick.Controls 2.15 as QQC2
import org.kde.plasma.components 3.0 as PlasmaComponents

PlasmaComponents.CheckBox {
    id: checkBox

    property int value: 0
    property string tooltip: ""

    //! Compatibility aliases for the public API that used to live on the old
    //! QtQuick Controls CheckBox. QQC2/PlasmaComponents 3 renamed these
    //! to "tristate" and "checkState"; expose the old names so consumers keep working.
    property alias partiallyCheckedEnabled: checkBox.tristate
    property alias checkedState: checkBox.checkState

    QQC2.ToolTip.text: tooltip
    QQC2.ToolTip.visible: hovered && tooltip.length > 0

    onValueChanged: {
        if (partiallyCheckedEnabled) {
            checkedState = value;
        } else {
            checked = value;
        }
    }
}
