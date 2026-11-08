/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// QmlObjectLoader instantiates a QML file in its own context on a shared engine and keeps
// the root object alive for as long as the loader lives. It stands in for the loading
// libplasma used to do for the indicator config pages and the widget-alternatives dialog,
// so these tests pin down the parts those callers rely on: context properties set before the
// file is created, the localization context, initial properties, ownership, and a loud
// failure for a file that does not compile.

#include "qmlobjectloader.h"

#include <QDir>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

#include <KLocalizedQmlContext>
#include <PlasmaQuick/PlasmaQuick>

using Latte::QmlObjectLoader;

class QmlObjectLoaderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void loadsAFileAndExposesItsRootObject();
    void contextPropertiesSetBeforeLoadAreVisibleToBindings();
    void initialPropertiesAreApplied();
    void localizationContextIsInstalled();
    void translationDomainReachesTheLocalizationContext();
    void defaultConstructorUsesThePlasmaQuickEngine();
    void deleteLaterWhenHiddenDeletesTheLoaderOnceTheRootIsHidden();
    void deleteLaterWhenHiddenKeepsTheLoaderWhileTheRootIsShown();
    void deleteLaterWhenHiddenNeedsARootThatCanBeHidden();
    void windowRootsAreSupported();
    void deletingTheLoaderDeletesTheRootObject();
    void loadingAgainReplacesTheRootObject();
    void aFileThatDoesNotCompileFailsLoudly();
    void aMissingFileFailsLoudly();
    void rootObjectIsNullBeforeLoad();

private:
    QUrl write(const QString &name, const QByteArray &qml);

    QTemporaryDir m_dir;
    int m_written{0};
    std::shared_ptr<QQmlEngine> m_engine;
};

void QmlObjectLoaderTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_engine = std::make_shared<QQmlEngine>();
}

QUrl QmlObjectLoaderTest::write(const QString &name, const QByteArray &qml)
{
    //! The engine caches directory listings to check file name case, so a file added to a
    //! directory it already listed reads as a case mismatch. One directory per file.
    QDir().mkpath(m_dir.filePath(QString::number(++m_written)));
    const QString path = m_dir.filePath(QString::number(m_written) + QLatin1Char('/') + name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qFatal("cannot write %s", qPrintable(path));
    }
    f.write(qml);
    f.close();
    return QUrl::fromLocalFile(path);
}

void QmlObjectLoaderTest::loadsAFileAndExposesItsRootObject()
{
    const QUrl url = write(QStringLiteral("item.qml"), "import QtQuick\nItem { objectName: \"hello\"; width: 40 }");

    QmlObjectLoader loader(m_engine);
    QVERIFY(loader.load(url));

    QVERIFY(loader.rootObject());
    QCOMPARE(loader.rootObject()->objectName(), QStringLiteral("hello"));
    QCOMPARE(loader.rootObject()->property("width").toInt(), 40);
}

void QmlObjectLoaderTest::contextPropertiesSetBeforeLoadAreVisibleToBindings()
{
    //! The callers hand the QML its helper object through a context property, and the file
    //! reads it while it is being created.
    const QUrl url = write(QStringLiteral("ctx.qml"), "import QtQuick\nItem { property string seen: helperName }");

    QmlObjectLoader loader(m_engine);
    loader.rootContext()->setContextProperty(QStringLiteral("helperName"), QStringLiteral("alternatives"));
    QVERIFY(loader.load(url));

    QCOMPARE(loader.rootObject()->property("seen").toString(), QStringLiteral("alternatives"));
}

void QmlObjectLoaderTest::initialPropertiesAreApplied()
{
    const QUrl url = write(QStringLiteral("props.qml"),
                           "import QtQuick\nItem {\n"
                           "  property string label\n"
                           "  property string atCompletion\n"
                           "  Component.onCompleted: atCompletion = label\n"
                           "}");

    QmlObjectLoader loader(m_engine);
    QVERIFY(loader.load(url, {{QStringLiteral("label"), QStringLiteral("set from C++")}}));

    QCOMPARE(loader.rootObject()->property("label").toString(), QStringLiteral("set from C++"));
    //! applied before completeCreate, so the file's own startup code sees the value
    QCOMPARE(loader.rootObject()->property("atCompletion").toString(), QStringLiteral("set from C++"));
}

void QmlObjectLoaderTest::localizationContextIsInstalled()
{
    //! i18n() only exists when a KLocalizedContext is the context object; without it the
    //! binding below is a ReferenceError and the property stays empty.
    const QUrl url = write(QStringLiteral("i18n.qml"), "import QtQuick\nItem { property string text: i18n(\"Hello\") }");

    QmlObjectLoader loader(m_engine);
    loader.setTranslationDomain(QStringLiteral("latte_indicator_test"));
    QVERIFY(loader.load(url));

    QCOMPARE(loader.rootObject()->property("text").toString(), QStringLiteral("Hello"));
}

void QmlObjectLoaderTest::translationDomainReachesTheLocalizationContext()
{
    //! The domain is the reason IndicatorUiManager sets it at all; i18n("Hello") alone can not
    //! tell a domain that was applied from one that was dropped.
    QmlObjectLoader loader(m_engine);
    loader.setTranslationDomain(QStringLiteral("latte_indicator_test"));

    auto *localization = qobject_cast<KLocalizedQmlContext *>(loader.rootContext()->contextObject());
    QVERIFY(localization);
    QCOMPARE(localization->translationDomain(), QStringLiteral("latte_indicator_test"));
}

void QmlObjectLoaderTest::defaultConstructorUsesThePlasmaQuickEngine()
{
    //! the engine both production callers get
    QmlObjectLoader loader;
    QVERIFY(loader.engine());
    QCOMPARE(loader.engine(), PlasmaQuick::globalEngine().get());
}

void QmlObjectLoaderTest::deleteLaterWhenHiddenDeletesTheLoaderOnceTheRootIsHidden()
{
    const QUrl url = write(QStringLiteral("hidden.qml"), "import QtQuick\nimport QtQuick.Window\nWindow { visible: false }");

    QPointer<QmlObjectLoader> loader = new QmlObjectLoader(m_engine);
    QVERIFY(loader->load(url));
    QVERIFY(loader->deleteLaterWhenHidden());

    loader->rootObject()->setProperty("visible", true);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(loader);

    QPointer<QObject> root = loader->rootObject();
    root->setProperty("visible", false);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    QVERIFY(!loader);
    QVERIFY(!root);
}

void QmlObjectLoaderTest::deleteLaterWhenHiddenKeepsTheLoaderWhileTheRootIsShown()
{
    const QUrl url = write(QStringLiteral("shown.qml"), "import QtQuick\nimport QtQuick.Window\nWindow { visible: false }");

    QmlObjectLoader loader(m_engine);
    QVERIFY(loader.load(url));
    QVERIFY(loader.deleteLaterWhenHidden());

    //! showing the root is not a reason to go away
    loader.rootObject()->setProperty("visible", true);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(loader.rootObject());
}

void QmlObjectLoaderTest::deleteLaterWhenHiddenNeedsARootThatCanBeHidden()
{
    QmlObjectLoader empty(m_engine);
    QVERIFY(!empty.deleteLaterWhenHidden());

    const QUrl url = write(QStringLiteral("plainobject.qml"), "import QtQml\nQtObject { }");
    QmlObjectLoader loader(m_engine);
    QVERIFY(loader.load(url));
    QVERIFY(!loader.deleteLaterWhenHidden());
}

void QmlObjectLoaderTest::windowRootsAreSupported()
{
    //! The widget-alternatives dialog is a window, not an item.
    const QUrl url = write(QStringLiteral("window.qml"), "import QtQuick\nimport QtQuick.Window\nWindow { visible: false }");

    QmlObjectLoader loader(m_engine);
    QVERIFY(loader.load(url));

    QVERIFY(qobject_cast<QQuickWindow *>(loader.rootObject()));
}

void QmlObjectLoaderTest::deletingTheLoaderDeletesTheRootObject()
{
    const QUrl url = write(QStringLiteral("owned.qml"), "import QtQuick\nItem {}");

    QPointer<QObject> root;
    {
        QmlObjectLoader loader(m_engine);
        QVERIFY(loader.load(url));
        root = loader.rootObject();
        QVERIFY(root);
    }

    QVERIFY(!root);
}

void QmlObjectLoaderTest::loadingAgainReplacesTheRootObject()
{
    const QUrl first = write(QStringLiteral("first.qml"), "import QtQuick\nItem { objectName: \"first\" }");
    const QUrl second = write(QStringLiteral("second.qml"), "import QtQuick\nItem { objectName: \"second\" }");

    QmlObjectLoader loader(m_engine);
    QVERIFY(loader.load(first));
    QPointer<QObject> firstRoot = loader.rootObject();

    QVERIFY(loader.load(second));

    QVERIFY(!firstRoot);
    QCOMPARE(loader.rootObject()->objectName(), QStringLiteral("second"));
}

void QmlObjectLoaderTest::aFileThatDoesNotCompileFailsLoudly()
{
    const QUrl url = write(QStringLiteral("broken.qml"), "import QtQuick\nItem { this is not qml }");

    QmlObjectLoader loader(m_engine);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("QmlObjectLoader: .*broken\\.qml")));

    QVERIFY(!loader.load(url));
    QVERIFY(!loader.rootObject());
}

void QmlObjectLoaderTest::aMissingFileFailsLoudly()
{
    const QUrl url = QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("nope.qml")));

    QmlObjectLoader loader(m_engine);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("QmlObjectLoader: .*nope\\.qml")));

    QVERIFY(!loader.load(url));
    QVERIFY(!loader.rootObject());
}

void QmlObjectLoaderTest::rootObjectIsNullBeforeLoad()
{
    QmlObjectLoader loader(m_engine);
    QVERIFY(!loader.rootObject());
    QVERIFY(loader.rootContext());
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QmlObjectLoaderTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "qmlobjectloadertest.moc"
