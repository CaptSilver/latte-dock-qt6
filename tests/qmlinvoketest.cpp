/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Every C++ path that calls into QML used to spell the same four steps by hand: ask the
// metaobject for a signature, bail on -1, fetch the QMetaMethod, invoke. They share
// Latte::invokeIfPresent() now, and two of its properties are contracts rather than
// conveniences:
//   * it stays SILENT on a miss. ContainmentInterface probes every child item of every
//     applet on every shortcut press, so a warning inside the helper would fire dozens of
//     times per keystroke. Callers that do want a diagnostic log it on a false return.
//   * it forwards Q_ARG straight through and never stores it. Q_ARG keeps only a pointer
//     to a temporary that dies at the end of the caller's full-expression, so a helper
//     that held the pack or deferred delivery would read freed memory.
// The QML case pins the naming rule that every signature string in the tree depends on:
// an untyped `function f(a, b)` declared in QML is "f(QVariant,QVariant)" to the metaobject.

#include "tools/qmlinvoke.h"

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopedPointer>
#include <QString>
#include <QVariant>
#include <QLoggingCategory>
#include <QtTest>

namespace {

int s_messageCount = 0;

void countingMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type)
    Q_UNUSED(context)
    Q_UNUSED(msg)
    ++s_messageCount;
}

}

class InvokeTarget : public QObject
{
    Q_OBJECT

public:
    QString firstArg;
    QString secondArg;
    int records{0};
    int pings{0};

    Q_INVOKABLE void record(QVariant first, QVariant second)
    {
        firstArg = first.toString();
        secondArg = second.toString();
        ++records;
    }

    Q_INVOKABLE void ping()
    {
        ++pings;
    }
};

class QmlInvokeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void presentSignatureForwardsArgumentValues();
    void missingSignatureStaysSilent();
    void arityMismatchDoesNotInvoke();
    void zeroArgumentSignatureInvokes();
    void nullTargetReturnsFalse();
    void qmlFunctionIsReachableAsQVariantSignature();
};

void QmlInvokeTest::presentSignatureForwardsArgumentValues()
{
    InvokeTarget target;

    // The concatenation forces Q_ARG to materialise a QVariant temporary around a QString
    // rvalue and keep only its address; reading the value back proves it outlived the
    // forward into the helper.
    QVERIFY(Latte::invokeIfPresent(&target, "record(QVariant,QVariant)",
                                   Q_ARG(QVariant, QString(QStringLiteral("alpha") + QString::number(1))),
                                   Q_ARG(QVariant, QStringLiteral("beta"))));

    QCOMPARE(target.records, 1);
    QCOMPARE(target.firstArg, QStringLiteral("alpha1"));
    QCOMPARE(target.secondArg, QStringLiteral("beta"));
}

void QmlInvokeTest::missingSignatureStaysSilent()
{
    InvokeTarget target;

    //! this distro's qtlogging.ini sets *.debug=false, and a filtered qDebug never reaches an
    //! installed handler -- so without opening the category the counter can only ever read 0
    QLoggingCategory::setFilterRules(QStringLiteral("default.debug=true"));

    s_messageCount = 0;
    QtMessageHandler previous = qInstallMessageHandler(countingMessageHandler);
    const bool invoked = Latte::invokeIfPresent(&target, "absent(QVariant)", Q_ARG(QVariant, QStringLiteral("x")));
    qInstallMessageHandler(previous);

    QLoggingCategory::setFilterRules(QString());

    QVERIFY(!invoked);
    QCOMPARE(target.records, 0);
    QCOMPARE(s_messageCount, 0);
}

void QmlInvokeTest::arityMismatchDoesNotInvoke()
{
    InvokeTarget target;

    // A signature is matched whole, so a wrong argument count is just a miss - never a
    // half-filled call.
    QVERIFY(!Latte::invokeIfPresent(&target, "record(QVariant)", Q_ARG(QVariant, QStringLiteral("only"))));
    QCOMPARE(target.records, 0);
}

void QmlInvokeTest::zeroArgumentSignatureInvokes()
{
    InvokeTarget target;

    QVERIFY(Latte::invokeIfPresent(&target, "ping()"));
    QCOMPARE(target.pings, 1);
}

void QmlInvokeTest::nullTargetReturnsFalse()
{
    QVERIFY(!Latte::invokeIfPresent(nullptr, "ping()"));
}

void QmlInvokeTest::qmlFunctionIsReachableAsQVariantSignature()
{
    static const char source[] = R"(
import QtQml

QtObject {
    property string seen

    function record(a, b) { seen = a + "|" + b }
}
)";

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(source, QUrl());

    QScopedPointer<QObject> object(component.create());
    QVERIFY2(!object.isNull(), qPrintable(component.errorString()));

    QVERIFY(Latte::invokeIfPresent(object.data(), "record(QVariant,QVariant)",
                                   Q_ARG(QVariant, QStringLiteral("left")),
                                   Q_ARG(QVariant, QStringLiteral("right"))));

    QCOMPARE(object->property("seen").toString(), QStringLiteral("left|right"));
}

QTEST_GUILESS_MAIN(QmlInvokeTest)

#include "qmlinvoketest.moc"
