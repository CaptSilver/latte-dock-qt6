/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include "../declarativeimports/core/iconsourceclassifier.h"

using namespace Latte::IconSourceClassifier;

class IconSourceClassifierTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void sourceName_plainString_returnsString();
    void sourceName_imageVariant_returnsEmpty();
    void classify_localFileUrl_isLocalFile();
    void classify_plainName_isSvgOrIconName();
    void classify_relativeToken_isSvgOrIconName();
    void classify_qimage_isImage();
    void classify_emptyVariant_isClear();
    void classify_emptyString_isClear();
    void filter_empty_isFiltered();
    void filter_executablePlaceholder_isFiltered();
    void filter_realName_isNotFiltered();
    void isValid_truthTable();
    void isValid_truthTable_data();
};

void IconSourceClassifierTest::sourceName_plainString_returnsString()
{
    QCOMPARE(sourceName(QVariant(QStringLiteral("firefox"))), QStringLiteral("firefox"));
}

void IconSourceClassifierTest::sourceName_imageVariant_returnsEmpty()
{
    // QImage::toString() is empty; QImage does not canConvert<QIcon>(), so name stays "".
    QCOMPARE(sourceName(QVariant(QImage(4, 4, QImage::Format_ARGB32))), QString());
}

void IconSourceClassifierTest::classify_localFileUrl_isLocalFile()
{
    QCOMPARE(classify(QVariant(QStringLiteral("file:///tmp/x.png"))), SourceKind::LocalFile);
}

void IconSourceClassifierTest::classify_plainName_isSvgOrIconName()
{
    QCOMPARE(classify(QVariant(QStringLiteral("firefox"))), SourceKind::SvgOrIconName);
}

void IconSourceClassifierTest::classify_relativeToken_isSvgOrIconName()
{
    QCOMPARE(classify(QVariant(QStringLiteral("plain-icon-name"))), SourceKind::SvgOrIconName);
}

void IconSourceClassifierTest::classify_qimage_isImage()
{
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::red);
    QVariant v(img);
    QCOMPARE(classify(v), SourceKind::Image);
}

void IconSourceClassifierTest::classify_emptyVariant_isClear()
{
    QVariant empty;
    QCOMPARE(classify(empty), SourceKind::Clear);
}

void IconSourceClassifierTest::classify_emptyString_isClear()
{
    QCOMPARE(classify(QVariant(QString())), SourceKind::Clear);
}

void IconSourceClassifierTest::filter_empty_isFiltered()
{
    QVERIFY(isFilteredSourceName(QString()));
}

void IconSourceClassifierTest::filter_executablePlaceholder_isFiltered()
{
    QVERIFY(isFilteredSourceName(QStringLiteral("application-x-executable")));
}

void IconSourceClassifierTest::filter_realName_isNotFiltered()
{
    QVERIFY(!isFilteredSourceName(QStringLiteral("firefox")));
}

void IconSourceClassifierTest::isValid_truthTable_data()
{
    QTest::addColumn<bool>("hasIcon");
    QTest::addColumn<bool>("hasSvg");
    QTest::addColumn<bool>("hasImage");
    QTest::addColumn<bool>("expected");

    QTest::newRow("none")        << false << false << false << false;
    QTest::newRow("icon only")   << true  << false << false << true;
    QTest::newRow("svg only")    << false << true  << false << true;
    QTest::newRow("image only")  << false << false << true  << true;
    QTest::newRow("icon+svg")    << true  << true  << false << true;
    QTest::newRow("icon+image")  << true  << false << true  << true;
    QTest::newRow("svg+image")   << false << true  << true  << true;
    QTest::newRow("all")         << true  << true  << true  << true;
}

void IconSourceClassifierTest::isValid_truthTable()
{
    QFETCH(bool, hasIcon);
    QFETCH(bool, hasSvg);
    QFETCH(bool, hasImage);
    QFETCH(bool, expected);
    QCOMPARE(isValid(ResolvedIcon{hasIcon, hasSvg, hasImage}), expected);
}

QTEST_GUILESS_MAIN(IconSourceClassifierTest)
#include "iconsourceclassifiertest.moc"
