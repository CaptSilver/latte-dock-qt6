/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Pins the Plasma 6 ContainmentInterface::createApplet contract that
// AlternativesHelper::onAlternativesTriggered relies on. Latte swaps an applet
// for an alternative via QMetaObject::invokeMethod(containmentItem,
// "createApplet", ...). Plasma 6 changed the geometry argument from QPoint to a
// QRectF hint; passing the wrong type makes the invoke fail to resolve and the
// swap silently no-op. This reads Plasma's installed type info and fails if the
// geom parameter is no longer QRectF, flagging that the helper's Q_ARG needs to
// follow. Skips where the Plasma plasmoid QML module is not installed.

#include <QFile>
#include <QLibraryInfo>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QtTest>

class AlternativesCreateAppletTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void createAppletTakesQRectFGeometry();
};

void AlternativesCreateAppletTest::createAppletTakesQRectFGeometry()
{
    // Qt knows where QML modules live; derive the path instead of hard-coding it.
    const QString qmlPath = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    const QString typesFile =
        qmlPath + QStringLiteral("/org/kde/plasma/plasmoid/plasmoidplugin.qmltypes");

    if (!QFile::exists(typesFile)) {
        QSKIP("Plasma plasmoid QML module not installed; createApplet contract unverifiable here");
    }

    QFile f(typesFile);
    QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(typesFile));
    const QString content = QString::fromUtf8(f.readAll());

    const int idx = content.indexOf(QStringLiteral("name: \"createApplet\""));
    QVERIFY2(idx >= 0, "createApplet not found in plasmoidplugin.qmltypes");

    // The geometry parameter of the createApplet method block.
    const QRegularExpression geomRe(
        QStringLiteral("Parameter \\{ name: \"geom\"; type: \"([A-Za-z0-9_]+)\" \\}"));
    const QRegularExpressionMatch m = geomRe.match(content, idx);
    QVERIFY2(m.hasMatch(), "createApplet geometry parameter not found");
    QCOMPARE(m.captured(1), QStringLiteral("QRectF"));
}

QTEST_GUILESS_MAIN(AlternativesCreateAppletTest)

#include "alternativescreateapplettest.moc"
