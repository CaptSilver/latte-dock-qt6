#include "covself.h"
#include <QtTest>

class CovSelfTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void exercisesOnlyCovered()
    {
        QCOMPARE(covselfCovered(4), 6); // 0+1+2+3
        // covselfUncovered() is deliberately never called.
    }
};

QTEST_GUILESS_MAIN(CovSelfTest)
#include "covselftest.moc"
