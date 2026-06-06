/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import org.kde.plasma.components 3.0 as PlasmaComponents

PlasmaComponents.CheckBox {
    id: checkBox

    property int value: 0

    //! Compatibility aliases for the public API that used to live on the old
    //! QtQuick Controls CheckBox. QQC2/PlasmaComponents 3 renamed these
    //! to "tristate" and "checkState"; expose the old names so consumers keep working.
    property alias partiallyCheckedEnabled: checkBox.tristate
    property alias checkedState: checkBox.checkState

    onValueChanged: {
        if (partiallyCheckedEnabled) {
            checkedState = value;
        } else {
            checked = value;
        }
    }
}
