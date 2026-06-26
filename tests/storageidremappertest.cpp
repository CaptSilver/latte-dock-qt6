/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Pure unit tests for StorageIdRemapper — the id-assignment algorithm lifted
// from Storage::newUniqueIdsFile.  No KConfig, no Corona graph required.

#include "storageidremapper.h"

#include <QObject>
#include <QtTest>

using namespace Latte::Layouts;

#define QL(x) QStringLiteral(x)

class StorageIdRemapperTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void availableId_findsFirstGapAtOrAboveBase();
    void availableId_exhaustionReturnsEmpty();
    void remap_keepsHighFreeIdsUnchanged();
    void remap_reassignsLowIds();
    void remap_reassignsCollidingUsedId();
    void remap_containmentsBeforeApplets_distinctRanges();
    void remap_appletAllocationSkipsContainmentAssignments();
    void remap_twoCycleCheckDoesNotCorruptNonCycleCase();
    void remap_emptyIdGetsFreshId();
    void mapped_passthroughForUnknownKey();
    void remap_orderKeyBehavior_unknownTokenMapsToEmpty();
};

void StorageIdRemapperTest::availableId_findsFirstGapAtOrAboveBase()
{
    // ({"12","13"},{},12) → "14": 12 taken, 13 taken, 14 is first gap
    QCOMPARE(StorageIdRemapper::availableId({QL("12"), QL("13")}, {}, 12), QL("14"));
    // ({},{},12) → "12": nothing taken, base is free
    QCOMPARE(StorageIdRemapper::availableId({}, {}, 12), QL("12"));
    // ({"40"},{"41"},40) → "42": 40 in all, 41 in assigned, 42 free
    QCOMPARE(StorageIdRemapper::availableId({QL("40")}, {QL("41")}, 40), QL("42"));
    // ({"5"},{},12) → "12": 5 is in all but below base; base=12 is free
    QCOMPARE(StorageIdRemapper::availableId({QL("5")}, {}, 12), QL("12"));
}

void StorageIdRemapperTest::availableId_exhaustionReturnsEmpty()
{
    // Only slot at base=31999 is taken; 32000 is the exclusive cap → exhausted
    QCOMPARE(StorageIdRemapper::availableId({QL("31999")}, {}, 31999), QL(""));
}

void StorageIdRemapperTest::remap_keepsHighFreeIdsUnchanged()
{
    // usedIds empty → no collisions; containment 12 and applet 40 keep themselves
    IdRemap r = StorageIdRemapper::remap({{}, {QL("12")}, {QL("40")}});
    QCOMPARE(r.assigned.value(QL("12")), QL("12"));
    QCOMPARE(r.assigned.value(QL("40")), QL("40"));
}

void StorageIdRemapperTest::remap_reassignsLowIds()
{
    // Containment "1" < 12 → reassigned to 12 (lowest free containment base).
    // Applet "5" < 40 → reassigned to 40 (lowest free applet base).
    IdRemap r = StorageIdRemapper::remap({{}, {QL("1")}, {QL("5")}});
    QCOMPARE(r.assigned.value(QL("1")), QL("12"));
    QCOMPARE(r.assigned.value(QL("5")), QL("40"));
}

void StorageIdRemapperTest::remap_reassignsCollidingUsedId()
{
    // Destination already has id "12"; importing a containment also numbered "12"
    // must receive the next free id (13).
    IdRemap r = StorageIdRemapper::remap({{QL("12")}, {QL("12")}, {}});
    QCOMPARE(r.assigned.value(QL("12")), QL("13"));
}

void StorageIdRemapperTest::remap_containmentsBeforeApplets_distinctRanges()
{
    // Low-numbered containments/applets all get fresh ids from their respective bases.
    // Containments: "1"→12, "2"→13.  Applets: "3"→40, "4"→41.
    IdRemap r = StorageIdRemapper::remap({{}, {QL("1"), QL("2")}, {QL("3"), QL("4")}});
    QCOMPARE(r.assigned.value(QL("1")), QL("12"));
    QCOMPARE(r.assigned.value(QL("2")), QL("13"));
    QCOMPARE(r.assigned.value(QL("3")), QL("40"));
    QCOMPARE(r.assigned.value(QL("4")), QL("41"));
}

void StorageIdRemapperTest::remap_appletAllocationSkipsContainmentAssignments()
{
    // Containment "50" keeps id "50" (assignedIds=["50"]).
    // Applet "50": 50 >= 40 ✓, not in usedIds ✓, but "50" IS in assignedIds
    // → goes to availableId([], ["50"], 40) → "40".
    // The applet pass overwrites the "50" key in assigned, so assigned["50"]="40".
    IdRemap r = StorageIdRemapper::remap({{}, {QL("50")}, {QL("50")}});
    QCOMPARE(r.assigned.value(QL("50")), QL("40"));
}

void StorageIdRemapperTest::remap_twoCycleCheckDoesNotCorruptNonCycleCase()
{
    // The "PROBLEM APPEARED" 2-cycle fix fires when assigned[assigned[X]] == X for X != assigned[X].
    // Exhaustive analysis (see report) shows natural inputs cannot trigger this condition with the
    // current algorithm: any id that forces a remap is either < base (so unavailable as a return
    // value) or in usedIds (so excluded from availableId returns). The fix is defensive dead code.
    //
    // This test verifies the fix does NOT corrupt a near-cycle: X→Y where Y is also a key in
    // assigned, but assigned[Y] != X (no true cycle).
    //
    // usedIds={"12"}, containmentIds={"12","13"}:
    // "12": in usedIds → availableId({"12"}, [], 12) → "13". assigned["12"]="13". assignedIds=["13"]
    // "13": 13>=12 ✓, not in usedIds ✓, "13" IS in assignedIds → availableId({"12"}, ["13"], 12)
    //       → i=12: in all, i=13: in assigned, i=14 → "14". assigned["13"]="14".
    // 2-cycle check "12": value="13", assigned["13"]="14" ≠ "12" → no fix fires.
    IdRemap r = StorageIdRemapper::remap({{QL("12")}, {QL("12"), QL("13")}, {}});
    QCOMPARE(r.assigned.value(QL("12")), QL("13"));
    QCOMPARE(r.assigned.value(QL("13")), QL("14"));
}

void StorageIdRemapperTest::remap_emptyIdGetsFreshId()
{
    // Empty string → toInt() == 0, which is < 12 → gets a fresh id from base 12.
    IdRemap r = StorageIdRemapper::remap({{}, {QL("")}, {}});
    QCOMPARE(r.assigned.value(QL("")), QL("12"));
}

void StorageIdRemapperTest::mapped_passthroughForUnknownKey()
{
    // IdRemap::mapped() passes through ids not in the assignment map.
    IdRemap r;
    QCOMPARE(r.mapped(QL("99")), QL("99"));
}

void StorageIdRemapperTest::remap_orderKeyBehavior_unknownTokenMapsToEmpty()
{
    // In storage.cpp ~:474 the order-key rewrite does `assigned[token]` (operator[]),
    // which INSERTS an empty QString for unknown tokens.  The adapter uses
    // r.assigned.value(token, QString()) to stay output-equivalent without the
    // side-effecting insert — unknown token → "".
    //
    // Verify: a token that is NOT in containmentIds/appletIds is absent from
    // r.assigned, so r.assigned.value(token, QString()) returns "".
    IdRemap r = StorageIdRemapper::remap({{}, {QL("12")}, {QL("40")}});
    // "99" was never an original id → not in assigned
    QVERIFY(!r.assigned.contains(QL("99")));
    QCOMPARE(r.assigned.value(QL("99"), QString()), QString());
    // Confirm value() returns "" without inserting (const access, no side effect):
    int sizeBefore = r.assigned.size();
    QString got = r.assigned.value(QL("99"), QString());
    QCOMPARE(got, QString());
    QCOMPARE(r.assigned.size(), sizeBefore); // no insert happened
}

QTEST_GUILESS_MAIN(StorageIdRemapperTest)

#include "storageidremappertest.moc"
