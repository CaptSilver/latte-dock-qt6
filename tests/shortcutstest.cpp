/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Real-link unit tests for the two pure-logic shortcut helpers:
//   ShortcutsPart::ShortcutsTracker  — parses [lattedock] entries from kglobalshortcutsrc
//   ShortcutsPart::ModifierTracker   — modifier press/track bookkeeping
// Both link against the real production .cpp.

#include "shortcutstracker.h"
#include "modifiertracker.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QKeySequence>
#include <QtTest>

using namespace Latte::ShortcutsPart;

class ShortcutsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void init();

    //! ShortcutsTracker
    void emptyConfigGivesNineteenEmptyBadges();
    void metaSingleCharBadgeIsLowercased();
    void multiModifierBadgeIsUppercased();
    void noneShortcutGivesEmptyBadge();
    void unsetActiveShortcutGivesEmptyBadge();
    void tabSeparatedRecordIsTrimmed();
    void basedOnPositionNeedsFirstTwoEntries();
    void basedOnPositionStaysOffWhenSecondMissing();
    void appletWidgetShortcutParsesIdAndBadge();
    void unknownAppletBadgeIsEmpty();

    //! ModifierTracker
    void freshTrackerHasNoModifierPressed();
    void singleModifierFalseWhenNothingPressed();
    void emptySequenceModifierIsNotPressed();
    void sequenceModifierFalseWhenNothingPressed();
    void blockUnblockDoNotCrash();

private:
    //! write a kglobalshortcutsrc under the temp XDG_CONFIG_HOME with the given
    //! [lattedock] body lines, then return a freshly-parsed tracker.
    void writeShortcutsConfig(const QStringList &latteLines);

    QTemporaryDir *m_configDir{nullptr};
};

void ShortcutsTest::initTestCase()
{
    m_configDir = new QTemporaryDir();
    QVERIFY(m_configDir->isValid());
    qputenv("XDG_CONFIG_HOME", m_configDir->path().toUtf8());
    // QStandardPaths caches config locations off the env at first use; reset so
    // configPath() picks up our temp dir.
    QStandardPaths::setTestModeEnabled(false);
}

void ShortcutsTest::cleanupTestCase()
{
    delete m_configDir;
    m_configDir = nullptr;
}

void ShortcutsTest::init()
{
    // Start each case from a clean config file.
    const QString path = m_configDir->path() + QStringLiteral("/kglobalshortcutsrc");
    QFile::remove(path);
}

void ShortcutsTest::writeShortcutsConfig(const QStringList &latteLines)
{
    const QString path = m_configDir->path() + QStringLiteral("/kglobalshortcutsrc");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    QTextStream out(&f);
    out << QStringLiteral("[lattedock]") << QStringLiteral("\n");
    out << QStringLiteral("_k_friendly_name=Latte Dock") << QStringLiteral("\n");
    for (const QString &line : latteLines) {
        out << line << QStringLiteral("\n");
    }
    f.close();
}

void ShortcutsTest::emptyConfigGivesNineteenEmptyBadges()
{
    // No [lattedock] group at all -> parseGlobalShortcuts() leaves the 19 empty
    // placeholders it seeded in initGlobalShortcutsWatcher().
    const QString path = m_configDir->path() + QStringLiteral("/kglobalshortcutsrc");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    QTextStream out(&f);
    out << QStringLiteral("[someothergroup]\n") << QStringLiteral("foo=bar\n");
    f.close();

    ShortcutsTracker tracker(nullptr);
    QCOMPARE(tracker.badgesForActivate().count(), 19);
    for (const QString &badge : tracker.badgesForActivate()) {
        QVERIFY(badge.isEmpty());
    }
    QVERIFY(!tracker.basedOnPositionEnabled());
}

void ShortcutsTest::metaSingleCharBadgeIsLowercased()
{
    // "Meta+Z" -> two tokens, first is "Meta" -> the lowercase branch.
    writeShortcutsConfig({QStringLiteral("activate entry 11=Meta+Z,Meta+Z,Activate Entry 11")});

    ShortcutsTracker tracker(nullptr);
    // entry 11 lands at index 10 (entries are 1-based).
    QCOMPARE(tracker.badgesForActivate().at(10), QStringLiteral("z"));
}

void ShortcutsTest::multiModifierBadgeIsUppercased()
{
    // Three tokens (more than the Meta+char scheme) -> uppercase branch.
    writeShortcutsConfig({QStringLiteral("activate entry 3=Meta+Ctrl+5,Meta+Ctrl+5,Activate Entry 3")});

    ShortcutsTracker tracker(nullptr);
    QCOMPARE(tracker.badgesForActivate().at(2), QStringLiteral("5"));
}

void ShortcutsTest::noneShortcutGivesEmptyBadge()
{
    // The literal "none" active shortcut is treated as no badge.
    writeShortcutsConfig({QStringLiteral("activate entry 4=none,none,Activate Entry 4")});

    ShortcutsTracker tracker(nullptr);
    QVERIFY(tracker.badgesForActivate().at(3).isEmpty());
}

void ShortcutsTest::unsetActiveShortcutGivesEmptyBadge()
{
    // Real-world shape: active shortcut empty, only the default is set.
    // records[0] is "" -> empty badge.
    writeShortcutsConfig({QStringLiteral("activate entry 5=,Meta+5,Activate Entry 5")});

    ShortcutsTracker tracker(nullptr);
    QVERIFY(tracker.badgesForActivate().at(4).isEmpty());
}

void ShortcutsTest::tabSeparatedRecordIsTrimmed()
{
    // KConfig escapes a literal tab as \t inside a value; parseGlobalShortcuts
    // splits records[0] on '\t' and keeps the head. "Meta+Q\tjunk" -> "Meta+Q" -> "q".
    writeShortcutsConfig({QStringLiteral("activate entry 6=Meta+Q\\tjunk,Meta+Q,Activate Entry 6")});

    ShortcutsTracker tracker(nullptr);
    QCOMPARE(tracker.badgesForActivate().at(5), QStringLiteral("q"));
}

void ShortcutsTest::basedOnPositionNeedsFirstTwoEntries()
{
    // basedOnPositionEnabled is true only when BOTH entry 1 and entry 2 carry a badge.
    writeShortcutsConfig({
        QStringLiteral("activate entry 1=Meta+1,Meta+1,Activate Entry 1"),
        QStringLiteral("activate entry 2=Meta+2,Meta+2,Activate Entry 2"),
    });

    ShortcutsTracker tracker(nullptr);
    QCOMPARE(tracker.badgesForActivate().at(0), QStringLiteral("1"));
    QCOMPARE(tracker.badgesForActivate().at(1), QStringLiteral("2"));
    QVERIFY(tracker.basedOnPositionEnabled());
}

void ShortcutsTest::basedOnPositionStaysOffWhenSecondMissing()
{
    writeShortcutsConfig({
        QStringLiteral("activate entry 1=Meta+1,Meta+1,Activate Entry 1"),
        // entry 2 absent
    });

    ShortcutsTracker tracker(nullptr);
    QCOMPARE(tracker.badgesForActivate().at(0), QStringLiteral("1"));
    QVERIFY(tracker.badgesForActivate().at(1).isEmpty());
    QVERIFY(!tracker.basedOnPositionEnabled());
}

void ShortcutsTest::appletWidgetShortcutParsesIdAndBadge()
{
    // "activate widget <id>" keys feed appletShortcutBadge(id).
    writeShortcutsConfig({
        QStringLiteral("activate widget 42=Meta+W,Meta+W,Activate Widget 42"),
    });

    ShortcutsTracker tracker(nullptr);
    QCOMPARE(tracker.appletShortcutBadge(42), QStringLiteral("w"));

    const QList<uint> applets = tracker.appletsWithPlasmaShortcuts();
    QCOMPARE(applets.count(), 1);
    QCOMPARE(applets.first(), static_cast<uint>(42));
}

void ShortcutsTest::unknownAppletBadgeIsEmpty()
{
    writeShortcutsConfig({QStringLiteral("activate entry 1=Meta+1,Meta+1,Activate Entry 1")});

    ShortcutsTracker tracker(nullptr);
    QVERIFY(tracker.appletShortcutBadge(999).isEmpty());
}

void ShortcutsTest::freshTrackerHasNoModifierPressed()
{
    ModifierTracker tracker(nullptr);
    QVERIFY(tracker.noModifierPressed());
}

void ShortcutsTest::singleModifierFalseWhenNothingPressed()
{
    // singleModifierPressed(key) requires <key> pressed and all others released;
    // nothing is pressed at startup, so the target-key check fails.
    ModifierTracker tracker(nullptr);
    QVERIFY(!tracker.singleModifierPressed(Qt::Key_Super_L));
    QVERIFY(!tracker.singleModifierPressed(Qt::Key_Control));
}

void ShortcutsTest::emptySequenceModifierIsNotPressed()
{
    ModifierTracker tracker(nullptr);
    QVERIFY(!tracker.sequenceModifierPressed(QKeySequence()));
}

void ShortcutsTest::sequenceModifierFalseWhenNothingPressed()
{
    ModifierTracker tracker(nullptr);
    QVERIFY(!tracker.sequenceModifierPressed(QKeySequence(Qt::META | Qt::Key_A)));
    QVERIFY(!tracker.sequenceModifierPressed(QKeySequence(Qt::CTRL | Qt::Key_C)));
}

void ShortcutsTest::blockUnblockDoNotCrash()
{
    // Block/unblock mutate the blocked-modifier list; with nothing pressed the
    // observable predicates stay stable. Mainly a guard that the public API is callable.
    ModifierTracker tracker(nullptr);
    tracker.blockModifierTracking(Qt::Key_Super_L);
    tracker.unblockModifierTracking(Qt::Key_Super_L);
    // unblocking a key that was never blocked is a no-op, must not throw.
    tracker.unblockModifierTracking(Qt::Key_Alt);
    tracker.cancelMetaPressed();
    QVERIFY(tracker.noModifierPressed());
}

QTEST_MAIN(ShortcutsTest)

#include "shortcutstest.moc"
