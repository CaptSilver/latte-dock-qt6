/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Regression test for the QRegExp -> QRegularExpression conversion of the
// copy-name dedup used by Importer::uniqueLayoutName() (app/layouts/importer.cpp)
// and the identical sites in layoutscontroller, viewscontroller and
// templatesmanager. The copy suffix is " - [0-9]+"; the dedup must locate the
// LAST such suffix (lastIndexOf) and strip it before appending a fresh " - N".
// QRegularExpression does not auto-anchor (unlike a fully-specified QRegExp),
// so this guards the exact construct against a silent matching/position change.

#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QtTest>

class ImporterNameTest : public QObject
{
    Q_OBJECT

private:
    // Mirrors importer.cpp:744 — the converted construct under test.
    static int suffixPos(const QString &name)
    {
        return name.lastIndexOf(QRegularExpression(QStringLiteral(" - [0-9]+")));
    }

    // Mirrors importer.cpp:744-748 — strip a recognised copy suffix.
    static QString stripCopySuffix(QString name)
    {
        const int pos = suffixPos(name);
        if (pos > 0) {
            name = name.left(pos);
        }
        return name;
    }

    // Mirrors Importer::nameOfConfigFile() (importer.cpp:702): drop the directory
    // and a trailing .latterc. A name ending in neither must survive intact — the
    // original remove(lastIndexOf, 8) chopped the last character on a -1 miss.
    static QString nameOfConfigFile(const QString &fileName)
    {
        int lastSlash = fileName.lastIndexOf(QLatin1String("/"));
        QString layoutName = fileName.mid(lastSlash + 1);

        const QString extension(QStringLiteral(".latterc"));
        if (layoutName.endsWith(extension)) {
            layoutName.chop(extension.size());
        }
        return layoutName;
    }

private Q_SLOTS:
    void suffixPosition_data();
    void suffixPosition();
    void stripCopySuffix_data();
    void stripCopySuffix();
    void configFileName_data();
    void configFileName();
};

void ImporterNameTest::suffixPosition_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("expected");

    QTest::newRow("no suffix")            << QStringLiteral("Default")          << -1;
    QTest::newRow("simple copy")          << QStringLiteral("Default - 2")      << 7;
    QTest::newRow("multi-digit")          << QStringLiteral("My Layout - 10")   << 9;
    QTest::newRow("no spaces not matched")<< QStringLiteral("Default-2")        << -1;
    QTest::newRow("last of several")      << QStringLiteral("Default - 2 - 3")  << 11;
    QTest::newRow("dash without number")  << QStringLiteral("Plasma - Foo")     << -1;
    QTest::newRow("number not a suffix")  << QStringLiteral("Layout 2")         << -1;
    QTest::newRow("embedded copy kept")   << QStringLiteral("A - 1 tail")       << 1;
}

void ImporterNameTest::suffixPosition()
{
    QFETCH(QString, name);
    QFETCH(int, expected);
    QCOMPARE(suffixPos(name), expected);
}

void ImporterNameTest::stripCopySuffix_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("expected");

    QTest::newRow("no suffix unchanged")  << QStringLiteral("Default")         << QStringLiteral("Default");
    QTest::newRow("strip simple copy")    << QStringLiteral("Default - 2")     << QStringLiteral("Default");
    QTest::newRow("strip multi-digit")    << QStringLiteral("My Layout - 10")  << QStringLiteral("My Layout");
    QTest::newRow("strip last suffix")    << QStringLiteral("Default - 2 - 3") << QStringLiteral("Default - 2");
    QTest::newRow("no-space name kept")   << QStringLiteral("Default-2")       << QStringLiteral("Default-2");
}

void ImporterNameTest::stripCopySuffix()
{
    QFETCH(QString, name);
    QFETCH(QString, expected);
    QCOMPARE(stripCopySuffix(name), expected);
}

void ImporterNameTest::configFileName_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("expected");

    QTest::newRow("strips latterc")    << QStringLiteral("/home/u/My Config.latterc") << QStringLiteral("My Config");
    QTest::newRow("bare latterc")      << QStringLiteral("foo.latterc")                << QStringLiteral("foo");
    QTest::newRow("non-latterc kept")  << QStringLiteral("/p/lattedockrc")             << QStringLiteral("lattedockrc");
    QTest::newRow("no extension kept") << QStringLiteral("/p/Plasma")                  << QStringLiteral("Plasma");
}

void ImporterNameTest::configFileName()
{
    QFETCH(QString, path);
    QFETCH(QString, expected);
    QCOMPARE(nameOfConfigFile(path), expected);
}

QTEST_GUILESS_MAIN(ImporterNameTest)

#include "importernametest.moc"
