/*
    SPDX-FileCopyrightText: 2016 Smith AR <audoban@openmailbox.org>
    SPDX-FileCopyrightText: 2016-2018 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

function insertIndexAt(above, x, y) {
    if (above && above.itemIndex) {
        return above.itemIndex;
    } else {
        var distance = root.vertical ? y : x;
        var step = appletAbilities.metrics.totals.length;
        var stripe = Math.ceil(distance / step);

        return stripe-1;
    }
}
