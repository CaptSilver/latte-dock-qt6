/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick.Controls 2.15 as QQC2
import org.kde.plasma.components 3.0 as PlasmaComponents

PlasmaComponents.CheckBox {
    id: checkBox

    //! The config key this checkbox reads and writes, named once. Sites whose state is
    //! computed from more than one source leave these unset, assign `value` themselves and
    //! keep their own onClicked.
    property var bindTarget: null
    property string bindProperty: ""

    readonly property bool isBound: !!bindTarget && bindProperty !== ""

    property int value: isBound && bindTarget[bindProperty] ? 1 : 0
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

    //! Gated on isBound because a handler declared here does not replace one declared at a
    //! use site -- both run. An unbound site keeps its own onClicked, and this one has to
    //! stay out of its way or the two writes cancel and the setting silently stops saving.
    onClicked: {
        if (isBound) {
            bindTarget[bindProperty] = !bindTarget[bindProperty];
        }
    }
}
