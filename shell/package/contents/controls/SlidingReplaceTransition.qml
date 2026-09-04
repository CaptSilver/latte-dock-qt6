/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.7

//! Page swap for the settings StackViews: the new page slides in from one edge while the
//! old one leaves through the other, both fading.
//!
//! slideWidth is the caller's to give. The dock settings dialog travels the width of the
//! background behind its StackView rather than the StackView's own, which follows whatever
//! page it currently holds and would leave the pages sliding by the wrong distance.
Transition {
    id: slide

    property bool entering: true
    property bool forward: true
    property real slideWidth: 0

    readonly property int slideDuration: 350

    ParallelAnimation {
        PropertyAnimation {
            property: "x"
            from: slide.entering ? (slide.forward ? -slide.slideWidth : slide.slideWidth) : 0
            to: slide.entering ? 0 : (slide.forward ? slide.slideWidth : -slide.slideWidth)
            duration: slide.slideDuration
        }

        PropertyAnimation {
            property: "opacity"
            from: slide.entering ? 0 : 1
            to: slide.entering ? 1 : 0
            duration: slide.slideDuration
        }
    }
}
