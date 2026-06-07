/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTextStream>

#include "backend.h"

class TasksBackendTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void isApplicationDetectsDesktopFiles();
    void isApplicationRejectsPlainFiles();
    void applicationCategoriesReadsCategories();
    void jsonArrayToUrlListConverts();
    void generateMimeDataCarriesTaskUrl();
    void highlightWindowsRoundTrips();

private:
    QString writeFile(const QTemporaryDir &dir, const QString &name, const QString &body);
};

QString TasksBackendTest::writeFile(const QTemporaryDir &dir, const QString &name, const QString &body)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream(&f) << body;
    f.close();
    return path;
}

void TasksBackendTest::isApplicationDetectsDesktopFiles()
{
    Backend backend;
    QTemporaryDir dir;
    const QString path = writeFile(dir, QStringLiteral("app.desktop"), QStringLiteral("[Desktop Entry]\nType=Application\nName=T\nExec=true\n"));
    QVERIFY(backend.isApplication(QUrl::fromLocalFile(path)));
}

void TasksBackendTest::isApplicationRejectsPlainFiles()
{
    Backend backend;
    QTemporaryDir dir;
    const QString path = writeFile(dir, QStringLiteral("plain.txt"), QStringLiteral("hello"));
    QVERIFY(!backend.isApplication(QUrl::fromLocalFile(path)));
    QVERIFY(!backend.isApplication(QUrl()));
}

void TasksBackendTest::applicationCategoriesReadsCategories()
{
    QTemporaryDir dir;
    const QString path =
        writeFile(dir, QStringLiteral("cat.desktop"), QStringLiteral("[Desktop Entry]\nType=Application\nName=T\nExec=true\nCategories=Qt;KDE;\n"));
    const QStringList cats = Backend::applicationCategories(QUrl::fromLocalFile(path));
    QVERIFY(cats.contains(QStringLiteral("Qt")));
    QVERIFY(cats.contains(QStringLiteral("KDE")));
}

void TasksBackendTest::jsonArrayToUrlListConverts()
{
    Backend backend;
    const QJsonArray arr{QStringLiteral("file:///tmp/a"), QStringLiteral("file:///tmp/b")};
    const QList<QUrl> urls = backend.jsonArrayToUrlList(arr);
    QCOMPARE(urls.size(), 2);
    QCOMPARE(urls.at(0), QUrl(QStringLiteral("file:///tmp/a")));
    QCOMPARE(urls.at(1), QUrl(QStringLiteral("file:///tmp/b")));
}

void TasksBackendTest::generateMimeDataCarriesTaskUrl()
{
    const QUrl url(QStringLiteral("file:///tmp/a"));
    const QVariantMap m = Backend::generateMimeData(QStringLiteral("application/x-test"), QVariant(QByteArrayLiteral("payload")), url);
    QCOMPARE(m.value(QStringLiteral("text/x-orgkdeplasmataskmanager_taskurl")).toString(), QStringLiteral("file:///tmp/a"));
    QVERIFY(m.contains(QStringLiteral("application/x-test")));
}

void TasksBackendTest::highlightWindowsRoundTrips()
{
    Backend backend;
    QCOMPARE(backend.highlightWindows(), false);
    backend.setHighlightWindows(true);
    QCOMPARE(backend.highlightWindows(), true);
}

QTEST_MAIN(TasksBackendTest)
#include "tasksbackendtest.moc"
