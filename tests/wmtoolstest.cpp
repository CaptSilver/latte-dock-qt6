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

#include <KConfig>
#include <KConfigGroup>
#include <KDirWatch>
#include <KService>
#include <KSharedConfig>

#include <QBuffer>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QStandardPaths>
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
    void appDataFromUrl_iconDataQueryDecodesPixmap();

    // schemecolors
    void schemeColors_parsesWmAndSelectionColors();
    void schemeColors_plasmaThemeReadsWindowGroup();
    void schemeColors_schemeNameFromGeneralGroup();
    void schemeColors_missingFileYieldsEmptyFileAndInvalidColors();
    void schemeColors_possibleSchemeFileAcceptsAbsoluteColors();
    void schemeColors_destructorReleasesTheWatchItTook();
    void schemeColors_twoSchemesOnOneFileEachReleaseOnce();

    // tasktools - windowUrlFromMetadata (rules-config driven, no service DB needed)
    void windowUrl_nullConfigReturnsEmpty();
    void windowUrl_mappingAppIdAndClass();
    void windowUrl_mappingAppIdOnly();
    void windowUrl_manualOnlyReturnsEmpty();
    void windowUrl_appIdIsDesktopPath();
    void windowUrl_appIdPathPlusExtension();
    void windowUrl_skipTaskbarAddsQuery();
    void windowUrl_matchCommandLineFirstAppId();
    void windowUrl_matchCommandLineFirstWmClass();

    // tasktools - servicesFromCmdLine / servicesFromPid
    void servicesFromCmdLine_nullConfigEmpty();
    void servicesFromCmdLine_syntheticFromRealBinary();
    void servicesFromCmdLine_stripsArgumentsThenSynthesizes();
    void servicesFromCmdLine_tryIgnoreRuntimesRecurses();
    void servicesFromPid_zeroPidEmpty();
    void servicesFromPid_nullConfigEmpty();
    void servicesFromPid_selfPidReadsProc();

    // tasktools - defaultApplication scheme branches (isolated via test mode)
    void defaultApplication_terminalReadsConfig();
    void defaultApplication_browserStripsBang();
    void defaultApplication_browserEmptyFallsThrough();
    void defaultApplication_filemanagerNoServiceEmpty();
    void defaultApplication_mailerEmptyConfigEmpty();
    void defaultApplication_genericUnknownEmpty();

private:
    QString writeColorsFile(const QString &name, const QString &body);
    QString writeDesktopFile(const QString &name);
    KSharedConfig::Ptr rulesConfig(const QString &name);

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

QString WmToolsTest::writeDesktopFile(const QString &name)
{
    const QString path = m_dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream(&f) << QStringLiteral("[Desktop Entry]\nType=Application\nName=Probe\nExec=/bin/true\n");
    f.close();
    return path;
}

KSharedConfig::Ptr WmToolsTest::rulesConfig(const QString &name)
{
    return KSharedConfig::openConfig(m_dir.filePath(name), KConfig::SimpleConfig);
}

void WmToolsTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    // Redirect config/data/cache to a throwaway location so defaultApplication's
    // reads of the global config are deterministic and can't pollute the real one,
    // and so the service database the trader queries is empty (which the
    // windowUrlFromMetadata assertions below rely on).
    QStandardPaths::setTestModeEnabled(true);
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
                             "TryExec=%1\n")
                             .arg(shell);

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

void WmToolsTest::schemeColors_destructorReleasesTheWatchItTook()
{
    // The constructor hands the scheme file to the shared KDirWatch; without a matching
    // release the dock keeps an inotify watch on every colour scheme it ever read, long
    // after the object that cared about it is gone.
    const QString path = writeColorsFile(QStringLiteral("WatchRelease.colors"),
                                         QStringLiteral("[General]\nName=WatchRelease\n"));
    QVERIFY(!path.isEmpty());
    QVERIFY(!KDirWatch::self()->contains(path));

    {
        SchemeColors scheme(nullptr, path, /*plasmaTheme*/ false);
        QCOMPARE(scheme.schemeFile(), path);
        QVERIFY(KDirWatch::self()->contains(path));
    }

    QVERIFY(!KDirWatch::self()->contains(path));
}

void WmToolsTest::schemeColors_twoSchemesOnOneFileEachReleaseOnce()
{
    // Two live objects can share one scheme file: the extended theme builds its
    // replacement on the same path before deleting the old one, and the schemes model
    // lists the kdeglobals-resolved file alongside the same file found in a scheme
    // directory. The release must be per-object, or the first destruction pulls the
    // watch out from under the survivor.
    const QString path = writeColorsFile(QStringLiteral("SharedWatch.colors"),
                                         QStringLiteral("[General]\nName=SharedWatch\n"));
    QVERIFY(!path.isEmpty());
    QVERIFY(!KDirWatch::self()->contains(path));

    //! parented to nullptr and deleted by hand so the ordering is deterministic
    //! without spinning an event loop
    auto *first = new SchemeColors(nullptr, path, /*plasmaTheme*/ false);
    auto *second = new SchemeColors(nullptr, path, /*plasmaTheme*/ false);
    QVERIFY(KDirWatch::self()->contains(path));

    delete first;
    QVERIFY(KDirWatch::self()->contains(path));

    delete second;
    QVERIFY(!KDirWatch::self()->contains(path));
}

void WmToolsTest::appDataFromUrl_iconDataQueryDecodesPixmap()
{
    // A base64url iconData query is decoded straight into the AppData icon.
    QImage im(4, 4, QImage::Format_ARGB32);
    im.fill(Qt::red);
    QByteArray png;
    QBuffer buf(&png);
    QVERIFY(buf.open(QIODevice::WriteOnly));
    QVERIFY(im.save(&buf, "PNG"));
    buf.close();

    QUrl url(QStringLiteral("file:///tmp/iconprobe"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("iconData"), QString::fromLatin1(png.toBase64(QByteArray::Base64UrlEncoding)));
    url.setQuery(q);

    const AppData data = appDataFromUrl(url);
    QVERIFY(!data.icon.isNull());
    QVERIFY(!data.icon.availableSizes().isEmpty());
}

void WmToolsTest::windowUrl_nullConfigReturnsEmpty()
{
    QVERIFY(windowUrlFromMetadata(QStringLiteral("anyapp"), 0, KSharedConfig::Ptr(), QStringLiteral("cls")).isEmpty());
}

void WmToolsTest::windowUrl_mappingAppIdAndClass()
{
    // A [Mapping] entry keyed "appId::wmClass" pointing at a .desktop wins outright.
    auto cfg = rulesConfig(QStringLiteral("map-ac"));
    KConfigGroup(cfg, QStringLiteral("Mapping")).writeEntry(QStringLiteral("myapp::myclass"), QStringLiteral("mapped-ac.desktop"));
    cfg->sync();

    QCOMPARE(windowUrlFromMetadata(QStringLiteral("myapp"), 0, cfg, QStringLiteral("myclass")), QUrl(QStringLiteral("mapped-ac.desktop")));
}

void WmToolsTest::windowUrl_mappingAppIdOnly()
{
    // With no wmClass the "appId::" key misses and the bare-appId mapping is used.
    auto cfg = rulesConfig(QStringLiteral("map-a"));
    KConfigGroup(cfg, QStringLiteral("Mapping")).writeEntry(QStringLiteral("myapp"), QStringLiteral("mapped-a.desktop"));
    cfg->sync();

    QCOMPARE(windowUrlFromMetadata(QStringLiteral("myapp"), 0, cfg, QString()), QUrl(QStringLiteral("mapped-a.desktop")));
}

void WmToolsTest::windowUrl_manualOnlyReturnsEmpty()
{
    // [Settings]ManualOnly listing the appId short-circuits to an empty URL.
    auto cfg = rulesConfig(QStringLiteral("manual"));
    KConfigGroup(cfg, QStringLiteral("Settings")).writeEntry(QStringLiteral("ManualOnly"), QStringList{QStringLiteral("myapp")});
    cfg->sync();

    QVERIFY(windowUrlFromMetadata(QStringLiteral("myapp"), 0, cfg, QString()).isEmpty());
}

void WmToolsTest::windowUrl_appIdIsDesktopPath()
{
    // An appId that is itself an absolute path to an existing .desktop resolves to it.
    const QString path = writeDesktopFile(QStringLiteral("directpath.desktop"));
    QVERIFY(!path.isEmpty());
    auto cfg = rulesConfig(QStringLiteral("path"));

    QCOMPARE(windowUrlFromMetadata(path, 0, cfg, QString()), QUrl::fromLocalFile(path));
}

void WmToolsTest::windowUrl_appIdPathPlusExtension()
{
    // An appId path missing the .desktop suffix still matches once the suffix is added.
    const QString full = writeDesktopFile(QStringLiteral("withext.desktop"));
    QVERIFY(!full.isEmpty());
    const QString base = full.left(full.length() - QStringLiteral(".desktop").length());
    auto cfg = rulesConfig(QStringLiteral("pathext"));

    QCOMPARE(windowUrlFromMetadata(base, 0, cfg, QString()), QUrl::fromLocalFile(full));
}

void WmToolsTest::windowUrl_skipTaskbarAddsQuery()
{
    // With no mapping and no DB match, a [Settings]SkipTaskbar entry tags the
    // (otherwise empty) URL with skipTaskbar=true.
    const QString nonsense = QStringLiteral("latteNoSuchApp9x8y7z");
    auto cfg = rulesConfig(QStringLiteral("skip"));
    KConfigGroup(cfg, QStringLiteral("Settings")).writeEntry(QStringLiteral("SkipTaskbar"), QStringList{nonsense});
    cfg->sync();

    const QUrl url = windowUrlFromMetadata(nonsense, 0, cfg, QString());
    QCOMPARE(QUrlQuery(url).queryItemValue(QStringLiteral("skipTaskbar")), QStringLiteral("true"));
}

void WmToolsTest::windowUrl_matchCommandLineFirstAppId()
{
    // MatchCommandLineFirst listing the appId forces the pid path first; with pid 0
    // that yields nothing and (triedPid set) the later pid retry is skipped.
    const QString nonsense = QStringLiteral("latteMclfNoApp123");
    auto cfg = rulesConfig(QStringLiteral("mclf"));
    KConfigGroup(cfg, QStringLiteral("Settings")).writeEntry(QStringLiteral("MatchCommandLineFirst"), QStringList{nonsense});
    cfg->sync();

    QVERIFY(windowUrlFromMetadata(nonsense, 0, cfg, QString()).isEmpty());
}

void WmToolsTest::windowUrl_matchCommandLineFirstWmClass()
{
    // The "::wmClass" form of MatchCommandLineFirst triggers the same pid-first path.
    auto cfg = rulesConfig(QStringLiteral("mclf2"));
    KConfigGroup(cfg, QStringLiteral("Settings")).writeEntry(QStringLiteral("MatchCommandLineFirst"), QStringList{QStringLiteral("::myclass")});
    cfg->sync();

    QVERIFY(windowUrlFromMetadata(QString(), 0, cfg, QStringLiteral("myclass")).isEmpty());
}

void WmToolsTest::servicesFromCmdLine_nullConfigEmpty()
{
    QVERIFY(servicesFromCmdLine(QStringLiteral("anything"), QStringLiteral("proc"), KSharedConfig::Ptr()).isEmpty());
}

void WmToolsTest::servicesFromCmdLine_syntheticFromRealBinary()
{
    // No service matches the test binary, so the from-binary synthetic KService fires.
    const QString selfBin = QCoreApplication::applicationFilePath();
    auto cfg = rulesConfig(QStringLiteral("cmd"));

    const KService::List svcs = servicesFromCmdLine(selfBin, QStringLiteral("wmtoolstest"), cfg);
    QCOMPARE(svcs.count(), 1);
    QCOMPARE(svcs.first()->exec(), selfBin);
}

void WmToolsTest::servicesFromCmdLine_stripsArgumentsThenSynthesizes()
{
    // Arguments are stripped before the executable is resolved into the synthetic.
    const QString selfBin = QCoreApplication::applicationFilePath();
    auto cfg = rulesConfig(QStringLiteral("cmd-args"));

    const KService::List svcs = servicesFromCmdLine(selfBin + QStringLiteral(" --flag value"), QStringLiteral("wmtoolstest"), cfg);
    QCOMPARE(svcs.count(), 1);
    QCOMPARE(svcs.first()->exec(), selfBin);
}

void WmToolsTest::servicesFromCmdLine_tryIgnoreRuntimesRecurses()
{
    // A runtime prefix listed in TryIgnoreRuntimes is dropped and the remainder
    // re-evaluated; the bogus remainder resolves to nothing.
    auto cfg = rulesConfig(QStringLiteral("cmd-runtime"));
    KConfigGroup(cfg, QStringLiteral("Settings")).writeEntry(QStringLiteral("TryIgnoreRuntimes"), QStringList{QStringLiteral("wine")});
    cfg->sync();

    QVERIFY(servicesFromCmdLine(QStringLiteral("wine /nonexistent/app.exe"), QStringLiteral("wine"), cfg).isEmpty());
}

void WmToolsTest::servicesFromPid_zeroPidEmpty()
{
    auto cfg = rulesConfig(QStringLiteral("pid"));
    QVERIFY(servicesFromPid(0, cfg).isEmpty());
}

void WmToolsTest::servicesFromPid_nullConfigEmpty()
{
    QVERIFY(servicesFromPid(1234, KSharedConfig::Ptr()).isEmpty());
}

void WmToolsTest::servicesFromPid_selfPidReadsProc()
{
    // Drives the real /proc/<pid>/environ read and the KProcessList fallback for a
    // live pid (this process). No BAMF hint is present, so it delegates to the
    // command-line resolver; whatever comes back must at least be well-formed.
    auto cfg = rulesConfig(QStringLiteral("pid-self"));
    const quint32 self = static_cast<quint32>(QCoreApplication::applicationPid());

    const KService::List svcs = servicesFromPid(self, cfg);
    for (const auto &service : svcs) {
        QVERIFY(service);
    }
}

void WmToolsTest::defaultApplication_terminalReadsConfig()
{
    KConfigGroup general(KSharedConfig::openConfig(), QStringLiteral("General"));
    general.writeEntry(QStringLiteral("TerminalApplication"), QStringLiteral("xterm"));
    general.sync();

    QCOMPARE(defaultApplication(QUrl(QStringLiteral("preferred://terminal"))), QStringLiteral("xterm"));
}

void WmToolsTest::defaultApplication_browserStripsBang()
{
    KConfigGroup general(KSharedConfig::openConfig(), QStringLiteral("General"));
    general.writeEntry(QStringLiteral("BrowserApplication"), QStringLiteral("!mybrowser"));
    general.sync();

    // The leading '!' (a "run this exact command" marker) is stripped from the id.
    QCOMPARE(defaultApplication(QUrl(QStringLiteral("preferred://browser"))), QStringLiteral("mybrowser"));
}

void WmToolsTest::defaultApplication_browserEmptyFallsThrough()
{
    KConfigGroup general(KSharedConfig::openConfig(), QStringLiteral("General"));
    general.deleteEntry(QStringLiteral("BrowserApplication"));
    general.sync();

    // No configured browser and an empty service DB -> empty id.
    QCOMPARE(defaultApplication(QUrl(QStringLiteral("preferred://browser"))), QString());
}

void WmToolsTest::defaultApplication_filemanagerNoServiceEmpty()
{
    // No inode/directory handler in the empty test DB -> empty id.
    QCOMPARE(defaultApplication(QUrl(QStringLiteral("preferred://filemanager"))), QString());
}

void WmToolsTest::defaultApplication_mailerEmptyConfigEmpty()
{
    // No email client configured and no kontact/kmail in the DB -> empty id.
    QCOMPARE(defaultApplication(QUrl(QStringLiteral("preferred://mailer"))), QString());
}

void WmToolsTest::defaultApplication_genericUnknownEmpty()
{
    // An unrecognized host falls to the generic trader lookup, which is empty here.
    QCOMPARE(defaultApplication(QUrl(QStringLiteral("preferred://somethingunknown"))), QString());
}

QTEST_MAIN(WmToolsTest)
#include "wmtoolstest.moc"
