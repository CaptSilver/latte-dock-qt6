/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "data/activitydata.h"
#include "data/appletdata.h"
#include "data/errordata.h"
#include "data/errorinformationdata.h"
#include "data/genericdata.h"
#include "data/generictable.h"
#include "data/layoutcolordata.h"
#include "data/layoutdata.h"
#include "data/layouticondata.h"
#include "data/layoutstable.h"
#include "data/screendata.h"
#include "data/viewdata.h"
#include "data/viewstable.h"

#include <QDir>
#include <QtTest>

using namespace Latte;

class DataTypesTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void generic_equality();
    void activity_isRunning_truthTable();
    void applet_visibleNameAndInstalled();
    void table_insertSortLookupRemove();
    void table_insertBasedOnNameAndId_caseInsensitive();
    void table_operatorIndex_byIdAndIndex();
    void table_operatorQString_join();
    void layoutColor_setDataAndEquality();
    void layoutIcon_isEmptyAndEquality();
    void errorInformation_isValid();
    void error_codeConstantsAndNestedCompare();
    void layout_activityAndTemplateChecks();
    void layout_operatorEqualExclusions();
    void view_stateMachine();
    void screen_serializeRoundTrip();
    void screen_isScreensGroup();
    void viewsTable_hasContainmentIdRecursion();
    void viewsTable_subtractedAndOnlyOriginals();
    void viewsTable_appendTemporaryView();
    void layoutsTable_subtractedAndFreeActivities();
};

void DataTypesTest::generic_equality()
{
    Data::Generic a(QStringLiteral("id1"), QStringLiteral("Name One"));
    Data::Generic b = a;                 // copy ctor
    QVERIFY(a == b);
    b.name = QStringLiteral("changed");
    QVERIFY(a != b);

    Data::Generic c;
    c = a;                               // copy assignment
    QVERIFY(c == a);
}

void DataTypesTest::activity_isRunning_truthTable()
{
    Data::Activity act;
    act.id = QStringLiteral("act1");

    // KF6 dropped KActivities::Info::State, so the enum is a local mirror with
    // Running==2, Starting==1 — isRunning() must accept both.
    act.state = Data::Activity::Running;
    QVERIFY(act.isRunning());
    QVERIFY(act.isValid());

    act.state = Data::Activity::Starting;
    QVERIFY(act.isRunning());
    QVERIFY(act.isValid());

    act.state = Data::Activity::Stopping;
    QVERIFY(!act.isRunning());
    QVERIFY(act.isValid());

    act.state = Data::Activity::Stopped;
    QVERIFY(!act.isRunning());
    QVERIFY(act.isValid());

    // isValid() keys off state, not id: Invalid is the one non-valid state.
    act.state = Data::Activity::Invalid;
    QVERIFY(!act.isRunning());
    QVERIFY(!act.isValid());
}

void DataTypesTest::applet_visibleNameAndInstalled()
{
    Data::Applet ap;
    ap.id = QStringLiteral("org.kde.foo");
    ap.name = QString();
    QCOMPARE(ap.visibleName(), QStringLiteral("org.kde.foo")); // empty name falls back to id
    QVERIFY(ap.isValid());                                     // id non-empty
    QVERIFY(ap.isInstalled());                                 // valid && id != name

    ap.name = QStringLiteral("Foo");
    QCOMPARE(ap.visibleName(), QStringLiteral("Foo"));
    QVERIFY(ap.isInstalled());

    // id == name means "not installed" (placeholder row)
    ap.name = ap.id;
    QVERIFY(!ap.isInstalled());

    Data::Applet empty;
    QVERIFY(!empty.isValid());
}

void DataTypesTest::table_insertSortLookupRemove()
{
    Data::GenericTable<Data::Generic> t;
    Data::Generic row(QStringLiteral("b"), QStringLiteral("Beta"));
    t << Data::Generic(QStringLiteral("a"), QStringLiteral("Alpha"))
      << row
      << Data::Generic(QStringLiteral("c"), QStringLiteral("Gamma"));
    QCOMPARE(t.rowCount(), 3);
    QVERIFY(t.containsId(QStringLiteral("b")));
    QVERIFY(t.containsName(QStringLiteral("Beta")));
    QCOMPARE(t.indexOf(QStringLiteral("b")), 1);
    QCOMPARE(t.idForName(QStringLiteral("Gamma")), QStringLiteral("c"));

    t.remove(QStringLiteral("b"));
    QVERIFY(!t.containsId(QStringLiteral("b")));
    QCOMPARE(t.rowCount(), 2);

    // operator<< rejects empty-id rows
    Data::Generic empty;
    t << empty;
    QCOMPARE(t.rowCount(), 2);

    // remove(int) is guarded against out-of-range
    t.remove(99);
    QCOMPARE(t.rowCount(), 2);
    t.remove(0);
    QCOMPARE(t.rowCount(), 1);

    t.clear();
    QVERIFY(t.isEmpty());
    QCOMPARE(t.indexOf(QStringLiteral("nope")), -1);
}

void DataTypesTest::table_insertBasedOnNameAndId_caseInsensitive()
{
    Data::GenericTable<Data::Generic> byName;
    byName.insertBasedOnName(Data::Generic(QStringLiteral("1"), QStringLiteral("banana")));
    byName.insertBasedOnName(Data::Generic(QStringLiteral("2"), QStringLiteral("Apple")));
    byName.insertBasedOnName(Data::Generic(QStringLiteral("3"), QStringLiteral("cherry")));
    // Case-insensitive sort: Apple, banana, cherry
    QCOMPARE(byName.names(), (QStringList{QStringLiteral("Apple"), QStringLiteral("banana"), QStringLiteral("cherry")}));

    Data::GenericTable<Data::Generic> byId;
    byId.insertBasedOnId(Data::Generic(QStringLiteral("Bravo"), QStringLiteral("b")));
    byId.insertBasedOnId(Data::Generic(QStringLiteral("alpha"), QStringLiteral("a")));
    byId.insertBasedOnId(Data::Generic(QStringLiteral("Charlie"), QStringLiteral("c")));
    QCOMPARE(byId.ids(), (QStringList{QStringLiteral("alpha"), QStringLiteral("Bravo"), QStringLiteral("Charlie")}));
}

void DataTypesTest::table_operatorIndex_byIdAndIndex()
{
    Data::GenericTable<Data::Generic> t;
    t << Data::Generic(QStringLiteral("a"), QStringLiteral("Alpha"))
      << Data::Generic(QStringLiteral("b"), QStringLiteral("Beta"));

    // by-id lookup (non-const)
    QCOMPARE(t[QStringLiteral("b")].name, QStringLiteral("Beta"));
    // by-id mutation writes back
    t[QStringLiteral("b")].name = QStringLiteral("Beta2");
    QCOMPARE(t[QStringLiteral("b")].name, QStringLiteral("Beta2"));

    // by-index lookup needs a uint to pick the index overload
    QCOMPARE(t[uint(0)].id, QStringLiteral("a"));

    const Data::GenericTable<Data::Generic> ct = t;
    QCOMPARE(ct[QStringLiteral("a")].name, QStringLiteral("Alpha"));
    QCOMPARE(ct[uint(1)].id, QStringLiteral("b"));
}

void DataTypesTest::table_operatorQString_join()
{
    Data::GenericTable<Data::Generic> t;
    QCOMPARE(static_cast<QString>(t), QString());

    t << Data::Generic(QStringLiteral("a"), QStringLiteral("Alpha"))
      << Data::Generic(QStringLiteral("b"), QStringLiteral("Beta"))
      << Data::Generic(QStringLiteral("c"), QStringLiteral("Gamma"));
    QCOMPARE(static_cast<QString>(t), QStringLiteral("a, b, c"));
}

void DataTypesTest::layoutColor_setDataAndEquality()
{
    Data::LayoutColor c;
    c.setData(QStringLiteral("blue"), QStringLiteral("Blue"), QStringLiteral("/path/blue.svg"), QStringLiteral("#ffffff"));
    QCOMPARE(c.id, QStringLiteral("blue"));
    QCOMPARE(c.name, QStringLiteral("Blue"));
    QCOMPARE(c.path, QStringLiteral("/path/blue.svg"));
    QCOMPARE(c.textColor, QStringLiteral("#ffffff"));

    Data::LayoutColor d = c;
    QVERIFY(c == d);
    d.textColor = QStringLiteral("#000000");
    QVERIFY(c != d);
}

void DataTypesTest::layoutIcon_isEmptyAndEquality()
{
    Data::LayoutIcon icon;
    QVERIFY(icon.isEmpty());             // empty id+name
    QVERIFY(icon.isBackgroundFile);      // default true

    icon.id = QStringLiteral("configure");
    QVERIFY(!icon.isEmpty());

    Data::LayoutIcon other = icon;
    QVERIFY(other == icon);
    other.isBackgroundFile = false;
    QVERIFY(other != icon);
}

void DataTypesTest::errorInformation_isValid()
{
    Data::ErrorInformation info;
    QVERIFY(!info.isValid());            // neither containment nor applet valid

    info.applet.id = QStringLiteral("org.kde.foo");
    QVERIFY(info.isValid());             // applet.isValid() path

    Data::ErrorInformation info2;
    info2.containment.id = QStringLiteral("99");
    QVERIFY(info2.isValid());            // containment.isValid() path
}

void DataTypesTest::error_codeConstantsAndNestedCompare()
{
    QCOMPARE(QString::fromLatin1(Data::Error::APPLETSWITHSAMEID), QStringLiteral("E103"));
    QCOMPARE(QString::fromLatin1(Data::Error::ORPHANEDPARENTAPPLETOFSUBCONTAINMENT), QStringLiteral("E107"));
    QCOMPARE(QString::fromLatin1(Data::Error::ORPHANEDSUBCONTAINMENT), QStringLiteral("W201"));
    QCOMPARE(QString::fromLatin1(Data::Error::APPLETANDCONTAINMENTWITHSAMEID), QStringLiteral("W205"));

    Data::Error e;
    e.id = QString::fromLatin1(Data::Error::APPLETSWITHSAMEID);
    e.name = QStringLiteral("duplicate id");
    QVERIFY(e.isValid());

    Data::ErrorInformation inf;
    inf.id = QStringLiteral("i1");
    inf.applet.id = QStringLiteral("org.kde.foo");
    e.information << inf;

    Data::Error copy = e;
    QVERIFY(copy == e);

    // mutating the nested information table breaks equality
    Data::ErrorInformation inf2;
    inf2.id = QStringLiteral("i2");
    inf2.applet.id = QStringLiteral("org.kde.bar");
    copy.information << inf2;
    QVERIFY(copy != e);
}

void DataTypesTest::layout_activityAndTemplateChecks()
{
    Data::Layout lay;
    lay.id = QStringLiteral("l1");
    lay.name = QStringLiteral("Layout 1");

    // single-entry sentinel checks
    lay.activities = QStringList{QString::fromLatin1(Data::Layout::ALLACTIVITIESID)};
    QVERIFY(lay.isOnAllActivities());
    QVERIFY(!lay.isForFreeActivities());

    lay.activities = QStringList{QString::fromLatin1(Data::Layout::FREEACTIVITIESID)};
    QVERIFY(lay.isForFreeActivities());
    QVERIFY(!lay.isOnAllActivities());

    // two entries → neither single-sentinel check matches
    lay.activities = QStringList{QString::fromLatin1(Data::Layout::ALLACTIVITIESID), QStringLiteral("extra")};
    QVERIFY(!lay.isOnAllActivities());
    QVERIFY(!lay.isForFreeActivities());

    // isTemporary keys off a /tmp id prefix
    Data::Layout tmp;
    tmp.id = QStringLiteral("/tmp/foo.layout.latte");
    QVERIFY(tmp.isTemporary());
    Data::Layout notmp;
    notmp.id = QStringLiteral("/home/u/foo.layout.latte");
    QVERIFY(!notmp.isTemporary());

    // isSystemTemplate: a template whose id is under neither tempPath nor homePath
    Data::Layout systmpl;
    systmpl.id = QStringLiteral("/usr/share/latte/templates/Default.layout.latte");
    systmpl.isTemplate = true;
    QVERIFY(systmpl.isSystemTemplate());

    Data::Layout hometmpl;
    hometmpl.id = QDir::homePath() + QStringLiteral("/.config/latte/My.layout.latte");
    hometmpl.isTemplate = true;
    QVERIFY(!hometmpl.isSystemTemplate());

    // not a template at all
    systmpl.isTemplate = false;
    QVERIFY(!systmpl.isSystemTemplate());

    // isNull / isEmpty
    Data::Layout nul;
    QVERIFY(nul.isNull());
    QVERIFY(nul.isEmpty());
    QVERIFY(!lay.isNull());

    // hasErrors / hasWarnings
    lay.errors = 0;
    lay.warnings = 0;
    QVERIFY(!lay.hasErrors());
    QVERIFY(!lay.hasWarnings());
    lay.errors = 2;
    lay.warnings = 1;
    QVERIFY(lay.hasErrors());
    QVERIFY(lay.hasWarnings());
}

void DataTypesTest::layout_operatorEqualExclusions()
{
    Data::Layout a;
    a.id = QStringLiteral("l1");
    a.name = QStringLiteral("Layout");
    a.color = QStringLiteral("blue");
    a.activities = QStringList{QStringLiteral("act1")};

    Data::Layout b = a;
    QVERIFY(a == b);

    // These fields are deliberately excluded from operator== (state, not data):
    // lastUsedActivity, isActive, isConsideredActive, errors, warnings.
    b.lastUsedActivity = QStringLiteral("somewhere");
    b.isActive = true;
    b.isConsideredActive = true;
    b.errors = 5;
    b.warnings = 9;
    QVERIFY(a == b);                     // still equal despite those differences

    // A real data field flips equality
    b.color = QStringLiteral("red");
    QVERIFY(a != b);
}

void DataTypesTest::view_stateMachine()
{
    Data::View v;
    // default state IsInvalid → not valid, not created
    QVERIFY(!v.isValid());
    QVERIFY(!v.isCreated());

    v.setState(Data::View::IsCreated);
    QVERIFY(v.isValid());
    QVERIFY(v.isCreated());
    QCOMPARE(v.state(), Data::View::IsCreated);

    v.setState(Data::View::OriginFromViewTemplate, QStringLiteral("/file.view.latte"), QStringLiteral("MyLayout"), QStringLiteral("MyView"));
    QVERIFY(v.hasViewTemplateOrigin());
    QVERIFY(!v.hasLayoutOrigin());
    QCOMPARE(v.originFile(), QStringLiteral("/file.view.latte"));
    QCOMPARE(v.originLayout(), QStringLiteral("MyLayout"));
    QCOMPARE(v.originView(), QStringLiteral("MyView"));

    v.setState(Data::View::OriginFromLayout);
    QVERIFY(v.hasLayoutOrigin());
    QVERIFY(!v.hasViewTemplateOrigin());

    // cloned vs original
    QVERIFY(v.isOriginal());
    QVERIFY(!v.isCloned());
    v.isClonedFrom = 7;
    QVERIFY(v.isCloned());
    QVERIFY(!v.isOriginal());

    // horizontal / vertical keys off edge
    v.edge = Plasma::Types::BottomEdge;
    QVERIFY(v.isHorizontal());
    QVERIFY(!v.isVertical());
    v.edge = Plasma::Types::LeftEdge;
    QVERIFY(v.isVertical());
    QVERIFY(!v.isHorizontal());

    // subcontainments membership
    v.subcontainments << Data::Generic(QStringLiteral("sub1"), QStringLiteral("Sub One"));
    QVERIFY(v.hasSubContainment(QStringLiteral("sub1")));
    QVERIFY(!v.hasSubContainment(QStringLiteral("nope")));
}

void DataTypesTest::screen_serializeRoundTrip()
{
    Data::Screen s;
    s.name = QStringLiteral("DP-1");
    s.geometry = QRect(10, 20, 1280, 720);

    QString serialized = s.serialize();
    QCOMPARE(serialized, QStringLiteral("DP-1:::10,20 1280x720"));

    // init() round-trips the serialized form back through the ::: splitter
    Data::Screen back;
    back.init(QStringLiteral("3"), serialized);
    QCOMPARE(back.id, QStringLiteral("3"));
    QCOMPARE(back.name, QStringLiteral("DP-1"));
    QCOMPARE(back.geometry, QRect(10, 20, 1280, 720));

    // the (id, serialized) ctor delegates to init()
    Data::Screen ctor(QStringLiteral("5"), serialized);
    QCOMPARE(ctor.id, QStringLiteral("5"));
    QCOMPARE(ctor.name, QStringLiteral("DP-1"));
    QCOMPARE(ctor.geometry, QRect(10, 20, 1280, 720));

    // equality excludes isActive/isRemovable (state, not data)
    Data::Screen eq = back;
    eq.isActive = true;
    eq.isRemovable = true;
    QVERIFY(eq == back);
    eq.geometry = QRect(0, 0, 1, 1);
    QVERIFY(eq != back);
}

void DataTypesTest::screen_isScreensGroup()
{
    Data::Screen all;
    all.id = QString::number(Data::Screen::ONALLSCREENSID);
    QVERIFY(all.isScreensGroup());

    Data::Screen allSecondary;
    allSecondary.id = QString::number(Data::Screen::ONALLSECONDARYSCREENSID);
    QVERIFY(allSecondary.isScreensGroup());

    Data::Screen primary;
    primary.id = QString::number(Data::Screen::ONPRIMARYID);
    QVERIFY(!primary.isScreensGroup());

    Data::Screen ordinary;
    ordinary.id = QStringLiteral("3");
    QVERIFY(!ordinary.isScreensGroup());
}

void DataTypesTest::viewsTable_hasContainmentIdRecursion()
{
    Data::ViewsTable views;

    Data::View v1(QStringLiteral("100"), QStringLiteral("View A"));
    v1.subcontainments << Data::Generic(QStringLiteral("201"), QStringLiteral("Sub of A"));
    Data::View v2(QStringLiteral("101"), QStringLiteral("View B"));

    views << v1 << v2;

    // top-level id
    QVERIFY(views.hasContainmentId(QStringLiteral("100")));
    QVERIFY(views.hasContainmentId(QStringLiteral("101")));
    // nested subcontainment id (the recursion this method exists for)
    QVERIFY(views.hasContainmentId(QStringLiteral("201")));
    // absent
    QVERIFY(!views.hasContainmentId(QStringLiteral("999")));
}

void DataTypesTest::viewsTable_subtractedAndOnlyOriginals()
{
    Data::ViewsTable lhs;
    lhs << Data::View(QStringLiteral("a"), QStringLiteral("A"))
        << Data::View(QStringLiteral("b"), QStringLiteral("B"))
        << Data::View(QStringLiteral("c"), QStringLiteral("C"));

    Data::ViewsTable rhs;
    rhs << Data::View(QStringLiteral("a"), QStringLiteral("A"))
        << Data::View(QStringLiteral("c"), QStringLiteral("C"));

    Data::ViewsTable diff = lhs.subtracted(rhs);
    QCOMPARE(diff.rowCount(), 1);
    QVERIFY(diff.containsId(QStringLiteral("b")));

    // identical tables subtract to empty
    Data::ViewsTable same = lhs.subtracted(lhs);
    QVERIFY(same.isEmpty());

    // onlyOriginals filters out cloned views
    Data::View original(QStringLiteral("o"), QStringLiteral("Original"));
    Data::View cloned(QStringLiteral("cl"), QStringLiteral("Cloned"));
    cloned.isClonedFrom = 5;             // makes isOriginal() false
    Data::ViewsTable mixed;
    mixed << original << cloned;
    Data::ViewsTable origs = mixed.onlyOriginals();
    QCOMPARE(origs.rowCount(), 1);
    QVERIFY(origs.containsId(QStringLiteral("o")));
    QVERIFY(!origs.containsId(QStringLiteral("cl")));
}

void DataTypesTest::viewsTable_appendTemporaryView()
{
    Data::ViewsTable views;
    Data::View v(QStringLiteral("real"), QStringLiteral("Real"));

    views.appendTemporaryView(v);
    QCOMPARE(views.rowCount(), 1);
    QCOMPARE(views[uint(0)].id, QStringLiteral("temp:1"));

    views.appendTemporaryView(v);
    QCOMPARE(views.rowCount(), 2);
    QCOMPARE(views[uint(1)].id, QStringLiteral("temp:2"));
}

void DataTypesTest::layoutsTable_subtractedAndFreeActivities()
{
    Data::LayoutsTable lhs;
    Data::Layout a; a.id = QStringLiteral("a"); a.name = QStringLiteral("A");
    Data::Layout b; b.id = QStringLiteral("b"); b.name = QStringLiteral("B");
    Data::Layout c; c.id = QStringLiteral("c"); c.name = QStringLiteral("C");
    lhs << a << b << c;

    Data::LayoutsTable rhs;
    rhs << a << c;

    Data::LayoutsTable diff = lhs.subtracted(rhs);
    QCOMPARE(diff.rowCount(), 1);
    QVERIFY(diff.containsId(QStringLiteral("b")));

    // setLayoutForFreeActivities writes the free-activities sentinel into the matched row
    lhs.setLayoutForFreeActivities(QStringLiteral("b"));
    QVERIFY(lhs[QStringLiteral("b")].isForFreeActivities());
    // unmatched id is a no-op (no crash, others untouched)
    lhs.setLayoutForFreeActivities(QStringLiteral("missing"));
    QVERIFY(lhs[QStringLiteral("a")].activities.isEmpty());
}

QTEST_GUILESS_MAIN(DataTypesTest)
#include "datatypestest.moc"
