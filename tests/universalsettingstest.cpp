/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Drives the real Latte::UniversalSettings port code (linked directly) over a
// throwaway KSharedConfig. Constructed with a plain QObject parent, the internal
// qobject_cast<Latte::Corona*> yields a null m_corona, so every corona-independent
// getter/setter is exercisable headlessly:
//   - each setter flips state, emits its change signal (QSignalSpy), and persists
//     the whole [UniversalSettings] group through saveConfig,
//   - a second instance reading the same config back reproduces every value,
//   - the enum-backed and default-valued properties load their documented defaults.
// Getters that hard-deref m_corona (splitterIconPath/trademarkPath/trademarkIconPath)
// are intentionally NOT touched; with a null corona they would crash.

#include "universalsettings.h"

#include "../app/apptypes.h"
#include "../app/coretypes.h"
#include "../app/data/preferencesdata.h"

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QObject>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

using namespace Latte;

class UniversalSettingsTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_configPath;

    KSharedConfig::Ptr freshConfig();

private Q_SLOTS:
    void initTestCase();
    void init();

    void nullCoronaOnPlainParent();
    void defaultsBeforeLoad();
    void settersEmitAndGuardNoOp();
    void roundTripThroughConfig();
    void enumAndDefaultsLoadFromEmptyConfig();
    void screenScalesRoundTrip();
    void sensitivityAlwaysHigh();
};

KSharedConfig::Ptr UniversalSettingsTest::freshConfig()
{
    // A standalone backing file, not the global config, so no real settings leak in.
    return KSharedConfig::openConfig(m_configPath, KConfig::SimpleConfig);
}

void UniversalSettingsTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_configPath = m_dir.filePath(QStringLiteral("lattedockrc"));
}

void UniversalSettingsTest::init()
{
    // Start every case from an empty config file.
    QFile::remove(m_configPath);
}

void UniversalSettingsTest::nullCoronaOnPlainParent()
{
    // The whole headless approach hinges on this: a plain QObject parent can't be
    // cast to Latte::Corona, so corona-dependent code stays untouched. If this ever
    // regressed, the corona-deref getters would no longer be the only unsafe ones.
    QObject parent;
    UniversalSettings settings(freshConfig(), &parent);

    // Reaching any non-corona getter here must not crash.
    QCOMPARE(settings.showInfoWindow(), true);
    QCOMPARE(settings.metaPressAndHoldEnabled(), true);
}

void UniversalSettingsTest::defaultsBeforeLoad()
{
    // Member initializers define the pre-load state. These come straight from the
    // class definition, independent of any config contents.
    UniversalSettings settings(freshConfig(), this);

    QCOMPARE(settings.showInfoWindow(), true);
    QCOMPARE(settings.metaPressAndHoldEnabled(), true);
    QCOMPARE(settings.isAvailableGeometryBroadcastedToPlasma(), true);
    QCOMPARE(settings.badges3DStyle(), false);
    QCOMPARE(settings.canDisableBorders(), false);
    QCOMPARE(settings.inAdvancedModeForEditSettings(), false);
    QCOMPARE(settings.inConfigureAppletsMode(), false);
    QCOMPARE(settings.version(), 1);
    QCOMPARE(settings.screenTrackerInterval(), 2500);
    QCOMPARE(settings.parabolicSpread(), Data::Preferences::PARABOLICSPREAD);
    QCOMPARE(settings.thicknessMarginInfluence(), Data::Preferences::THICKNESSMARGININFLUENCE);
    QVERIFY(settings.singleModeLayoutName().isEmpty());
}

void UniversalSettingsTest::settersEmitAndGuardNoOp()
{
    UniversalSettings settings(freshConfig(), this);

    QSignalSpy showSpy(&settings, &UniversalSettings::showInfoWindowChanged);
    QSignalSpy spreadSpy(&settings, &UniversalSettings::parabolicSpreadChanged);
    QSignalSpy advancedSpy(&settings, &UniversalSettings::inAdvancedModeForEditSettingsChanged);

    // A real change emits exactly once and updates the getter.
    settings.setShowInfoWindow(false);
    QCOMPARE(settings.showInfoWindow(), false);
    QCOMPARE(showSpy.count(), 1);

    // Setting the same value again is a guarded no-op: no extra signal.
    settings.setShowInfoWindow(false);
    QCOMPARE(showSpy.count(), 1);

    settings.setParabolicSpread(7);
    QCOMPARE(settings.parabolicSpread(), 7);
    QCOMPARE(spreadSpy.count(), 1);
    settings.setParabolicSpread(7);
    QCOMPARE(spreadSpy.count(), 1);

    settings.setInAdvancedModeForEditSettings(true);
    QCOMPARE(settings.inAdvancedModeForEditSettings(), true);
    QCOMPARE(advancedSpy.count(), 1);
}

void UniversalSettingsTest::roundTripThroughConfig()
{
    const QStringList launchers = {QStringLiteral("applications:org.kde.dolphin.desktop"),
                                   QStringLiteral("applications:org.kde.konsole.desktop")};
    const QStringList actions = {QStringLiteral("_layouts"), QStringLiteral("_preferences")};

    // Each setter fires its change signal, which is wired to saveConfig: the whole
    // [UniversalSettings] group is rewritten and synced to disk.
    {
        UniversalSettings settings(freshConfig(), this);
        settings.setShowInfoWindow(false);
        settings.setBadges3DStyle(true);
        settings.setCanDisableBorders(true);
        settings.setInAdvancedModeForEditSettings(true);
        settings.setInConfigureAppletsMode(true);
        settings.setIsAvailableGeometryBroadcastedToPlasma(false);
        settings.setMetaPressAndHoldEnabled(false);
        settings.setVersion(7);
        settings.setScreenTrackerInterval(4000);
        settings.setParabolicSpread(9);
        settings.setThicknessMarginInfluence(0.5f);
        settings.setSingleModeLayoutName(QStringLiteral("MyLayout"));
        settings.setLaunchers(launchers);
        settings.setContextMenuActionsAlwaysShown(actions);
        settings.syncSettings();
    }

    // The values really landed under [UniversalSettings] on disk.
    {
        KConfig disk(m_configPath, KConfig::SimpleConfig);
        const KConfigGroup g = disk.group(QStringLiteral("UniversalSettings"));
        QCOMPARE(g.readEntry(QStringLiteral("version"), 1), 7);
        QCOMPARE(g.readEntry(QStringLiteral("showInfoWindow"), true), false);
        QCOMPARE(g.readEntry(QStringLiteral("badges3DStyle"), false), true);
        QCOMPARE(g.readEntry(QStringLiteral("parabolicSpread"), 0), 9);
        QCOMPARE(g.readEntry(QStringLiteral("singleModeLayoutName"), QString()), QStringLiteral("MyLayout"));
        QCOMPARE(g.readEntry(QStringLiteral("launchers"), QStringList()), launchers);
        // The thickness margin is persisted under a deliberately different key.
        QCOMPARE(g.readEntry(QStringLiteral("parabolicThicknessMarginInfluence"), 1.0f), 0.5f);
    }

    // A second instance loading the same config reproduces every persisted value.
    UniversalSettings reloaded(freshConfig(), this);
    reloaded.load();

    QCOMPARE(reloaded.showInfoWindow(), false);
    QCOMPARE(reloaded.badges3DStyle(), true);
    QCOMPARE(reloaded.canDisableBorders(), true);
    QCOMPARE(reloaded.inAdvancedModeForEditSettings(), true);
    QCOMPARE(reloaded.inConfigureAppletsMode(), true);
    QCOMPARE(reloaded.isAvailableGeometryBroadcastedToPlasma(), false);
    QCOMPARE(reloaded.metaPressAndHoldEnabled(), false);
    QCOMPARE(reloaded.version(), 7);
    QCOMPARE(reloaded.screenTrackerInterval(), 4000);
    QCOMPARE(reloaded.parabolicSpread(), 9);
    QCOMPARE(reloaded.thicknessMarginInfluence(), 0.5f);
    QCOMPARE(reloaded.singleModeLayoutName(), QStringLiteral("MyLayout"));
    QCOMPARE(reloaded.launchers(), launchers);
    QCOMPARE(reloaded.contextMenuActionsAlwaysShown(), actions);
}

void UniversalSettingsTest::enumAndDefaultsLoadFromEmptyConfig()
{
    // Seed userConfiguredAutostart so load() won't try to enable autostart (which
    // would touch the real autostart directory). Everything else stays at default.
    {
        KConfig seed(m_configPath, KConfig::SimpleConfig);
        seed.group(QStringLiteral("UniversalSettings")).writeEntry(QStringLiteral("userConfiguredAutostart"), true);
        seed.sync();
    }

    UniversalSettings settings(freshConfig(), this);
    settings.load();

    // Documented defaults, read back through loadConfig on an otherwise empty group.
    QCOMPARE(settings.version(), 1);
    QCOMPARE(settings.badges3DStyle(), false);
    QCOMPARE(settings.canDisableBorders(), false);
    QCOMPARE(settings.showInfoWindow(), true);
    QCOMPARE(settings.metaPressAndHoldEnabled(), true);
    QCOMPARE(settings.isAvailableGeometryBroadcastedToPlasma(), true);
    QCOMPARE(settings.screenTrackerInterval(), 2500);
    QCOMPARE(settings.parabolicSpread(), Data::Preferences::PARABOLICSPREAD);
    QCOMPARE(settings.thicknessMarginInfluence(), Data::Preferences::THICKNESSMARGININFLUENCE);

    // contextMenuActionsAlwaysShown defaults to the built-in always-visible set.
    QCOMPARE(settings.contextMenuActionsAlwaysShown(), Latte::Data::ContextMenu::ACTIONSALWAYSVISIBLE);
}

void UniversalSettingsTest::screenScalesRoundTrip()
{
    {
        KConfig seed(m_configPath, KConfig::SimpleConfig);
        seed.group(QStringLiteral("UniversalSettings")).writeEntry(QStringLiteral("userConfiguredAutostart"), true);
        seed.sync();
    }

    {
        UniversalSettings settings(freshConfig(), this);
        settings.load();

        // Unknown screen always reads back unity scale.
        QCOMPARE(settings.screenWidthScale(QStringLiteral("Nonexistent")), 1.0f);
        QCOMPARE(settings.screenHeightScale(QStringLiteral("Nonexistent")), 1.0f);

        QSignalSpy scaleSpy(&settings, &UniversalSettings::screenScalesChanged);
        settings.setScreenScales(QStringLiteral("DP-1"), 1.25f, 0.75f);
        QCOMPARE(scaleSpy.count(), 1);
        QCOMPARE(settings.screenWidthScale(QStringLiteral("DP-1")), 1.25f);
        QCOMPARE(settings.screenHeightScale(QStringLiteral("DP-1")), 0.75f);

        // Same values are a guarded no-op.
        settings.setScreenScales(QStringLiteral("DP-1"), 1.25f, 0.75f);
        QCOMPARE(scaleSpy.count(), 1);
    }

    // Scales persist to the nested [UniversalSettings][ScreenScales] group and a
    // fresh load brings them back.
    UniversalSettings reloaded(freshConfig(), this);
    reloaded.load();
    QCOMPARE(reloaded.screenWidthScale(QStringLiteral("DP-1")), 1.25f);
    QCOMPARE(reloaded.screenHeightScale(QStringLiteral("DP-1")), 0.75f);
}

void UniversalSettingsTest::sensitivityAlwaysHigh()
{
    // The port deliberately hard-returns HighMouseSensitivity regardless of stored
    // state (the setter updates the member but the getter ignores it).
    UniversalSettings settings(freshConfig(), this);
    QCOMPARE(settings.sensitivity(), Settings::HighMouseSensitivity);

    settings.setSensitivity(Settings::LowMouseSensitivity);
    QCOMPARE(settings.sensitivity(), Settings::HighMouseSensitivity);
}

// The UniversalSettings ctor connects to QGuiApplication::screenAdded/screenRemoved
// and several getters reach qGuiApp, so a real QGuiApplication is required; the
// offscreen platform (set in CMake) keeps it headless.
QTEST_MAIN(UniversalSettingsTest)

#include "universalsettingstest.moc"
