/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Latte's diagnostic switches are the only way to see anything it reports: the
// non-debug launch installs a handler that drops every message on the floor.
//
// Two things were broken here. --log-file was never part of the condition that
// selects the real handler, so on its own it silently wrote nothing. And the
// handler re-emitted each formatted line through qDebug(), which distro logging
// rules (*.debug=false in qtlogging.ini) drop before any handler runs -- so
// --debug printed nothing at all on Fedora.

#include "debugoutput.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QLoggingCategory>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>

#include <cstdio>
#include <unistd.h>

class DebugOutputTest : public QObject
{
    Q_OBJECT

private:
    //! Mirrors the options main() registers, so the wiring is tested against the
    //! same names the user types.
    void addOptions(QCommandLineParser &parser);
    QString captureStderr(const std::function<void()> &emitMessages);

private Q_SLOTS:
    void initTestCase();
    void logFileAloneSelectsTheRealHandler();
    void debugAndMaskSelectTheRealHandler();
    void bareCommandLineAsksForNoOutput();
    void handlerReachesStderrDespiteDistroDebugRules();
    void handlerWritesToTheLogFile();
    void unopenableLogFileFallsBackToStderr();
    void deprecationNoiseIsSuppressed();
};

void DebugOutputTest::addOptions(QCommandLineParser &parser)
{
    parser.addOptions({{{QStringLiteral("d"), QStringLiteral("debug")}, QStringLiteral("debug")},
                       {QStringLiteral("mask"), QStringLiteral("mask")}});
    QCommandLineOption debugText(QStringList() << QStringLiteral("debug-text"));
    debugText.setValueName(QStringLiteral("t"));
    parser.addOption(debugText);
    QCommandLineOption logFile(QStringList() << QStringLiteral("log-file"));
    logFile.setValueName(QStringLiteral("f"));
    parser.addOption(logFile);
}

QString DebugOutputTest::captureStderr(const std::function<void()> &emitMessages)
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("stderr.txt"));

    fflush(stderr);
    const int saved = dup(fileno(stderr));
    FILE *redirected = freopen(path.toLocal8Bit().constData(), "w", stderr);
    Q_UNUSED(redirected)

    emitMessages();

    fflush(stderr);
    dup2(saved, fileno(stderr));
    ::close(saved);

    QFile out(path);
    if (!out.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    return QString::fromUtf8(out.readAll());
}

void DebugOutputTest::initTestCase()
{
    // Assert independence from whatever the host's qtlogging.ini says: this is the
    // rule Fedora ships, and it is what made --debug silent.
    QLoggingCategory::setFilterRules(QStringLiteral("default.debug=false"));
}

void DebugOutputTest::logFileAloneSelectsTheRealHandler()
{
    QCommandLineParser parser;
    addOptions(parser);
    QVERIFY(parser.parse({QStringLiteral("latte-dock"), QStringLiteral("--log-file"), QStringLiteral("/tmp/x.log")}));

    QVERIFY2(Latte::DebugOutput::outputRequested(parser),
             "--log-file on its own must install the handler, or it writes nothing");
}

void DebugOutputTest::debugAndMaskSelectTheRealHandler()
{
    for (const QString &opt : {QStringLiteral("--debug"), QStringLiteral("--mask")}) {
        QCommandLineParser parser;
        addOptions(parser);
        QVERIFY(parser.parse({QStringLiteral("latte-dock"), opt}));
        QVERIFY2(Latte::DebugOutput::outputRequested(parser), qPrintable(opt));
    }
}

void DebugOutputTest::bareCommandLineAsksForNoOutput()
{
    QCommandLineParser parser;
    addOptions(parser);
    QVERIFY(parser.parse({QStringLiteral("latte-dock")}));

    QVERIFY(!Latte::DebugOutput::outputRequested(parser));
}

void DebugOutputTest::handlerReachesStderrDespiteDistroDebugRules()
{
    Latte::DebugOutput::setLogFile(QString());

    const QString captured = captureStderr([]() {
        Latte::DebugOutput::messageHandler(QtWarningMsg, QMessageLogContext(), QStringLiteral("probe-visible"));
    });

    QVERIFY2(captured.contains(QStringLiteral("probe-visible")),
             qPrintable(QStringLiteral("handler produced nothing on stderr; got: ") + captured));
}

void DebugOutputTest::handlerWritesToTheLogFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString logpath = dir.filePath(QStringLiteral("latte.log"));

    Latte::DebugOutput::setLogFile(logpath);
    Latte::DebugOutput::messageHandler(QtWarningMsg, QMessageLogContext(), QStringLiteral("probe-logged"));
    Latte::DebugOutput::setLogFile(QString());

    QFile logfile(logpath);
    QVERIFY2(logfile.open(QIODevice::ReadOnly | QIODevice::Text), "no log file was written");
    const QString contents = QString::fromUtf8(logfile.readAll());

    QVERIFY(contents.contains(QStringLiteral("probe-logged")));
    QVERIFY(contents.contains(QStringLiteral("[Warning : ")));
}

void DebugOutputTest::unopenableLogFileFallsBackToStderr()
{
    // Streaming into a QFile that never opened drops the line and makes QIODevice
    // warn about the closed device -- on every message after it, through this very
    // handler. So an unusable log path has to fall back rather than write blind.
    Latte::DebugOutput::setLogFile(QStringLiteral("/proc/definitely/not/writable.log"));

    const QString captured = captureStderr([]() {
        Latte::DebugOutput::messageHandler(QtWarningMsg, QMessageLogContext(), QStringLiteral("probe-fallback"));
    });

    Latte::DebugOutput::setLogFile(QString());

    QVERIFY2(captured.contains(QStringLiteral("probe-fallback")),
             qPrintable(QStringLiteral("nothing reached stderr; got: ") + captured));
    QVERIFY2(!captured.contains(QStringLiteral("QIODevice")),
             qPrintable(QStringLiteral("wrote into an unopened device; got: ") + captured));
}

void DebugOutputTest::deprecationNoiseIsSuppressed()
{
    const QString onFoo = QStringLiteral("file.qml:1:1: QML Connections: Implicitly defined onFoo properties in Connections are deprecated. Use this syntax instead: function onFoo(<arguments>) { ... }");

    QVERIFY(Latte::DebugOutput::isSuppressedDeprecationMessage(onFoo));
    QVERIFY(!Latte::DebugOutput::isSuppressedDeprecationMessage(QStringLiteral("a real warning")));
}

QTEST_MAIN(DebugOutputTest)

#include "debugoutputtest.moc"
