/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include "../app/settings/settingsnameutils.h"

using namespace Latte::Settings;

class SettingsNameUtilsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // uniqueName
    void uniqueName_freeNameUntouched();
    void uniqueName_collisionNumbersFromTwo();
    void uniqueName_walksPastConsecutive();
    void uniqueName_suffixedInputStripsNotStacks();
    void uniqueName_suffixKeptWhenBaseAbsent();
    void uniqueName_pos0SuffixNotStripped();
    void uniqueName_caseSensitive();
    // rowForValue
    void rowForValue_found();
    void rowForValue_firstMatchWins();
    void rowForValue_missReturnsNegativeOne();
    void rowForValue_emptyListReturnsNegativeOne();
    // needsRename / plannedRenames
    void needsRename_nameChanged();
    void needsRename_temporaryAlwaysRenames();
    void needsRename_unchangedPersistentSkips();
    void plannedRenames_picksOnlyAltered();
    void plannedRenames_activeFlagsShorterDefaultsFalse();
    // pasteSkipsView / pasteTurnsCutIntoMove
    void pasteSkipsView_cutFromCurrentLayoutSkipped();
    void pasteSkipsView_copyNotSkipped();
    void pasteSkipsView_cutFromOtherLayoutNotSkipped();
    void pasteTurnsCutIntoMove_trueAndFalse();
};

// ---------- uniqueName ----------

void SettingsNameUtilsTest::uniqueName_freeNameUntouched()
{
    QCOMPARE(uniqueName(QStringLiteral("Fresh"), {}), QStringLiteral("Fresh"));
    QCOMPARE(uniqueName(QStringLiteral("Fresh"), {QStringLiteral("Other")}), QStringLiteral("Fresh"));
}

void SettingsNameUtilsTest::uniqueName_collisionNumbersFromTwo()
{
    QCOMPARE(uniqueName(QStringLiteral("Taken"), {QStringLiteral("Taken")}), QStringLiteral("Taken - 2"));
}

void SettingsNameUtilsTest::uniqueName_walksPastConsecutive()
{
    QCOMPARE(uniqueName(QStringLiteral("Taken"), {QStringLiteral("Taken"), QStringLiteral("Taken - 2")}),
             QStringLiteral("Taken - 3"));
}

void SettingsNameUtilsTest::uniqueName_suffixedInputStripsNotStacks()
{
    // "Taken - 2" strips to "Taken"; "Taken" and "Taken - 2" both occupied → next free slot is "Taken - 3".
    // Crucially the result is NOT "Taken - 2 - 2" — the suffix is stripped, not stacked.
    QCOMPARE(uniqueName(QStringLiteral("Taken - 2"), {QStringLiteral("Taken"), QStringLiteral("Taken - 2")}),
             QStringLiteral("Taken - 3"));
}

void SettingsNameUtilsTest::uniqueName_suffixKeptWhenBaseAbsent()
{
    // No collision, so the suffix stays — the name is returned as-is.
    QCOMPARE(uniqueName(QStringLiteral("My - 5"), {}), QStringLiteral("My - 5"));
}

void SettingsNameUtilsTest::uniqueName_pos0SuffixNotStripped()
{
    // " - 3" starts at position 0 → pos_ == 0, NOT > 0, so no strip → " - 3 - 2"
    QCOMPARE(uniqueName(QStringLiteral(" - 3"), {QStringLiteral(" - 3")}), QStringLiteral(" - 3 - 2"));
}

void SettingsNameUtilsTest::uniqueName_caseSensitive()
{
    // "Taken" != "taken" so no collision — returned unchanged.
    QCOMPARE(uniqueName(QStringLiteral("Taken"), {QStringLiteral("taken")}), QStringLiteral("Taken"));
}

// ---------- rowForValue ----------

void SettingsNameUtilsTest::rowForValue_found()
{
    const QStringList list = {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    QCOMPARE(rowForValue(list, QStringLiteral("b")), 1);
}

void SettingsNameUtilsTest::rowForValue_firstMatchWins()
{
    const QStringList list = {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("b")};
    QCOMPARE(rowForValue(list, QStringLiteral("b")), 1);
}

void SettingsNameUtilsTest::rowForValue_missReturnsNegativeOne()
{
    const QStringList list = {QStringLiteral("a"), QStringLiteral("b")};
    QCOMPARE(rowForValue(list, QStringLiteral("z")), -1);
}

void SettingsNameUtilsTest::rowForValue_emptyListReturnsNegativeOne()
{
    QCOMPARE(rowForValue({}, QStringLiteral("x")), -1);
}

// ---------- needsRename / plannedRenames ----------

static Latte::Data::Layout makeLayout(const QString &id, const QString &name)
{
    Latte::Data::Layout l;
    l.id = id;
    l.name = name;
    return l;
}

void SettingsNameUtilsTest::needsRename_nameChanged()
{
    const auto current = makeLayout(QStringLiteral("/home/user/A.layout.latte"), QStringLiteral("B"));
    const auto original = makeLayout(QStringLiteral("/home/user/A.layout.latte"), QStringLiteral("A"));
    QVERIFY(needsRename(current, original));
}

void SettingsNameUtilsTest::needsRename_temporaryAlwaysRenames()
{
    // Same name, but the id lives under /tmp → isTemporary() == true → needs rename.
    const auto current = makeLayout(QStringLiteral("/tmp/x.layout.latte"), QStringLiteral("MyLayout"));
    const auto original = makeLayout(QStringLiteral("/tmp/x.layout.latte"), QStringLiteral("MyLayout"));
    QVERIFY(needsRename(current, original));
}

void SettingsNameUtilsTest::needsRename_unchangedPersistentSkips()
{
    const auto current = makeLayout(QStringLiteral("/home/user/A.layout.latte"), QStringLiteral("A"));
    const auto original = makeLayout(QStringLiteral("/home/user/A.layout.latte"), QStringLiteral("A"));
    QVERIFY(!needsRename(current, original));
}

void SettingsNameUtilsTest::plannedRenames_picksOnlyAltered()
{
    // Two layouts: A unchanged, B renamed. Only B should appear in the output.
    const auto aOld = makeLayout(QStringLiteral("/home/user/A.layout.latte"), QStringLiteral("A"));
    const auto aNew = makeLayout(QStringLiteral("/home/user/A.layout.latte"), QStringLiteral("A"));
    const auto bOld = makeLayout(QStringLiteral("/home/user/B.layout.latte"), QStringLiteral("B-old"));
    const auto bNew = makeLayout(QStringLiteral("/home/user/B.layout.latte"), QStringLiteral("B-new"));

    const QList<Latte::Data::Layout> currents = {aNew, bNew};
    const QList<Latte::Data::Layout> originals = {aOld, bOld};
    const QList<bool> activeFlags = {false, true};

    const auto result = plannedRenames(currents, originals, activeFlags);
    QCOMPARE(result.count(), 1);
    QCOMPARE(result[0].oldId, QStringLiteral("/home/user/B.layout.latte"));
    QCOMPARE(result[0].newName, QStringLiteral("B-new"));
    QVERIFY(result[0].wasActive);
}

void SettingsNameUtilsTest::plannedRenames_activeFlagsShorterDefaultsFalse()
{
    // activeFlags shorter than currents → missing entries treated as wasActive=false.
    const auto aOld = makeLayout(QStringLiteral("/home/user/A.layout.latte"), QStringLiteral("A-old"));
    const auto aNew = makeLayout(QStringLiteral("/home/user/A.layout.latte"), QStringLiteral("A-new"));

    const QList<Latte::Data::Layout> currents = {aNew};
    const QList<Latte::Data::Layout> originals = {aOld};
    const QList<bool> activeFlags = {}; // shorter

    const auto result = plannedRenames(currents, originals, activeFlags);
    QCOMPARE(result.count(), 1);
    QVERIFY(!result[0].wasActive);
}

// ---------- pasteSkipsView / pasteTurnsCutIntoMove ----------

void SettingsNameUtilsTest::pasteSkipsView_cutFromCurrentLayoutSkipped()
{
    QVERIFY(pasteSkipsView(true, QStringLiteral("L1"), QStringLiteral("L1")));
}

void SettingsNameUtilsTest::pasteSkipsView_copyNotSkipped()
{
    // isMoveOrigin==false means it was copied, never skipped regardless of layout ids.
    QVERIFY(!pasteSkipsView(false, QStringLiteral("L1"), QStringLiteral("L1")));
}

void SettingsNameUtilsTest::pasteSkipsView_cutFromOtherLayoutNotSkipped()
{
    QVERIFY(!pasteSkipsView(true, QStringLiteral("L2"), QStringLiteral("L1")));
}

void SettingsNameUtilsTest::pasteTurnsCutIntoMove_trueAndFalse()
{
    QVERIFY(pasteTurnsCutIntoMove(true));
    QVERIFY(!pasteTurnsCutIntoMove(false));
}

QTEST_GUILESS_MAIN(SettingsNameUtilsTest)
#include "settingsnameutilstest.moc"
