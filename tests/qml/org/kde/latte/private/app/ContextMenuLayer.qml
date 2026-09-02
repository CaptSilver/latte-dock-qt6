/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Stand-in for Latte::ContextMenuLayerQuickItem, which qmlRegisterType puts into
// org.kde.latte.private.app from inside the latte-dock binary (lattecorona.cpp).
// That module therefore does not exist under qmltestrunner at all -- it is why
// qml_load_compile.sh skips every file importing it, and why containment QML
// that reaches for it has never been testable headlessly.
//
// This shadows nothing: no installed or staged tree ships org.kde.latte.private.app
// (only private/containment and private/tasks). If the app module ever does get
// installed as a real QML plugin, tests/qml is the LAST -import the runner passes
// and so wins priority -- this stub would start hiding the real type, and should
// be deleted at that point.
import QtQuick

Item {
    property var view: null
    property bool menuIsShown: false
}
