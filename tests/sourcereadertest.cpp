/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The source-reading guards (sourceguardtest, scriptguardtest, bindingrestoremodetest and the
// rest) all read real files and extract brace-matched blocks out of them. Those primitives now
// live in one header, and they are the only part of the guard suite whose failure mode is
// silence: a reader that hands back an empty QString, or a brace scan that refuses a match,
// turns a negative assertion like QVERIFY(!body.contains(x)) green while it stops guarding
// anything. So the primitives get their own tests, on synthetic sources with known answers.

#include "sourcereader.h"

#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QtTest>

using namespace LatteTest;

class SourceReaderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void functionBodyAcceptsASignatureFollowedByAParameterList();
    void functionBodyKeepsNestedBraces();
    void functionBodyOfAnAbsentSignatureIsEmpty();
    void bracedBlockAfterStartsAtTheNextBrace();
    void elementBlocksRejectsAttachedReferencesAndLongerNames();
    void elementBlocksReturnsEveryBlockInOrder();
    void repoPathResolvesAgainstTheCheckout();
    void readRepoFileOfAnAbsentPathIsEmpty();
    void strippedCollapsesWhitespaceAcrossNewlines();
};

//! The whole reason the raw brace scan carries no "nothing but whitespace before the '{'" guard.
//! Every C++ caller queries a bare signature -- "void Positioner::updatePosition" -- and the text
//! between that and the brace is the parameter list. A guarded primitive returns an empty body for
//! all of them, and the negative assertions built on it pass on nothing.
void SourceReaderTest::functionBodyAcceptsASignatureFollowedByAParameterList()
{
    const QString src = QStringLiteral("void Positioner::updatePosition(QPoint p)\n{\n    m_pos = p;\n}\n");
    const QString body = functionBody(src, QStringLiteral("void Positioner::updatePosition"));
    QVERIFY2(!body.isEmpty(), "a signature with a parameter list must still yield its body");
    QVERIFY(body.startsWith(QLatin1Char('{')));
    QVERIFY(body.endsWith(QLatin1Char('}')));
    QVERIFY(body.contains(QStringLiteral("m_pos = p;")));
}

void SourceReaderTest::functionBodyKeepsNestedBraces()
{
    const QString src = QStringLiteral("bool f()\n{\n    if (a) {\n        return true;\n    }\n    return false;\n}\ntrailing");
    const QString body = functionBody(src, QStringLiteral("bool f()"));
    QCOMPARE(body.count(QLatin1Char('{')), 2);
    QCOMPARE(body.count(QLatin1Char('}')), 2);
    QVERIFY(body.contains(QStringLiteral("return false;")));
    QVERIFY2(!body.contains(QStringLiteral("trailing")), "the body must stop at its own closing brace");
}

void SourceReaderTest::functionBodyOfAnAbsentSignatureIsEmpty()
{
    const QString src = QStringLiteral("void g()\n{\n}\n");
    QVERIFY(functionBody(src, QStringLiteral("void neverDeclared")).isEmpty());
    QVERIFY2(functionBody(QStringLiteral("void h();"), QStringLiteral("void h")).isEmpty(),
             "a declaration with no body has no block to return");
}

void SourceReaderTest::bracedBlockAfterStartsAtTheNextBrace()
{
    const QString src = QStringLiteral("A { one } B { two }");
    QCOMPARE(bracedBlockAfter(src, 0), QStringLiteral("{ one }"));
    QCOMPARE(bracedBlockAfter(src, src.indexOf(QStringLiteral("B"))), QStringLiteral("{ two }"));
    QVERIFY(bracedBlockAfter(src, src.size()).isEmpty());
}

//! The QML side does want the guard: `Binding.RestoreNone` and `Bindings { }` are not Binding
//! elements, and counting them as such is how the restoreMode sweep would fake its own coverage.
void SourceReaderTest::elementBlocksRejectsAttachedReferencesAndLongerNames()
{
    const QString src = QStringLiteral(
        "Item {\n"
        "    property int mode: Binding.RestoreNone\n"
        "    Bindings { junk: 1 }\n"
        "    Binding { target: a; when: b }\n"
        "}\n");

    const QList<QString> blocks = elementBlocks(src, QStringLiteral("Binding"));
    QCOMPARE(blocks.size(), 1);
    QVERIFY(blocks.first().contains(QStringLiteral("target: a")));
    QVERIFY2(!blocks.first().contains(QStringLiteral("junk")), "Bindings is not a Binding");
}

void SourceReaderTest::elementBlocksReturnsEveryBlockInOrder()
{
    const QString src = QStringLiteral(
        "Binding { id: first; Binding { id: nested } }\n"
        "Binding\n{\n    id: second\n}\n");

    const QList<QString> blocks = elementBlocks(src, QStringLiteral("Binding"));
    QCOMPARE(blocks.size(), 2);
    QVERIFY2(blocks.at(0).contains(QStringLiteral("id: nested")),
             "an outer block must carry its nested children, not stop at their brace");
    QVERIFY(blocks.at(1).contains(QStringLiteral("id: second")));
    QVERIFY2(!blocks.at(1).contains(QStringLiteral("first")), "the blocks must come back in source order");
}

void SourceReaderTest::repoPathResolvesAgainstTheCheckout()
{
    const QString path = repoPath(QStringLiteral("tests/sourcereadertest.cpp"));
    QVERIFY2(QFileInfo::exists(path), qPrintable(QStringLiteral("REPO_ROOT does not point at the checkout: %1").arg(path)));
    QVERIFY(readRepoFile(QStringLiteral("tests/sourcereadertest.cpp")).contains(QStringLiteral("repoPathResolvesAgainstTheCheckout")));
    QCOMPARE(readFile(path), readRepoFile(QStringLiteral("tests/sourcereadertest.cpp")));
}

//! panelbackgroundtest used to fail loudly on an unreadable file; it now relies on this contract
//! plus its own isEmpty check, so the contract is worth pinning.
void SourceReaderTest::readRepoFileOfAnAbsentPathIsEmpty()
{
    QVERIFY(readRepoFile(QStringLiteral("no/such/file.cpp")).isEmpty());
    QVERIFY(readFile(QStringLiteral("/no/such/absolute/file.cpp")).isEmpty());
}

void SourceReaderTest::strippedCollapsesWhitespaceAcrossNewlines()
{
    QCOMPARE(stripped(QStringLiteral("{\n    if (a\n        && b) {\n    }\n}")), QStringLiteral("{if(a&&b){}}"));
}

QTEST_MAIN(SourceReaderTest)
#include "sourcereadertest.moc"
