/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The settings windows are built from QtQuick Controls 2 / PlasmaComponents 3. Porting them off
// QtQuick Controls 1 left a class of property- and type-level holdovers that an import grep can't
// see and that only surfaced when the config windows actually loaded. These tests pin the Qt6
// rules behind each fix so the regressions cannot return silently:
//
//   * Qt6 marks several base members final (Control.implicitWidth/Height, AbstractButton.icon).
//     A QML type that redeclares one fails to LOAD entirely - the exact reason ItemDelegate,
//     TextField, ExternalShadow and HeaderSwitch had to stop redeclaring those and bind instead.
//   * A Controls 2 Button has no "tooltip" property; the tooltip moves to the ToolTip attached
//     property. The Latte CheckBox/TextField lost "tooltip"/"textColor" in the rebuild and had to
//     get them back.
//   * Controls 2 TabBar replaced QtQuick Controls 1's currentTab (a Tab item) with currentIndex.
//   * A Qt6 sequence-type model satisfies neither Array.isArray nor .get, so a ListModel must be
//     detected by probing for its get() method, not by Array.isArray.
//
// Most of these are QML compile errors, so they are caught at component compilation without having
// to instantiate a styled control.

#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QFile>
#include <QObject>

class ConfigControlsTest : public QObject
{
    Q_OBJECT

private:
    //! Compiles the QML fragment and returns the component error string (empty when it compiles).
    QString compileError(const QString &qml);
    //! Reads a shipped QML source file.
    QString readSource(const QString &path);

private Q_SLOTS:
    void textFieldImplicitWidthIsFinal();
    void textFieldImplicitWidthBindingIsAccepted();
    void abstractButtonIconIsFinal();
    void button2HasNoTooltipProperty();
    void button2AcceptsToolTipAttachedProperty();
    void tabBar2HasNoCurrentTab();
    void tabBar2AcceptsCurrentIndex();
    void listModelIsDistinguishedByItsGetMethod();
    void checkBoxSourceRestoresTooltip();
    void textFieldSourceRestoresTextColorAndBindsImplicitWidth();
    void itemDelegateSourceAvoidsFinalIconCollision();
};

QString ConfigControlsTest::compileError(const QString &qml)
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(qml.toUtf8(), QUrl());
    return component.isError() ? component.errorString() : QString();
}

QString ConfigControlsTest::readSource(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

//! Redeclaring implicitWidth on a PlasmaComponents TextField is fatal in Qt6 (final), so the type
//! never loads - this is exactly what broke the Latte TextField until it switched to a binding.
void ConfigControlsTest::textFieldImplicitWidthIsFinal()
{
    const QString err = compileError(QStringLiteral(
        "import org.kde.plasma.components 3.0 as PlasmaComponents\n"
        "PlasmaComponents.TextField { readonly property real implicitWidth: 5 }\n"));
    QVERIFY2(!err.isEmpty(), "Redeclaring TextField.implicitWidth must fail to compile in Qt6.");
    QVERIFY2(err.contains(QStringLiteral("final"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("Expected a 'final' override error, got: %1").arg(err)));
}

//! Binding the inherited implicitWidth (instead of redeclaring it) is the correct, accepted form.
void ConfigControlsTest::textFieldImplicitWidthBindingIsAccepted()
{
    QCOMPARE(compileError(QStringLiteral(
        "import org.kde.plasma.components 3.0 as PlasmaComponents\n"
        "PlasmaComponents.TextField { implicitWidth: 5 }\n")), QString());
}

//! AbstractButton.icon is final in Qt6; a custom 'icon' property (ItemDelegate) collides and the
//! whole type fails to load. ItemDelegate's property was renamed to iconSource because of this.
void ConfigControlsTest::abstractButtonIconIsFinal()
{
    const QString err = compileError(QStringLiteral(
        "import QtQuick.Controls\n"
        "Button { property string icon: \"x\" }\n"));
    QVERIFY2(!err.isEmpty(), "A custom 'icon' property on a Button must fail to compile in Qt6.");
    QVERIFY2(err.contains(QStringLiteral("final"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("Expected a 'final' override error, got: %1").arg(err)));
}

//! A Controls 2 Button has no tooltip property - assigning it must fail. This is why every Latte
//! ghost-tooltip button moved to the ToolTip attached property.
void ConfigControlsTest::button2HasNoTooltipProperty()
{
    QVERIFY2(!compileError(QStringLiteral(
        "import QtQuick.Controls\n"
        "Button { tooltip: \"x\" }\n")).isEmpty(),
        "Controls 2 Button has no 'tooltip' property; the assignment must not compile.");
}

//! The replacement pattern - the ToolTip attached property - does compile.
void ConfigControlsTest::button2AcceptsToolTipAttachedProperty()
{
    QCOMPARE(compileError(QStringLiteral(
        "import QtQuick.Controls\n"
        "Button { ToolTip.text: \"x\"; ToolTip.visible: false }\n")), QString());
}

//! Controls 2 TabBar dropped QtQuick Controls 1's currentTab; assigning it must fail.
void ConfigControlsTest::tabBar2HasNoCurrentTab()
{
    QVERIFY2(!compileError(QStringLiteral(
        "import QtQuick.Controls\n"
        "TabBar { currentTab: null }\n")).isEmpty(),
        "Controls 2 TabBar has no 'currentTab'; use currentIndex / currentItem.");
}

//! currentIndex is the Controls 2 replacement and compiles.
void ConfigControlsTest::tabBar2AcceptsCurrentIndex()
{
    QCOMPARE(compileError(QStringLiteral(
        "import QtQuick.Controls\n"
        "TabBar { currentIndex: 0 }\n")), QString());
}

//! A ListModel exposes get(); a plain JS array (and a Qt6 sequence model) does not. The ComboBox
//! must branch on the presence of get(), not on Array.isArray, to read the current row.
void ConfigControlsTest::listModelIsDistinguishedByItsGetMethod()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QByteArrayLiteral(
        "import QtQuick\n"
        "import QtQml.Models\n"
        "Item {\n"
        "    property bool listModelHasGet: false\n"
        "    property bool arrayHasGet: false\n"
        "    ListModel { id: lm; ListElement { name: \"a\" } }\n"
        "    property var arr: [1, 2, 3]\n"
        "    Component.onCompleted: {\n"
        "        listModelHasGet = (typeof lm.get === \"function\");\n"
        "        arrayHasGet = (typeof arr.get === \"function\");\n"
        "    }\n"
        "}\n"), QUrl());
    QScopedPointer<QObject> root(component.create());
    QVERIFY2(root, qPrintable(component.errorString()));
    QCOMPARE(QQmlProperty::read(root.data(), QStringLiteral("listModelHasGet")).toBool(), true);
    QCOMPARE(QQmlProperty::read(root.data(), QStringLiteral("arrayHasGet")).toBool(), false);
}

//! The QtQuick Controls 2 rebuild dropped the tooltip property from the Latte CheckBox; consumers
//! across every settings page set it. It must stay declared and wired to a ToolTip.
void ConfigControlsTest::checkBoxSourceRestoresTooltip()
{
    const QString src = readSource(QStringLiteral(CHECKBOX_QML_PATH));
    QVERIFY2(!src.isEmpty(), "CheckBox.qml source must be readable.");
    QVERIFY2(src.contains(QStringLiteral("property string tooltip")),
             "Latte CheckBox must declare a tooltip property.");
    QVERIFY2(src.contains(QStringLiteral("ToolTip.text")),
             "Latte CheckBox must wire tooltip to the ToolTip attached property.");
}

//! TextField lost textColor in the rebuild (the suffix label reads it), and it must bind the
//! inherited implicitWidth rather than redeclaring it (which is final in Qt6).
void ConfigControlsTest::textFieldSourceRestoresTextColorAndBindsImplicitWidth()
{
    const QString src = readSource(QStringLiteral(TEXTFIELD_QML_PATH));
    QVERIFY2(!src.isEmpty(), "TextField.qml source must be readable.");
    QVERIFY2(src.contains(QStringLiteral("property color textColor")),
             "Latte TextField must declare a textColor property.");
    QVERIFY2(!src.contains(QStringLiteral("readonly property int implicitWidth")),
             "TextField must not redeclare implicitWidth (final in Qt6); bind it instead.");
    QVERIFY2(src.contains(QStringLiteral("implicitWidth:")),
             "TextField must bind the inherited implicitWidth.");
}

//! ItemDelegate's custom icon property collided with the final AbstractButton.icon; it was renamed
//! to iconSource. The colliding declaration must not return.
void ConfigControlsTest::itemDelegateSourceAvoidsFinalIconCollision()
{
    const QString src = readSource(QStringLiteral(ITEMDELEGATE_QML_PATH));
    QVERIFY2(!src.isEmpty(), "ItemDelegate.qml source must be readable.");
    QVERIFY2(src.contains(QStringLiteral("property string iconSource")),
             "ItemDelegate must expose iconSource.");
    QVERIFY2(!src.contains(QStringLiteral("property string icon\n"))
                 && !src.contains(QStringLiteral("property string icon ")),
             "ItemDelegate must not declare a custom 'icon' property (collides with final icon).");
}

QTEST_MAIN(ConfigControlsTest)
#include "configcontrolstest.moc"
