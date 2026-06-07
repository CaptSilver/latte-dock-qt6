/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The shell view wrapper (shell/package/contents/views/Panel.qml) is a KSvg.FrameSvgItem sized
// to the whole view. The view is deliberately oversized to the maximum zoom thickness so applets
// have room to grow on hover; only a small strip at the screen edge is real dock. The wrapper used
// to pick its imagePath with
//
//     containment.backgroundHints === PlasmaCore.Types.NoBackground ? "" : "widgets/panel-background"
//
// On Plasma 5 the containment graphic object exposed backgroundHints, Latte set it to NoBackground,
// and the wrapper drew nothing (the containment paints its own background). On Plasma 6 the
// containment graphic object no longer carries a backgroundHints property, so the comparison reads
// undefined, the ternary falls through, and the wrapper paints widgets/panel-background across the
// whole oversized view. X11 hid the overflow behind the visual shape-mask; Wayland has no such mask,
// so it surfaced as a dark band over the zoom-reserve area.
//
// These tests pin both halves: the legacy check really does fall through when backgroundHints is
// absent (the regression), and the shipped wrapper no longer depends on it.

#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QFile>
#include <QObject>
#include <QRegularExpression>

class PanelBackgroundTest : public QObject
{
    Q_OBJECT

private:
    //! Evaluates the legacy imagePath ternary against a containment built from the given QML
    //! fragment, returning whatever imagePath the wrapper would have used.
    QString legacyImagePath(const QString &containmentFragment);

private Q_SLOTS:
    void legacyCheckPaintsBandWhenContainmentLacksBackgroundHints();
    void legacyCheckSuppressesBandWhenBackgroundHintsIsNoBackground();
    void shippedWrapperNeverPaintsPanelBackground();
};

QString PanelBackgroundTest::legacyImagePath(const QString &containmentFragment)
{
    QQmlEngine engine;
    QQmlComponent component(&engine);

    //! noBackground stands in for PlasmaCore.Types.NoBackground; the exact value is irrelevant,
    //! only that an absent property can never compare equal to it.
    const QString qml = QStringLiteral(R"(
        import QtQuick 2.15
        Item {
            id: root
            property int noBackground: 7
            property QtObject containment: %1
            readonly property string imagePath:
                containment && containment.backgroundHints === root.noBackground ? "" : "widgets/panel-background"
        }
    )").arg(containmentFragment);

    component.setData(qml.toUtf8(), QUrl());
    QObject *root = component.create();
    [&]() { QVERIFY2(root, qPrintable(component.errorString())); }();
    if (!root) {
        return QStringLiteral("<create-failed>");
    }

    const QString path = QQmlProperty::read(root, QStringLiteral("imagePath")).toString();
    delete root;
    return path;
}

//! Plasma 6: the containment graphic object has no backgroundHints, so the ternary falls through
//! to the panel-background SVG -> the band. This documents the exact regression the fix guards.
void PanelBackgroundTest::legacyCheckPaintsBandWhenContainmentLacksBackgroundHints()
{
    QCOMPARE(legacyImagePath(QStringLiteral("QtObject { }")), QStringLiteral("widgets/panel-background"));
}

//! Plasma 5: when backgroundHints exists and equals NoBackground the wrapper drew nothing. Kept so
//! the test makes clear the logic was once correct and broke only because the property disappeared.
void PanelBackgroundTest::legacyCheckSuppressesBandWhenBackgroundHintsIsNoBackground()
{
    QCOMPARE(legacyImagePath(QStringLiteral("QtObject { property int backgroundHints: 7 }")), QString());
}

//! The fix: the shipped wrapper must not depend on containment.backgroundHints and must not paint a
//! panel-background of its own (the containment owns all background drawing).
void PanelBackgroundTest::shippedWrapperNeverPaintsPanelBackground()
{
    QFile file(QStringLiteral(PANEL_QML_PATH));
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.errorString()));
    const QString source = QString::fromUtf8(file.readAll());

    //! The active imagePath assignment is empty (commented-out alternatives are ignored).
    static const QRegularExpression activeImagePath(QStringLiteral("^\\s*imagePath:\\s*\"\"\\s*$"),
                                                    QRegularExpression::MultilineOption);
    QVERIFY2(activeImagePath.match(source).hasMatch(),
             "Panel.qml must set imagePath to an empty string so it never paints a band.");

    //! The broken backgroundHints check must not come back as an active (uncommented) line.
    static const QRegularExpression activeBackgroundHints(QStringLiteral("^\\s*imagePath:.*backgroundHints"),
                                                          QRegularExpression::MultilineOption);
    QVERIFY2(!activeBackgroundHints.match(source).hasMatch(),
             "Panel.qml must not gate imagePath on containment.backgroundHints (absent on Plasma 6).");
}

QTEST_MAIN(PanelBackgroundTest)
#include "panelbackgroundtest.moc"
