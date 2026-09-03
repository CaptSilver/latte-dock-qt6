/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Latte used to carry hand-copied Plasma private headers under app/plasma/quick/.
// The copy of configview.h used the same CONFIGVIEW_H include guard as the
// installed header, so whichever was seen first guarded the other out -- and the
// copy declared ConfigView as a QQuickView while the linked library builds it as
// a QQuickWindow. Every Latte translation unit reaches lattecorona.h, so the copy
// won everywhere and the whole program disagreed with libPlasmaQuick about the
// base class of a type it constructs.
//
// This guards against re-vendoring. The compiler decides which declaration won,
// which is why this is a type trait and not a grep over include lines.

// Include order is load-bearing: lattecorona.h is the header that decided which
// declaration the program got. Flipping these two would pass for the wrong reason.
#include "../app/lattecorona.h"
#include <PlasmaQuick/ConfigView>

#include <QQuickView>
#include <QQuickWindow>
#include <QtTest>

#include <type_traits>

class PlasmaQuickHeadersTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void configViewMatchesTheLinkedLibrary();
};

void PlasmaQuickHeadersTest::configViewMatchesTheLinkedLibrary()
{
    // QQuickView derives QQuickWindow, so asking for the QQuickWindow base would
    // hold either way. Only the QQuickView base tells the two declarations apart.
    QVERIFY2((!std::is_base_of_v<QQuickView, PlasmaQuick::ConfigView>),
             "PlasmaQuick::ConfigView was compiled against a QQuickView-based declaration; "
             "the linked library derives it from QQuickWindow");
}

QTEST_MAIN(PlasmaQuickHeadersTest)

#include "plasmaquickheaderstest.moc"
