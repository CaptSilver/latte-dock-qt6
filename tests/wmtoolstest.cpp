/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-link unit tests for the two free-standing window-system helpers:
//   app/wm/tasktools.cpp   - appDataFromUrl()/defaultApplication() URL parsing
//   app/wm/schemecolors.cpp - SchemeColors color-scheme .colors parsing
// Both are linked through the prebuilt latte-dock application objects, so these
// drive the genuine compiled functions, not a reimplementation.

#include "wm/tasktools.h"
#include "wm/schemecolors.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QtTest>

using namespace Latte::WindowSystem;

class WmToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    // tasktools - defaultApplication
    void defaultApplication_nonPreferredScheme_returnsEmpty();
    void defaultApplication_preferredEmptyHost_returnsEmpty();

    // tasktools - appDataFromUrl
    void appDataFromUrl_keepsUrl();
    void appDataFromUrl_skipTaskbarTrue();
    void appDataFromUrl_skipTaskbarFalse();
    void appDataFromUrl_skipTaskbarAbsentDefaultsFalse();
    void appDataFromUrl_nameFallsBackToFileName();
    void appDataFromUrl_localDesktopFileReadsName();
    void appDataFromUrl_preferredEmptyHostHasNoId();

    // schemecolors
    void schemeColors_parsesWmAndSelectionColors();
    void schemeColors_plasmaThemeReadsWindowGroup();
    void schemeColors_schemeNameFromGeneralGroup();
    void schemeColors_missingFileYieldsEmptyFileAndInvalidColors();
    void schemeColors_possibleSchemeFileAcceptsAbsoluteColors();

private:
    QString writeColorsFile(const QString &name, const QString &body);

    QTemporaryDir m_dir;
};

QString WmToolsTest::writeColorsFile(const QString &name, const QString &body)
{
    const QString path = m_dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream out(&f);
    out << body;
    f.close();
    return path;
}

void WmToolsTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void WmToolsTest::defaultApplication_nonPreferredScheme_returnsEmpty()
{
    // Only preferred:// is handled; anything else short-circuits to an empty id.
    QCOMPARE(defaultApplication(QUrl(QStringLiteral("applications:firefox.desktop"))), QString());
    QCOMPARE(defaultApplication(QUrl(QStringLiteral("file:///usr/share/applications/x.desktop"))), QString());
}

void WmToolsTest::defaultApplication_preferredEmptyHost_returnsEmpty()
{
    // preferred:// with no host component (empty application) returns an empty id.
    QCOMPARE(defaultApplication(QUrl(QStringLiteral("preferred://"))), QString());
}

void WmToolsTest::appDataFromUrl_keepsUrl()
{
    const QUrl url(QStringLiteral("file:///tmp/does-not-exist"));
    const AppData data = appDataFromUrl(url);
    // For a plain non-desktop local file none of the resolvers fire, so url is untouched.
    QCOMPARE(data.url, url);
    QVERIFY(!data.skipTaskbar);
}

void WmToolsTest::appDataFromUrl_skipTaskbarTrue()
{
    const AppData data = appDataFromUrl(QUrl(QStringLiteral("file:///tmp/thing?skipTaskbar=true")));
    QVERIFY(data.skipTaskbar);
}

void WmToolsTest::appDataFromUrl_skipTaskbarFalse()
{
    const AppData data = appDataFromUrl(QUrl(QStringLiteral("file:///tmp/thing?skipTaskbar=false")));
    QVERIFY(!data.skipTaskbar);
}

void WmToolsTest::appDataFromUrl_skipTaskbarAbsentDefaultsFalse()
{
    const AppData data = appDataFromUrl(QUrl(QStringLiteral("file:///tmp/thing?other=1")));
    QVERIFY(!data.skipTaskbar);
}

void WmToolsTest::appDataFromUrl_nameFallsBackToFileName()
{
    // No service DB match, no readable desktop file: name defaults to the URL file name.
    const AppData data = appDataFromUrl(QUrl(QStringLiteral("file:///tmp/myfile.bin")));
    QCOMPARE(data.name, QStringLiteral("myfile.bin"));
}

void WmToolsTest::appDataFromUrl_localDesktopFileReadsName()
{
    // A .desktop file that is not registered as a KService falls through to the
    // KDesktopFile branch, which reads Name/GenericName/Icon directly and derives
    // the id from the file name (with the .desktop suffix stripped). TryExec must
    // point at a real executable for the read to happen.
    const QString shell = QStandardPaths::findExecutable(QStringLiteral("sh"));
    QVERIFY2(!shell.isEmpty(), "no /bin/sh available to use as TryExec");

    const QString body = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Latte WmTools Probe\n"
        "GenericName=Probe Generic\n"
        "Icon=utilities-terminal\n"
        "Exec=%1\n"
        "TryExec=%1\n").arg(shell);

    const QString path = m_dir.filePath(QStringLiteral("lattewmtoolsprobe.desktop"));
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream(&f) << body;
    }

    const AppData data = appDataFromUrl(QUrl::fromLocalFile(path));

    QCOMPARE(data.name, QStringLiteral("Latte WmTools Probe"));
    QCOMPARE(data.genericName, QStringLiteral("Probe Generic"));
    // id is the file name with the .desktop extension chopped off.
    QCOMPARE(data.id, QStringLiteral("lattewmtoolsprobe"));
    QVERIFY(!data.icon.isNull());
}

void WmToolsTest::appDataFromUrl_preferredEmptyHostHasNoId()
{
    // preferred:// with empty host: defaultApplication returns empty, no service
    // resolves, so id stays empty and name falls back to the (empty) file name.
    const AppData data = appDataFromUrl(QUrl(QStringLiteral("preferred://")));
    QVERIFY(data.id.isEmpty());
}

void WmToolsTest::schemeColors_parsesWmAndSelectionColors()
{
    const QString body = QStringLiteral(
        "[General]\n"
        "Name=WmToolsScheme\n"
        "\n"
        "[WM]\n"
        "activeBackground=10,20,30\n"
        "activeForeground=40,50,60\n"
        "inactiveBackground=70,80,90\n"
        "inactiveForeground=100,110,120\n"
        "\n"
        "[Colors:Selection]\n"
        "BackgroundNormal=200,0,0\n"
        "ForegroundNormal=0,200,0\n"
        "\n"
        "[Colors:Window]\n"
        "ForegroundPositive=1,2,3\n"
        "ForegroundNeutral=4,5,6\n"
        "ForegroundNegative=7,8,9\n"
        "\n"
        "[Colors:Button]\n"
        "ForegroundNormal=11,12,13\n"
        "BackgroundNormal=14,15,16\n"
        "DecorationHover=17,18,19\n"
        "DecorationFocus=20,21,22\n");

    const QString path = writeColorsFile(QStringLiteral("WmToolsScheme.colors"), body);
    QVERIFY(!path.isEmpty());

    SchemeColors scheme(nullptr, path, /*plasmaTheme*/ false);

    // Non-plasma path reads the [WM] group for the active/inactive pairs.
    QCOMPARE(scheme.backgroundColor(), QColor(10, 20, 30));
    QCOMPARE(scheme.textColor(), QColor(40, 50, 60));
    QCOMPARE(scheme.inactiveBackgroundColor(), QColor(70, 80, 90));
    QCOMPARE(scheme.inactiveTextColor(), QColor(100, 110, 120));

    // Selection group drives highlight colors.
    QCOMPARE(scheme.highlightColor(), QColor(200, 0, 0));
    QCOMPARE(scheme.highlightedTextColor(), QColor(0, 200, 0));

    // Window group drives the positive/neutral/negative trio.
    QCOMPARE(scheme.positiveTextColor(), QColor(1, 2, 3));
    QCOMPARE(scheme.neutralTextColor(), QColor(4, 5, 6));
    QCOMPARE(scheme.negativeTextColor(), QColor(7, 8, 9));

    // Button group drives the button colors.
    QCOMPARE(scheme.buttonTextColor(), QColor(11, 12, 13));
    QCOMPARE(scheme.buttonBackgroundColor(), QColor(14, 15, 16));
    QCOMPARE(scheme.buttonHoverColor(), QColor(17, 18, 19));
    QCOMPARE(scheme.buttonFocusColor(), QColor(20, 21, 22));

    QCOMPARE(scheme.schemeFile(), path);
    QCOMPARE(scheme.schemeName(), QStringLiteral("WmToolsScheme"));
}

void WmToolsTest::schemeColors_plasmaThemeReadsWindowGroup()
{
    // With plasmaTheme=true the active/inactive pairs come from [Colors:Window]
    // (BackgroundNormal/ForegroundNormal/BackgroundAlternate/ForegroundInactive)
    // rather than the [WM] group, so the [WM] values must be ignored.
    const QString body = QStringLiteral(
        "[General]\n"
        "Name=PlasmaProbe\n"
        "\n"
        "[WM]\n"
        "activeBackground=99,99,99\n"
        "\n"
        "[Colors:Window]\n"
        "BackgroundNormal=30,30,30\n"
        "ForegroundNormal=40,40,40\n"
        "BackgroundAlternate=50,50,50\n"
        "ForegroundInactive=60,60,60\n");

    const QString path = writeColorsFile(QStringLiteral("PlasmaProbe.colors"), body);
    QVERIFY(!path.isEmpty());

    SchemeColors scheme(nullptr, path, /*plasmaTheme*/ true);

    QCOMPARE(scheme.backgroundColor(), QColor(30, 30, 30));
    QCOMPARE(scheme.textColor(), QColor(40, 40, 40));
    QCOMPARE(scheme.inactiveBackgroundColor(), QColor(50, 50, 50));
    QCOMPARE(scheme.inactiveTextColor(), QColor(60, 60, 60));
}

void WmToolsTest::schemeColors_schemeNameFromGeneralGroup()
{
    // The static schemeName() reads [General]/Name from an absolute .colors path.
    const QString body = QStringLiteral(
        "[General]\n"
        "Name=Custom Display Name\n");
    const QString path = writeColorsFile(QStringLiteral("namedscheme.colors"), body);
    QVERIFY(!path.isEmpty());

    QCOMPARE(SchemeColors::schemeName(path), QStringLiteral("Custom Display Name"));

    // With no [General]/Name it falls back to the file base name sans extension.
    const QString path2 = writeColorsFile(QStringLiteral("noname.colors"), QStringLiteral("[WM]\nactiveBackground=1,1,1\n"));
    QVERIFY(!path2.isEmpty());
    QCOMPARE(SchemeColors::schemeName(path2), QStringLiteral("noname"));
}

void WmToolsTest::schemeColors_missingFileYieldsEmptyFileAndInvalidColors()
{
    // A scheme that resolves to no file leaves schemeFile empty and the colors
    // default-constructed (invalid).
    SchemeColors scheme(nullptr, m_dir.filePath(QStringLiteral("absent.colors")), false);
    QVERIFY(scheme.schemeFile().isEmpty());
    QVERIFY(!scheme.backgroundColor().isValid());
    QVERIFY(!scheme.textColor().isValid());
}

void WmToolsTest::schemeColors_possibleSchemeFileAcceptsAbsoluteColors()
{
    // An absolute path ending in .colors that exists is returned verbatim.
    const QString path = writeColorsFile(QStringLiteral("Absolute.colors"), QStringLiteral("[General]\nName=Abs\n"));
    QVERIFY(!path.isEmpty());
    QCOMPARE(SchemeColors::possibleSchemeFile(path), path);

    // A non-existent absolute .colors path resolves to nothing.
    QVERIFY(SchemeColors::possibleSchemeFile(m_dir.filePath(QStringLiteral("Nope.colors"))).isEmpty());
}

QTEST_MAIN(WmToolsTest)
#include "wmtoolstest.moc"
