/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-link test for the KDirWatch bookkeeping in app/plasma/extended/theme.cpp.
//
// When the current Plasma theme ships no colors file of its own, Theme falls back to
// the KDE color scheme and registers <config>/kdeglobals with the global KDirWatch so
// it can follow scheme changes. Theme::load() runs again on every theme switch, so that
// registration has to be released before the next one is taken and again on destruction
// -- KDirWatch refcounts per instance, and KDirWatch::self() is process-wide, so an
// unbalanced addFile keeps the entry (and its inotify watch) alive for the whole session.
//
// theme.cpp reaches lattecorona.h through its includes, so this drives the real compiled
// object out of lattedock-objs rather than recompiling the source here.

#include "plasma/extended/theme.h"

// Qt
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

// KDE
#include <KDirWatch>
#include <KSharedConfig>

// KSvg
#include <KSvg/ImageSet>

using namespace Latte::PlasmaExtended;

class PlasmaThemeWatchTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();

    void reloadingTheThemeDoesNotStackRegistrations();
    void destructionReleasesTheRegistration();
    void aThemeShippingItsOwnColorsReleasesTheRegistration();

private:
    Theme *buildTheme();
    bool writeFile(const QString &path, const QString &body);

    //! true while KDirWatch holds an entry for the fallback settings file
    bool kdeGlobalsIsWatched() const;

    QTemporaryDir m_dir;
    //! <config home>/kdeglobals -- the file Theme falls back to watching
    QString m_kdeGlobals;
    //! <data home>/plasma/desktoptheme/<current image set>
    QString m_themeDir;
    //! the colors file inside m_themeDir whose presence flips the branch
    QString m_themeColors;
    KSharedConfig::Ptr m_config;
};

bool PlasmaThemeWatchTest::writeFile(const QString &path, const QString &body)
{
    QFile f(path);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&f);
    out << body;
    f.close();

    return true;
}

bool PlasmaThemeWatchTest::kdeGlobalsIsWatched() const
{
    return KDirWatch::exists() && KDirWatch::self()->contains(m_kdeGlobals);
}

Theme *PlasmaThemeWatchTest::buildTheme()
{
    //! Theme only qobject_casts its parent to a Corona and never dereferences the
    //! result, so a parentless instance drives the real code unchanged.
    return new Theme(m_config, nullptr);
}

void PlasmaThemeWatchTest::initTestCase()
{
    QVERIFY(m_dir.isValid());

    //! Latte::configPath() reads QStandardPaths::ConfigLocation and Importer::standardPath()
    //! reads GenericDataLocation, both re-derived from the environment on every call.
    const QString configHome = m_dir.filePath(QStringLiteral("xdg-config-home"));
    const QString dataHome = m_dir.filePath(QStringLiteral("xdg-data-home"));
    QVERIFY(QDir().mkpath(configHome));
    QVERIFY(QDir().mkpath(dataHome));
    qputenv("XDG_CONFIG_HOME", configHome.toLocal8Bit());
    qputenv("XDG_DATA_HOME", dataHome.toLocal8Bit());

    //! SchemeColors::possibleSchemeFile("kdeglobals") only resolves to kdeglobals itself
    //! when the file carries a WM group, which is the modern accent-colour layout.
    m_kdeGlobals = configHome + QStringLiteral("/kdeglobals");
    QVERIFY(writeFile(m_kdeGlobals, QStringLiteral(
        "[General]\n"
        "ColorScheme=WatchTest\n"
        "\n"
        "[WM]\n"
        "activeBackground=10,20,30\n"
        "activeForeground=200,210,220\n"
        "\n"
        "[Colors:Window]\n"
        "BackgroundNormal=10,20,30\n"
        "ForegroundNormal=200,210,220\n")));

    //! Theme resolves its theme dir through the image set name, and XDG_DATA_HOME sorts
    //! ahead of the system data dirs, so a dir planted here shadows the installed theme
    //! and hands the test control over whether a colors file is present.
    const QString imageSetName = KSvg::ImageSet().imageSetName();
    QVERIFY(!imageSetName.isEmpty());

    m_themeDir = dataHome + QStringLiteral("/plasma/desktoptheme/") + imageSetName;
    QVERIFY(QDir().mkpath(m_themeDir));
    m_themeColors = m_themeDir + QStringLiteral("/colors");

    m_config = KSharedConfig::openConfig(m_dir.filePath(QStringLiteral("lattedockrc")));
    QVERIFY(m_config);
}

void PlasmaThemeWatchTest::init()
{
    //! every test starts from the fallback branch: no colors file in the theme dir
    QFile::remove(m_themeColors);
    QVERIFY(!QFile::exists(m_themeColors));
}

void PlasmaThemeWatchTest::reloadingTheThemeDoesNotStackRegistrations()
{
    {
        QScopedPointer<Theme> theme(buildTheme());

        theme->load();
        QVERIFY(kdeGlobalsIsWatched());

        //! each reload is one theme switch
        for (int i = 0; i < 4; ++i) {
            theme->load();
        }
    }

    //! the destructor releases one registration, so the entry can only be gone if the
    //! reloads released theirs as they went
    QVERIFY(!kdeGlobalsIsWatched());
}

void PlasmaThemeWatchTest::destructionReleasesTheRegistration()
{
    {
        QScopedPointer<Theme> theme(buildTheme());

        theme->load();
        QVERIFY(kdeGlobalsIsWatched());
    }

    QVERIFY(!kdeGlobalsIsWatched());
}

void PlasmaThemeWatchTest::aThemeShippingItsOwnColorsReleasesTheRegistration()
{
    QScopedPointer<Theme> theme(buildTheme());

    theme->load();
    QVERIFY(kdeGlobalsIsWatched());

    //! switching to a theme that carries its own colors takes the other branch, which
    //! registers nothing -- the fallback registration still has to go
    QVERIFY(writeFile(m_themeColors, QStringLiteral(
        "[General]\n"
        "Name=Latte Watch Test\n"
        "\n"
        "[WM]\n"
        "activeBackground=30,40,50\n"
        "activeForeground=220,230,240\n"
        "\n"
        "[Colors:Window]\n"
        "BackgroundNormal=30,40,50\n"
        "ForegroundNormal=220,230,240\n")));

    theme->load();
    QVERIFY(!kdeGlobalsIsWatched());
}

QTEST_MAIN(PlasmaThemeWatchTest)
#include "plasmathemewatchtest.moc"
