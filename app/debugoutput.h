/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LATTEDEBUGOUTPUT_H
#define LATTEDEBUGOUTPUT_H

// Qt
#include <QCommandLineParser>
#include <QString>
#include <QtGlobal>

namespace Latte {
namespace DebugOutput {

//! Deprecation warnings Qt emits for QML this project has not migrated yet. They
//! say nothing actionable and would drown everything else, so both handlers drop
//! them rather than the whole message stream.
bool isSuppressedDeprecationMessage(const QString &msg);

void setMessageTextFilter(const QString &text);
void setLogFile(const QString &path);

//! Formats and writes every message; installed when a diagnostic switch is given.
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

//! Dependable diagnostic sink for the QML edit-mode/interaction paths: QML console.log() and even
//! qDebug() get eaten by Latte's message handler and inconsistent journald routing. Write straight
//! to a dedicated file (and stderr) so the output is captured no matter how the process is launched.
//! Gated by LATTE_DEBUG_EDITMODE so the call sites stay in the tree long-term, silent by default.
void editModeLog(const QString &msg);

//! True when the user asked for diagnostics on this command line.
bool outputRequested(const QCommandLineParser &parser);

void installMessageHandler(bool debugMode);

}
}

#endif
