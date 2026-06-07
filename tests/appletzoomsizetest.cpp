/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// An applet's on-screen length/thickness is computed as zoomScale * layoutLength, where
// layoutLength is captured by a Binding that is only active while zoomScale === 1 (the rest
// state). The applet relies on that captured value PERSISTING once the binding deactivates
// during zoom. Qt5 kept the value; Qt6 changed the Binding default to RestoreBindingOrValue,
// which resets the property to its declared default (0) when "when" turns false, collapsing the
// applet to zero size on hover (it vanished). The fix is restoreMode: Binding.RestoreNone.
//
// This test pins that behavior so the regression cannot return silently.

#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QObject>

class AppletZoomSizeTest : public QObject
{
    Q_OBJECT

private:
    //! Loads an Item whose captured length is gated on zoomScale===1, using the given restoreMode
    //! expression, and returns the scaledLength after zoomScale is raised above 1.
    qreal scaledLengthAfterZoom(const QString &restoreModeLine);

private Q_SLOTS:
    void restoreNoneKeepsLengthThroughZoom();
    void qt6DefaultResetsLengthThroughZoom();
};

qreal AppletZoomSizeTest::scaledLengthAfterZoom(const QString &restoreModeLine)
{
    QQmlEngine engine;
    QQmlComponent component(&engine);

    const QString qml = QStringLiteral(R"(
        import QtQuick 2.15
        Item {
            id: root
            property real zoomScale: 1
            property real layoutLength: 0
            readonly property real scaledLength: zoomScale * layoutLength
            Binding {
                target: root
                property: "layoutLength"
                %1
                when: root.zoomScale === 1
                value: 64
            }
        }
    )").arg(restoreModeLine);

    component.setData(qml.toUtf8(), QUrl());
    QObject *root = component.create();
    [&]() { QVERIFY2(root, qPrintable(component.errorString())); }();
    if (!root) {
        return -1;
    }

    //! rest state: the binding is active, capturing the length
    [&]() { QCOMPARE(QQmlProperty::read(root, QStringLiteral("scaledLength")).toReal(), 64.0); }();

    //! zoom in: "when" turns false and the binding deactivates
    QQmlProperty::write(root, QStringLiteral("zoomScale"), 1.84375);

    const qreal scaled = QQmlProperty::read(root, QStringLiteral("scaledLength")).toReal();
    delete root;
    return scaled;
}

//! With RestoreNone the captured length survives, so the applet grows on zoom instead of vanishing.
void AppletZoomSizeTest::restoreNoneKeepsLengthThroughZoom()
{
    const qreal scaled = scaledLengthAfterZoom(QStringLiteral("restoreMode: Binding.RestoreNone"));
    QCOMPARE(scaled, 1.84375 * 64.0); // 118
}

//! The Qt6 default (RestoreBindingOrValue) resets layoutLength to 0 on deactivation -> zero size.
//! This documents the exact regression the fix guards against.
void AppletZoomSizeTest::qt6DefaultResetsLengthThroughZoom()
{
    const qreal scaled = scaledLengthAfterZoom(QString()); // no restoreMode -> Qt6 default
    QCOMPARE(scaled, 0.0);
}

QTEST_MAIN(AppletZoomSizeTest)
#include "appletzoomsizetest.moc"
