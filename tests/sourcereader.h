/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LATTETESTS_SOURCEREADER_H
#define LATTETESTS_SOURCEREADER_H

// Shared primitives for the guards that read real project files instead of linking against
// them -- the one-token C++ fixes, the shell scripts, the QML invariants. Each of those test
// binaries had grown its own QFile reader, under readFile / readSource / readScript depending
// on who wrote it, and the brace matcher had been copied around with them.
//
// Every consumer needs REPO_ROOT, so tests/CMakeLists.txt hands it to all of them in one
// foreach rather than each target inventing a bespoke *_PATH macro for the single file it
// happens to read.

#ifndef REPO_ROOT
#error "sourcereader.h needs REPO_ROOT; add the target to the REPO_ROOT foreach in tests/CMakeLists.txt"
#endif

#include <QFile>
#include <QLatin1Char>
#include <QList>
#include <QRegularExpression>
#include <QString>

namespace LatteTest {

//! Absolute path of a file in the checkout this build was configured from.
inline QString repoPath(const QString &rel)
{
    return QStringLiteral("%1/%2").arg(QStringLiteral(REPO_ROOT), rel);
}

//! Contents of an absolute path, or an empty string when it cannot be read. Callers that treat
//! an empty result as "the invariant holds" must assert !isEmpty() themselves -- a missing file
//! passes every negative assertion otherwise.
inline QString readFile(const QString &absolutePath)
{
    QFile f(absolutePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(f.readAll());
}

inline QString readRepoFile(const QString &rel)
{
    return readFile(repoPath(rel));
}

//! Brace-matched block, outer braces included, starting at the first `{` at or after `from`.
//! Deliberately positional and unguarded: the C++ callers query a bare signature and the
//! parameter list sits between it and the brace, so anything stricter would silently return
//! an empty body for every one of them.
inline QString bracedBlockAfter(const QString &src, int from)
{
    const int brace = src.indexOf(QLatin1Char('{'), from);
    if (brace == -1) {
        return QString();
    }
    int depth = 0;
    int i = brace;
    for (; i < src.size(); ++i) {
        if (src.at(i) == QLatin1Char('{')) {
            ++depth;
        } else if (src.at(i) == QLatin1Char('}') && --depth == 0) {
            ++i;
            break;
        }
    }
    return src.mid(brace, i - brace);
}

//! Body of the first `sig { ... }` in `src`, outer braces included.
inline QString functionBody(const QString &src, const QString &sig)
{
    const int s = src.indexOf(sig);
    if (s == -1) {
        return QString();
    }
    return bracedBlockAfter(src, s + sig.size());
}

//! Body of every `name { ... }` QML element, in source order. Here the whitespace check between
//! the name and the brace earns its keep: it is what tells the `Binding` element apart from a
//! `Binding.RestoreNone` reference or a `Bindings` sibling.
inline QList<QString> elementBlocks(const QString &src, const QString &name)
{
    QList<QString> blocks;
    int idx = 0;
    while ((idx = src.indexOf(name, idx)) != -1) {
        const int after = idx + name.size();
        const int brace = src.indexOf(QLatin1Char('{'), after);
        if (brace == -1 || !src.mid(after, brace - after).trimmed().isEmpty()) {
            idx = after;
            continue;
        }
        const QString block = bracedBlockAfter(src, brace);
        blocks << block;
        idx = brace + block.size();
    }
    return blocks;
}

//! Whitespace-free copy, so an assertion can spell the fixed form without pinning the formatting.
inline QString stripped(const QString &body)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    QString s = body;
    s.remove(whitespace);
    return s;
}

}

#endif
