/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The naming contract of Latte::Templates::Manager::templateName(): the display name of a
// template is its file name with a recognised extension (.layout.latte / .view.latte)
// stripped off the END, and a file ending in neither comes back untouched.
//
// templatesmanager.cpp cannot be linked headlessly -- its header pulls in Corona -- so
// templateName() delegates to Latte::CoronaHelpers::strippedLatteName(), which is what this
// test drives. sourceguardtest::templatesManager_templateName_delegatesToTheSharedStrip
// is what keeps the two ends tied together. (This test used to carry a private copy of the
// function body and assert against that, so it stayed green no matter what the app did.)
//
// The rows below are the ones that bit: the original code ran templatename.remove(ext, size)
// with ext == -1 from a lastIndexOf() miss, and Qt6 QString::remove(-1, n) clamps to and
// removes the LAST character -- so a stray "notes.txt" in the templates dir was listed as
// "notes.tx".

#include "coronahelpers.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtTest>

using namespace Latte;

class TemplatesNameTest : public QObject
{
    Q_OBJECT

private:
    static QString templateName(const QString &filePath)
    {
        return CoronaHelpers::strippedLatteName(filePath, {CoronaHelpers::LAYOUTEXTENSION, CoronaHelpers::VIEWEXTENSION});
    }

private Q_SLOTS:
    void templateName_data();
    void templateName();
};

void TemplatesNameTest::templateName_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("expected");

    QTest::newRow("layout extension")     << QStringLiteral("/p/Default.layout.latte")       << QStringLiteral("Default");
    QTest::newRow("view extension")       << QStringLiteral("/p/Default Dock.view.latte")     << QStringLiteral("Default Dock");
    QTest::newRow("spaces in name")       << QStringLiteral("/p/Default Panel.view.latte")    << QStringLiteral("Default Panel");
    QTest::newRow("no directory")         << QStringLiteral("Empty.layout.latte")             << QStringLiteral("Empty");
    QTest::newRow("non-template kept")    << QStringLiteral("/p/notes.txt")                   << QStringLiteral("notes.txt");
    QTest::newRow("no extension kept")    << QStringLiteral("/p/Plasma")                      << QStringLiteral("Plasma");
    QTest::newRow("dotfile kept")         << QStringLiteral("/p/.directory")                  << QStringLiteral(".directory");
    QTest::newRow("dots in name")         << QStringLiteral("/p/Plasma 5.27.layout.latte")    << QStringLiteral("Plasma 5.27");
}

void TemplatesNameTest::templateName()
{
    QFETCH(QString, path);
    QFETCH(QString, expected);
    QCOMPARE(templateName(path), expected);
}

QTEST_GUILESS_MAIN(TemplatesNameTest)

#include "templatesnametest.moc"
