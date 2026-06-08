/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Mirrors Latte::Templates::Manager::templateName() (app/templates/templatesmanager.cpp:315),
// which strips a recognised template extension (.layout.latte / .view.latte) off a
// template file path to get its display name. The static cannot be linked headlessly
// (templatesmanager.cpp pulls in Corona), so the logic is mirrored here and the real
// function carries the identical implementation.
//
// A path ending in NEITHER extension must be returned unchanged. The original code ran
// templatename.remove(ext, size) with ext == -1 (lastIndexOf miss), and Qt6
// QString::remove(-1, n) clamps to and removes the LAST character — so a stray file in
// the templates dir (e.g. "notes.txt") came back as "notes.tx".

#include <QObject>
#include <QString>
#include <QtTest>

class TemplatesNameTest : public QObject
{
    Q_OBJECT

private:
    // Mirror of Manager::templateName under test.
    static QString templateName(const QString &filePath)
    {
        int lastSlash = filePath.lastIndexOf(QLatin1Char('/'));
        QString templatename = filePath.mid(lastSlash + 1);

        const QString extensions[] = {QStringLiteral(".layout.latte"), QStringLiteral(".view.latte")};
        for (const QString &extension : extensions) {
            if (templatename.endsWith(extension)) {
                templatename.chop(extension.size());
                break;
            }
        }

        return templatename;
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
}

void TemplatesNameTest::templateName()
{
    QFETCH(QString, path);
    QFETCH(QString, expected);
    QCOMPARE(templateName(path), expected);
}

QTEST_GUILESS_MAIN(TemplatesNameTest)

#include "templatesnametest.moc"
