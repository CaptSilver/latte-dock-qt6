/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../app/view/tasksmodel.h"
#include "../app/view/indicator/indicatorinfo.h"

#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QVariant>

#include <PlasmaQuick/AppletQuickItem>

using Latte::ViewPart::TasksModel;
using Info = Latte::ViewPart::IndicatorPart::Info;

class ViewModelsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // TasksModel
    void tasksModel_emptyState();
    void tasksModel_roleNames();
    void tasksModel_addEmitsInsertAndCount();
    void tasksModel_addDedupesSamePlasmoid();
    void tasksModel_dataReturnsPlasmoidAtUserRole();
    void tasksModel_dataOutOfRangeIsInvalid();
    void tasksModel_dataNonUserRoleIsInvalid();
    void tasksModel_removeEmitsRemoveAndCount();
    void tasksModel_removeUnknownIsNoOp();
    void tasksModel_removeNullIsNoOp();

    // IndicatorPart::Info
    void info_defaults();
    void info_boolPropertiesSetGetAndSignal();
    void info_intPropertySetGetAndSignal();
    void info_floatPropertiesSetGetAndSignal();
    void info_signalFiresOnlyOnRealChange();
};

// ---------- TasksModel ----------

void ViewModelsTest::tasksModel_emptyState()
{
    TasksModel model;
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.rowCount(), 0);
}

void ViewModelsTest::tasksModel_roleNames()
{
    TasksModel model;
    const auto roles = model.roleNames();
    QCOMPARE(roles.value(Qt::UserRole), QByteArrayLiteral("tasks"));
}

void ViewModelsTest::tasksModel_addEmitsInsertAndCount()
{
    TasksModel model;
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy countSpy(&model, &TasksModel::countChanged);

    PlasmaQuick::AppletQuickItem item;
    model.addTask(&item);

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);

    // The inserted range is [0, 0] for the first element.
    const auto args = insertSpy.takeFirst();
    QCOMPARE(args.at(1).toInt(), 0);
    QCOMPARE(args.at(2).toInt(), 0);
}

void ViewModelsTest::tasksModel_addDedupesSamePlasmoid()
{
    TasksModel model;
    PlasmaQuick::AppletQuickItem item;

    model.addTask(&item);
    QCOMPARE(model.count(), 1);

    QSignalSpy countSpy(&model, &TasksModel::countChanged);
    // Adding the same plasmoid again is rejected by the contains() guard.
    model.addTask(&item);
    QCOMPARE(model.count(), 1);
    QCOMPARE(countSpy.count(), 0);
}

void ViewModelsTest::tasksModel_dataReturnsPlasmoidAtUserRole()
{
    TasksModel model;
    PlasmaQuick::AppletQuickItem first;
    PlasmaQuick::AppletQuickItem second;
    model.addTask(&first);
    model.addTask(&second);

    QCOMPARE(model.count(), 2);

    auto *got0 = model.data(model.index(0, 0), Qt::UserRole).value<PlasmaQuick::AppletQuickItem *>();
    auto *got1 = model.data(model.index(1, 0), Qt::UserRole).value<PlasmaQuick::AppletQuickItem *>();
    QCOMPARE(got0, &first);
    QCOMPARE(got1, &second);
}

void ViewModelsTest::tasksModel_dataOutOfRangeIsInvalid()
{
    TasksModel model;
    PlasmaQuick::AppletQuickItem item;
    model.addTask(&item);

    // Row past the end yields an invalid QVariant (the rowIsValid guard).
    QVERIFY(!model.data(model.index(5, 0), Qt::UserRole).isValid());
    QVERIFY(!model.data(model.index(-1, 0), Qt::UserRole).isValid());
}

void ViewModelsTest::tasksModel_dataNonUserRoleIsInvalid()
{
    TasksModel model;
    PlasmaQuick::AppletQuickItem item;
    model.addTask(&item);

    // Only Qt::UserRole returns a value; DisplayRole falls through to QVariant().
    QVERIFY(!model.data(model.index(0, 0), Qt::DisplayRole).isValid());
}

void ViewModelsTest::tasksModel_removeEmitsRemoveAndCount()
{
    TasksModel model;
    PlasmaQuick::AppletQuickItem first;
    PlasmaQuick::AppletQuickItem second;
    model.addTask(&first);
    model.addTask(&second);
    QCOMPARE(model.count(), 2);

    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy countSpy(&model, &TasksModel::countChanged);

    model.removeTask(&first);

    QCOMPARE(model.count(), 1);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);

    // first was at index 0, so the removed range is [0, 0].
    const auto args = removeSpy.takeFirst();
    QCOMPARE(args.at(1).toInt(), 0);
    QCOMPARE(args.at(2).toInt(), 0);

    // The survivor shifts into row 0.
    auto *survivor = model.data(model.index(0, 0), Qt::UserRole).value<PlasmaQuick::AppletQuickItem *>();
    QCOMPARE(survivor, &second);
}

void ViewModelsTest::tasksModel_removeUnknownIsNoOp()
{
    TasksModel model;
    PlasmaQuick::AppletQuickItem known;
    PlasmaQuick::AppletQuickItem stranger;
    model.addTask(&known);

    QSignalSpy countSpy(&model, &TasksModel::countChanged);
    // Removing a plasmoid the model never tracked is rejected by the contains() guard.
    model.removeTask(&stranger);
    QCOMPARE(model.count(), 1);
    QCOMPARE(countSpy.count(), 0);
}

void ViewModelsTest::tasksModel_removeNullIsNoOp()
{
    TasksModel model;
    PlasmaQuick::AppletQuickItem item;
    model.addTask(&item);

    QSignalSpy countSpy(&model, &TasksModel::countChanged);
    // The leading !plasmoid guard short-circuits before any list touch.
    model.removeTask(nullptr);
    QCOMPARE(model.count(), 1);
    QCOMPARE(countSpy.count(), 0);
}

// ---------- IndicatorPart::Info ----------

void ViewModelsTest::info_defaults()
{
    Info info(nullptr);

    QCOMPARE(info.needsIconColors(), false);
    QCOMPARE(info.needsMouseEventCoordinates(), false);
    QCOMPARE(info.providesClickedAnimation(), false);
    QCOMPARE(info.providesHoveredAnimation(), false);
    QCOMPARE(info.providesInAttentionAnimation(), false);
    QCOMPARE(info.providesTaskLauncherAnimation(), false);
    QCOMPARE(info.providesGroupedWindowAddedAnimation(), false);
    QCOMPARE(info.providesGroupedWindowRemovedAnimation(), false);
    QCOMPARE(info.providesFrontLayer(), false);
    QCOMPARE(info.extraMaskThickness(), 0);
    QCOMPARE(info.minLengthPadding(), 0.0f);
    QCOMPARE(info.minThicknessPadding(), 0.0f);
}

void ViewModelsTest::info_boolPropertiesSetGetAndSignal()
{
    Info info(nullptr);

    struct BoolProp {
        const char *signal;
        std::function<void(Info &, bool)> setter;
        std::function<bool(const Info &)> getter;
    };

    const std::vector<BoolProp> props = {
        {SIGNAL(needsIconColorsChanged()), [](Info &i, bool v) { i.setNeedsIconColors(v); }, [](const Info &i) { return i.needsIconColors(); }},
        {SIGNAL(needsMouseEventCoordinatesChanged()), [](Info &i, bool v) { i.setNeedsMouseEventCoordinates(v); }, [](const Info &i) { return i.needsMouseEventCoordinates(); }},
        {SIGNAL(providesClickedAnimationChanged()), [](Info &i, bool v) { i.setProvidesClickedAnimation(v); }, [](const Info &i) { return i.providesClickedAnimation(); }},
        {SIGNAL(providesHoveredAnimationChanged()), [](Info &i, bool v) { i.setProvidesHoveredAnimation(v); }, [](const Info &i) { return i.providesHoveredAnimation(); }},
        {SIGNAL(providesInAttentionAnimationChanged()), [](Info &i, bool v) { i.setProvidesInAttentionAnimation(v); }, [](const Info &i) { return i.providesInAttentionAnimation(); }},
        {SIGNAL(providesTaskLauncherAnimationChanged()), [](Info &i, bool v) { i.setProvidesTaskLauncherAnimation(v); }, [](const Info &i) { return i.providesTaskLauncherAnimation(); }},
        {SIGNAL(providesGroupedWindowAddedAnimationChanged()), [](Info &i, bool v) { i.setProvidesGroupedWindowAddedAnimation(v); }, [](const Info &i) { return i.providesGroupedWindowAddedAnimation(); }},
        {SIGNAL(providesGroupedWindowRemovedAnimationChanged()), [](Info &i, bool v) { i.setProvidesGroupedWindowRemovedAnimation(v); }, [](const Info &i) { return i.providesGroupedWindowRemovedAnimation(); }},
        {SIGNAL(providesFrontLayerChanged()), [](Info &i, bool v) { i.setProvidesFrontLayer(v); }, [](const Info &i) { return i.providesFrontLayer(); }},
    };

    for (const auto &p : props) {
        QSignalSpy spy(&info, p.signal);
        QVERIFY(spy.isValid());
        QCOMPARE(p.getter(info), false);
        p.setter(info, true);
        QCOMPARE(p.getter(info), true);
        QCOMPARE(spy.count(), 1);
    }
}

void ViewModelsTest::info_intPropertySetGetAndSignal()
{
    Info info(nullptr);
    QSignalSpy spy(&info, &Info::extraMaskThicknessChanged);
    QVERIFY(spy.isValid());

    info.setExtraMaskThickness(7);
    QCOMPARE(info.extraMaskThickness(), 7);
    QCOMPARE(spy.count(), 1);
}

void ViewModelsTest::info_floatPropertiesSetGetAndSignal()
{
    Info info(nullptr);

    QSignalSpy lengthSpy(&info, &Info::minLengthPaddingChanged);
    QSignalSpy thicknessSpy(&info, &Info::minThicknessPaddingChanged);
    QVERIFY(lengthSpy.isValid());
    QVERIFY(thicknessSpy.isValid());

    info.setMinLengthPadding(0.25f);
    QCOMPARE(info.minLengthPadding(), 0.25f);
    QCOMPARE(lengthSpy.count(), 1);

    info.setMinThicknessPadding(0.5f);
    QCOMPARE(info.minThicknessPadding(), 0.5f);
    QCOMPARE(thicknessSpy.count(), 1);
}

void ViewModelsTest::info_signalFiresOnlyOnRealChange()
{
    Info info(nullptr);

    // bool: set to current value -> equality guard suppresses the signal.
    QSignalSpy boolSpy(&info, &Info::needsIconColorsChanged);
    info.setNeedsIconColors(false); // already false
    QCOMPARE(boolSpy.count(), 0);
    info.setNeedsIconColors(true);
    QCOMPARE(boolSpy.count(), 1);
    info.setNeedsIconColors(true); // no change
    QCOMPARE(boolSpy.count(), 1);

    // int: idempotent set is suppressed.
    QSignalSpy intSpy(&info, &Info::extraMaskThicknessChanged);
    info.setExtraMaskThickness(0); // already 0
    QCOMPARE(intSpy.count(), 0);
    info.setExtraMaskThickness(3);
    QCOMPARE(intSpy.count(), 1);
    info.setExtraMaskThickness(3); // no change
    QCOMPARE(intSpy.count(), 1);

    // float: idempotent set is suppressed.
    QSignalSpy floatSpy(&info, &Info::minLengthPaddingChanged);
    info.setMinLengthPadding(0.0f); // already 0
    QCOMPARE(floatSpy.count(), 0);
    info.setMinLengthPadding(1.0f);
    QCOMPARE(floatSpy.count(), 1);
    info.setMinLengthPadding(1.0f); // no change
    QCOMPARE(floatSpy.count(), 1);
}

QTEST_MAIN(ViewModelsTest)
#include "viewmodelstest.moc"
