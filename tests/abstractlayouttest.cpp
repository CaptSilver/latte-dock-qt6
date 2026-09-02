/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Exercises the real Latte::Layout::AbstractLayout port code (linked directly):
//  - the [LayoutSettings] save/load round-trip (each setter persists via its
//    change signal; a fresh object must read the values back), and
//  - the static layoutName() path/extension parser.
// AbstractLayout links against only Qt + KConfig + Plasma headers, so it is one
// of the few storage classes that can be driven headlessly without a Corona.

#include "abstractlayout.h"

#include <KConfig>
#include <KConfigGroup>
#include <QObject>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

using namespace Latte::Layout;

class AbstractLayoutTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private Q_SLOTS:
    void initTestCase();
    void layoutSettingsRoundTrip();
    void layoutName_data();
    void layoutName();
    void defaultTextColor_data();
    void defaultTextColor();
};

void AbstractLayoutTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void AbstractLayoutTest::layoutSettingsRoundTrip()
{
    const QString path = m_dir.filePath(QStringLiteral("Test.layout.latte"));

    // AbstractLayout's ctor only loads an existing file, so seed it first.
    {
        KConfig seed(path);
        seed.group(QStringLiteral("LayoutSettings")).writeEntry(QStringLiteral("version"), 1);
        seed.sync();
    }

    const QStringList launchers = {QStringLiteral("applications:firefox.desktop"),
                                   QStringLiteral("applications:org.kde.dolphin.desktop")};

    // Set values that all differ from the loaded defaults, so each setter fires
    // its change signal and persists through saveConfig.
    {
        AbstractLayout layout(nullptr, path, QStringLiteral("Test"));
        layout.setVersion(5);
        layout.setColor(QStringLiteral("purple"));
        layout.setIcon(QStringLiteral("starred-symbolic"));
        layout.setLaunchers(launchers);
        layout.setPopUpMargin(9);
        layout.setPreferredForShortcutsTouched(true);
        layout.syncSettings();
    }

    // A fresh object must read every field back from disk.
    AbstractLayout reloaded(nullptr, path, QStringLiteral("Test"));
    QCOMPARE(reloaded.version(), 5);
    QCOMPARE(reloaded.color(), QStringLiteral("purple"));
    QCOMPARE(reloaded.icon(), QStringLiteral("starred-symbolic"));
    QCOMPARE(reloaded.launchers(), launchers);
    QCOMPARE(reloaded.popUpMargin(), 9);
    QCOMPARE(reloaded.preferredForShortcutsTouched(), true);

    // The values really landed under [LayoutSettings] on disk.
    KConfig disk(path);
    const KConfigGroup ls = disk.group(QStringLiteral("LayoutSettings"));
    QCOMPARE(ls.readEntry(QStringLiteral("color"), QString()), QStringLiteral("purple"));
    QCOMPARE(ls.readEntry(QStringLiteral("launchers"), QStringList()), launchers);
}

void AbstractLayoutTest::layoutName_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("expected");

    QTest::newRow("layout file")       << QStringLiteral("/a/b/My Layout.layout.latte") << QStringLiteral("My Layout");
    QTest::newRow("bare layout file")  << QStringLiteral("Default.layout.latte")        << QStringLiteral("Default");
    QTest::newRow("non-layout kept")   << QStringLiteral("/p/notes.txt")                << QStringLiteral("notes.txt");
    QTest::newRow("no extension kept") << QStringLiteral("/p/Plasma")                   << QStringLiteral("Plasma");
}

void AbstractLayoutTest::layoutName()
{
    QFETCH(QString, path);
    QFETCH(QString, expected);
    QCOMPARE(AbstractLayout::layoutName(path), expected);
}

void AbstractLayoutTest::defaultTextColor_data()
{
    QTest::addColumn<QString>("color");
    QTest::addColumn<QString>("expected");

    //! these eleven ids are the ones the details-dialog color model publishes;
    //! anything else has to land on the neutral fallback
    QTest::newRow("blue")         << QStringLiteral("blue")         << QStringLiteral("#D7E3FF");
    QTest::newRow("brown")        << QStringLiteral("brown")        << QStringLiteral("#F1DECB");
    QTest::newRow("darkgrey")     << QStringLiteral("darkgrey")     << QStringLiteral("#ECECEC");
    QTest::newRow("gold")         << QStringLiteral("gold")         << QStringLiteral("#7C3636");
    QTest::newRow("green")        << QStringLiteral("green")        << QStringLiteral("#4D7549");
    QTest::newRow("lightskyblue") << QStringLiteral("lightskyblue") << QStringLiteral("#0C2A43");
    QTest::newRow("orange")       << QStringLiteral("orange")       << QStringLiteral("#6F3902");
    QTest::newRow("pink")         << QStringLiteral("pink")         << QStringLiteral("#743C46");
    QTest::newRow("purple")       << QStringLiteral("purple")       << QStringLiteral("#ECD9FF");
    QTest::newRow("red")          << QStringLiteral("red")          << QStringLiteral("#F3E4E4");
    QTest::newRow("wheat")        << QStringLiteral("wheat")        << QStringLiteral("#6A4E25");
    QTest::newRow("unknown id")   << QStringLiteral("teal")         << QStringLiteral("#FCFCFC");
    QTest::newRow("empty id")     << QString()                      << QStringLiteral("#FCFCFC");
}

void AbstractLayoutTest::defaultTextColor()
{
    QFETCH(QString, color);
    QFETCH(QString, expected);
    QCOMPARE(AbstractLayout::defaultTextColor(color), expected);
}

QTEST_GUILESS_MAIN(AbstractLayoutTest)

#include "abstractlayouttest.moc"
