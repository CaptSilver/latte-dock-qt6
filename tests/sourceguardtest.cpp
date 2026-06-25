/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Source-level guards for three one-token correctness fixes that have no feasible
// headless behavioral repro: each lives behind the full View / Corona / settings
// graph and cannot be constructed offscreen. Mirrors bindingrestoremodetest --
// read the real source via REPO_ROOT, extract the function body by brace match,
// and assert the fixed form so the typo / empty-guard cannot silently return:
//   * VisibilityManager::updateSidebarState   '==' typo for '=' (state never set)
//   * Layouts::modeIsChanged                  missing '>' -> pointer arithmetic +
//                                             infinite self-recursion
//   * ContainmentInterface::updateContainmentConfigProperty  empty guard body
//                                             falls through to a null deref

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QtTest>

class SourceGuardTest : public QObject
{
    Q_OBJECT

private:
    static QString readFile(const QString &rel)
    {
        QFile f(QStringLiteral("%1/%2").arg(QStringLiteral(REPO_ROOT), rel));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }
        return QString::fromUtf8(f.readAll());
    }

    // Brace-matched body (including the outer braces) of the first `sig { ... }`.
    static QString functionBody(const QString &src, const QString &sig)
    {
        const int s = src.indexOf(sig);
        if (s == -1) {
            return QString();
        }
        const int brace = src.indexOf(QLatin1Char('{'), s + sig.size());
        if (brace == -1) {
            return QString();
        }
        int depth = 0;
        int i = brace;
        for (; i < src.size(); ++i) {
            if (src.at(i) == QLatin1Char('{')) {
                ++depth;
            } else if (src.at(i) == QLatin1Char('}') && --depth == 0) {
                ++i;
                break;
            }
        }
        return src.mid(brace, i - brace);
    }

    static QString stripped(const QString &body)
    {
        QString s = body;
        s.remove(QRegularExpression(QStringLiteral("\\s+")));
        return s;
    }

private Q_SLOTS:
    void visibilityManager_updateSidebarState_assignsState();
    void layoutsController_modeIsChanged_delegatesToModel();
    void containmentInterface_updateContainmentConfigProperty_guardReturns();
};

void SourceGuardTest::visibilityManager_updateSidebarState_assignsState()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/view/visibilitymanager.cpp")),
                                            QStringLiteral("void VisibilityManager::updateSidebarState()")));
    QVERIFY2(!s.isEmpty(), "updateSidebarState() not found");
    // Must ASSIGN the freshly computed state before emitting, not compare-and-discard.
    QVERIFY2(s.contains(QStringLiteral("m_isSidebar=cursidebarstate;")),
             "updateSidebarState must assign m_isSidebar (single '='), not compare it");
    QVERIFY2(!s.contains(QStringLiteral("m_isSidebar==cursidebarstate;")),
             "updateSidebarState has a discarded '==' comparison statement");
}

void SourceGuardTest::layoutsController_modeIsChanged_delegatesToModel()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/settings/settingsdialog/layoutscontroller.cpp")),
                                            QStringLiteral("bool Layouts::modeIsChanged() const")));
    QVERIFY2(!s.isEmpty(), "Layouts::modeIsChanged() not found");
    QVERIFY2(s.contains(QStringLiteral("m_model->modeIsChanged()")),
             "modeIsChanged must delegate via m_model->modeIsChanged()");
    QVERIFY2(!s.contains(QStringLiteral("m_model-modeIsChanged")),
             "modeIsChanged has the missing-'>' pointer-arithmetic / self-recursion typo");
}

void SourceGuardTest::containmentInterface_updateContainmentConfigProperty_guardReturns()
{
    const QString s = stripped(functionBody(readFile(QStringLiteral("app/view/containmentinterface.cpp")),
                                            QStringLiteral("void ContainmentInterface::updateContainmentConfigProperty")));
    QVERIFY2(!s.isEmpty(), "updateContainmentConfigProperty() not found");
    // The null/missing-key guard must early-return instead of an empty body that
    // falls through to dereferencing a possibly-null m_configuration.
    QVERIFY2(s.contains(QStringLiteral("contains(key)){return;")),
             "updateContainmentConfigProperty guard must early-return on a null/absent config");
}

QTEST_GUILESS_MAIN(SourceGuardTest)

#include "sourceguardtest.moc"
