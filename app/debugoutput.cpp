/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "debugoutput.h"

// Qt
#include <QFile>
#include <QIODevice>
#include <QLoggingCategory>
#include <QString>
#include <QTextStream>
#include <QTime>

//! COLORS
#define CNORMAL  "\033[0m"
#define CIGREEN  "\033[1;32m"
#define CGREEN   "\033[0;32m"
#define CICYAN   "\033[1;36m"
#define CCYAN    "\033[0;36m"
#define CIRED    "\033[1;31m"
#define CRED     "\033[0;31m"

namespace {
QString s_filterDebugMessageText;
QString s_filterDebugLogFile;

//! Qt hands back a callable default handler even when none was installed, so the
//! quiet path can drop the deprecation noise and still let everything else reach
//! wherever Qt normally puts it -- the journal on a desktop session.
QtMessageHandler s_previousHandler = nullptr;

void passThroughHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (Latte::DebugOutput::isSuppressedDeprecationMessage(msg)) {
        return;
    }

    if (s_previousHandler) {
        s_previousHandler(type, context, msg);
    }
}
}

namespace Latte {
namespace DebugOutput {

bool isSuppressedDeprecationMessage(const QString &msg)
{
    return msg.endsWith(QLatin1String("QML Binding: Not restoring previous value because restoreMode has not been set.This behavior is deprecated.In Qt < 6.0 the default is Binding.RestoreBinding.In Qt >= 6.0 the default is Binding.RestoreBindingOrValue."))
        || msg.endsWith(QLatin1String("QML Binding: Not restoring previous value because restoreMode has not been set.\nThis behavior is deprecated.\nYou have to import QtQml 2.15 after any QtQuick imports and set\nthe restoreMode of the binding to fix this warning.\nIn Qt < 6.0 the default is Binding.RestoreBinding.\nIn Qt >= 6.0 the default is Binding.RestoreBindingOrValue.\n"))
        || msg.endsWith(QLatin1String("QML Binding: Not restoring previous value because restoreMode has not been set.\nThis behavior is deprecated.\nYou have to import QtQml 2.15 after any QtQuick imports and set\nthe restoreMode of the binding to fix this warning.\nIn Qt < 6.0 the default is Binding.RestoreBinding.\nIn Qt >= 6.0 the default is Binding.RestoreBindingOrValue."))
        || msg.endsWith(QLatin1String("QML Connections: Implicitly defined onFoo properties in Connections are deprecated. Use this syntax instead: function onFoo(<arguments>) { ... }"));
}

void setMessageTextFilter(const QString &text)
{
    s_filterDebugMessageText = text;
}

void setLogFile(const QString &path)
{
    s_filterDebugLogFile = path;
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (isSuppressedDeprecationMessage(msg)) {
        return;
    }

    if (!s_filterDebugMessageText.isEmpty() && !msg.contains(s_filterDebugMessageText)) {
        return;
    }

    const char *function = context.function ? context.function : "";

    QString typeStr;
    switch (type) {
    case QtDebugMsg:
        typeStr = QStringLiteral("Debug");
        break;
    case QtInfoMsg:
        typeStr = QStringLiteral("Info");
        break;
    case QtWarningMsg:
        typeStr = QStringLiteral("Warning");
        break;
    case QtCriticalMsg:
        typeStr = QStringLiteral("Critical");
        break;
    case QtFatalMsg:
        typeStr = QStringLiteral("Fatal");
        break;
    };

    const char *TypeColor;

    if (type == QtInfoMsg || type == QtWarningMsg) {
        TypeColor = CGREEN;
    } else if (type == QtCriticalMsg || type == QtFatalMsg) {
        TypeColor = CRED;
    } else {
        TypeColor = CIGREEN;
    }

    if (s_filterDebugLogFile.isEmpty()) {
        //! Straight to stderr, not back through qDebug(): distros ship *.debug=false
        //! in qtlogging.ini, which drops debug messages before any handler runs, so
        //! re-emitting there printed nothing at all.
        QTextStream errts(stderr);
        errts << TypeColor << "[" << typeStr << " : " << CGREEN << QTime::currentTime().toString(QStringLiteral("h:mm:ss.zz")) << TypeColor << "]" << CNORMAL
#ifndef QT_NO_DEBUG
              << CIRED << " [" << CCYAN << function << CIRED << ":" << CCYAN << context.line << CIRED << "]"
#endif
              << CICYAN << " - " << CNORMAL << msg << Qt::endl;
    } else {
        const QString logline = QStringLiteral("[") + typeStr + QStringLiteral(" : ") + QTime::currentTime().toString(QStringLiteral("h:mm:ss.zz")) + QStringLiteral("] - ") + msg;

        QFile logfile(s_filterDebugLogFile);

        if (logfile.open(QIODevice::WriteOnly | QIODevice::Append)) {
            QTextStream logts(&logfile);
            logts << logline << Qt::endl;
        } else {
            //! a q*() call made from inside the installed handler skips the handler and lands in
            //! Qt's raw fallback, losing the prefix and filtering applied above, so report straight
            //! to stderr. Streaming into an unopened QFile instead drops the line and makes
            //! QIODevice warn about the closed device on every message that follows.
            QTextStream errts(stderr);
            errts << logline << Qt::endl;
        }
    }
}

bool outputRequested(const QCommandLineParser &parser)
{
    return parser.isSet(QStringLiteral("debug"))
            || parser.isSet(QStringLiteral("mask"))
            || parser.isSet(QStringLiteral("debug-text"))
            || parser.isSet(QStringLiteral("log-file"));
}

void installMessageHandler(bool debugMode)
{
    if (debugMode) {
        //! Distros ship *.debug=false in qtlogging.ini, which drops debug messages
        //! before any handler runs, so asking for diagnostics has to re-open the
        //! gate or --debug reports warnings only.
        QLoggingCategory::setFilterRules(QStringLiteral("default.debug=true"));
        qInstallMessageHandler(messageHandler);
        return;
    }

    //! Uncategorised debug output is noise outside --debug, and a distro shipping no
    //! rule of its own would get all of it. Warnings and worse still reach Qt's own
    //! handler, which on a desktop session means the journal.
    QLoggingCategory::setFilterRules(QStringLiteral("default.debug=false"));
    s_previousHandler = qInstallMessageHandler(passThroughHandler);
}

}
}
