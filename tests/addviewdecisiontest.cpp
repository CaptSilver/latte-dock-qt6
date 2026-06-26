/*
    SPDX-FileCopyrightText: 2026 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtTest>

#include "../app/layout/addviewdecision.h"

using namespace Latte;
using namespace Latte::Layout;

static AddViewInputs base()
{
    AddViewInputs in;
    in.onPrimary = true;
    in.screenId = 0;
    in.screenIdValid = true;
    in.screenActive = true;
    in.visibilityMode = Types::DodgeActive;
    in.configByPassWM = false;
    return in;
}

class AddViewDecisionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void primaryUsesPrimaryScreenNoReject();
    void nonPrimaryActiveScreenUsesExplicit();
    void nonPrimaryInactiveScreenIsRejected();
    void nonPrimaryInvalidScreenUsesPrimary();
    void alwaysVisibleForcesByPassWmFalse();
    void dodgeActiveHonoursConfigByPassWm();
};

void AddViewDecisionTest::primaryUsesPrimaryScreenNoReject()
{
    const AddViewDecision d = AddViewDecisionMaker::decide(base());
    QVERIFY(!d.reject);
    QVERIFY(!d.useExplicitScreen);
}

void AddViewDecisionTest::nonPrimaryActiveScreenUsesExplicit()
{
    AddViewInputs in = base();
    in.onPrimary = false;
    in.screenId = 3;
    in.screenActive = true;
    const AddViewDecision d = AddViewDecisionMaker::decide(in);
    QVERIFY(!d.reject);
    QVERIFY(d.useExplicitScreen);
}

void AddViewDecisionTest::nonPrimaryInactiveScreenIsRejected()
{
    AddViewInputs in = base();
    in.onPrimary = false;
    in.screenId = 3;
    in.screenActive = false;
    const AddViewDecision d = AddViewDecisionMaker::decide(in);
    QVERIFY(d.reject);
}

void AddViewDecisionTest::nonPrimaryInvalidScreenUsesPrimary()
{
    AddViewInputs in = base();
    in.onPrimary = false;
    in.screenIdValid = false;       // invalid screen id -> falls through to primary
    in.screenActive = false;
    const AddViewDecision d = AddViewDecisionMaker::decide(in);
    QVERIFY(!d.reject);
    QVERIFY(!d.useExplicitScreen);
}

void AddViewDecisionTest::alwaysVisibleForcesByPassWmFalse()
{
    AddViewInputs in = base();
    in.visibilityMode = Types::AlwaysVisible;
    in.configByPassWM = true;       // overridden to false for always-visible family
    const AddViewDecision d = AddViewDecisionMaker::decide(in);
    QVERIFY(!d.byPassWM);
}

void AddViewDecisionTest::dodgeActiveHonoursConfigByPassWm()
{
    AddViewInputs in = base();
    in.visibilityMode = Types::DodgeActive;
    in.configByPassWM = true;
    const AddViewDecision d = AddViewDecisionMaker::decide(in);
    QVERIFY(d.byPassWM);
}

QTEST_MAIN(AddViewDecisionTest)
#include "addviewdecisiontest.moc"
