/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Guards for the shell entry points that used to hardcode a build tree. Each of these
// scripts resolved a path that was only correct for one particular checkout layout, and
// each failed in a way that looked like success:
//   * uninstall.sh          read build/install_manifest.txt and exited 0 when it was
//                           absent, so an uninstall silently removed nothing
//   * tests/e2e/run.sh      pinned $REPO/build with no override, exercising whatever
//                           binaries happened to be in that tree
//   * tests/sceneprobe/run.sh  fell back from the ASan probe to an uninstrumented one
//                           without saying so, then still printed a passing verdict
//   * tests/coverage/qml_coverage.sh  deleted the staged tree before the step that
//                           rebuilds it, so a failure there left nothing behind
// These live in shell, so there is no symbol to link against -- read the real scripts
// through REPO_ROOT and assert the shape of the fix, the way sourceguardtest does for
// one-token C++ fixes.

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>

class ScriptGuardTest : public QObject
{
    Q_OBJECT

private:
    static QString repoPath(const QString &rel)
    {
        return QStringLiteral("%1/%2").arg(QStringLiteral(REPO_ROOT), rel);
    }

    static QString readScript(const QString &rel)
    {
        QFile f(repoPath(rel));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }
        return QString::fromUtf8(f.readAll());
    }

    // Offset of the first line matching `needle`, or -1. Used to assert ordering
    // between two steps of a script.
    static int lineOffset(const QString &src, const QString &needle)
    {
        return src.indexOf(needle);
    }

private Q_SLOTS:
    void uninstall_doesNotReadTheStaleBuildManifest();
    void uninstall_failsLoudlyWhenTheManifestIsMissing();
    void e2eRunner_honoursABuildDirOverride();
    void manualRunners_doNotDefaultToTheStaleBuildTree();
    void sceneprobeRunner_stagesFromTheSelectedBuildDir();
    void sceneprobeRunner_announcesAnUninstrumentedFallback();
    void qmlCoverage_honoursACoverageBuildOverride();
    void qmlCoverage_stagesBeforeDestroyingThePreviousStage();
    void buildScripts_configureWithTheMandatoryQtPathsFlag();
};

void ScriptGuardTest::uninstall_doesNotReadTheStaleBuildManifest()
{
    const QString src = readScript(QStringLiteral("uninstall.sh"));
    QVERIFY2(!src.isEmpty(), "uninstall.sh unreadable");

    // The manifest belongs to whichever tree ran the install; hardcoding build/ meant an
    // in-source build wrote install_manifest.txt at the repo root and the script removed
    // an older list from a stale out-of-source tree.
    QVERIFY2(!src.contains(QStringLiteral("build/install_manifest.txt")),
             "uninstall.sh still hardcodes the build/ manifest");
    QVERIFY2(src.contains(QStringLiteral("MANIFEST")),
             "uninstall.sh should resolve the manifest through an overridable variable");
}

void ScriptGuardTest::uninstall_failsLoudlyWhenTheManifestIsMissing()
{
    // Point the script at a manifest that cannot exist, so the removal branch is never
    // reached and nothing is passed to rm. A missing manifest must be an error: exiting 0
    // told the caller the uninstall had succeeded.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("MANIFEST"), tmp.filePath(QStringLiteral("absent_manifest.txt")));

    QProcess p;
    p.setProcessEnvironment(env);
    p.setWorkingDirectory(tmp.path());
    p.start(QStringLiteral("bash"), {repoPath(QStringLiteral("uninstall.sh"))});
    QVERIFY2(p.waitForFinished(30000), "uninstall.sh did not terminate");

    QCOMPARE(p.exitStatus(), QProcess::NormalExit);
    QVERIFY2(p.exitCode() != 0, "uninstall.sh exited 0 with no manifest -- a silent no-op");
}

void ScriptGuardTest::e2eRunner_honoursABuildDirOverride()
{
    const QString src = readScript(QStringLiteral("tests/e2e/run.sh"));
    QVERIFY2(!src.isEmpty(), "tests/e2e/run.sh unreadable");

    QVERIFY2(!src.contains(QStringLiteral("BUILD=\"$REPO/build\"")),
             "tests/e2e/run.sh still pins $REPO/build with no override");
    QVERIFY2(src.contains(QStringLiteral("BUILD=\"${BUILD:-")),
             "tests/e2e/run.sh should take BUILD from the environment");
}

void ScriptGuardTest::sceneprobeRunner_stagesFromTheSelectedBuildDir()
{
    const QString src = readScript(QStringLiteral("tests/sceneprobe/run.sh"));
    QVERIFY2(!src.isEmpty(), "tests/sceneprobe/run.sh unreadable");

    // Staging from a different tree than the probe came from is how the gate ended up
    // rendering QML from a tree nobody had built in two months.
    QVERIFY2(!src.contains(QStringLiteral("STAGE_BUILD=\"$REPO/build\"")),
             "tests/sceneprobe/run.sh still stages from a hardcoded $REPO/build");
    QVERIFY2(src.contains(QStringLiteral("STAGE_BUILD=\"${STAGE_BUILD:-")),
             "tests/sceneprobe/run.sh should take STAGE_BUILD from the environment");
}

void ScriptGuardTest::sceneprobeRunner_announcesAnUninstrumentedFallback()
{
    const QString src = readScript(QStringLiteral("tests/sceneprobe/run.sh"));
    QVERIFY2(!src.isEmpty(), "tests/sceneprobe/run.sh unreadable");

    // The fallback is legitimate -- build-asan is optional -- but it has to be visible,
    // because ASAN_OPTIONS stays exported and the run still reports PASS.
    const int fallback = lineOffset(src, QStringLiteral("latte-sceneprobe\" ]"));
    QVERIFY2(fallback != -1, "could not find the probe-presence check");
    const QString tail = src.mid(fallback, 400);
    QVERIFY2(tail.contains(QStringLiteral("ASan")) || tail.contains(QStringLiteral("asan")),
             "the fallback to an uninstrumented probe is silent");
}

void ScriptGuardTest::qmlCoverage_honoursACoverageBuildOverride()
{
    const QString src = readScript(QStringLiteral("tests/coverage/qml_coverage.sh"));
    QVERIFY2(!src.isEmpty(), "tests/coverage/qml_coverage.sh unreadable");

    // Every other path in this script is overridable; this one was not, which tied the
    // QML stage to a clang tree that only cxx_coverage.sh knows how to build.
    QVERIFY2(src.contains(QStringLiteral("COV_BUILD=\"${COV_BUILD:-")),
             "qml_coverage.sh should take the staging build dir from the environment");
}

void ScriptGuardTest::qmlCoverage_stagesBeforeDestroyingThePreviousStage()
{
    const QString src = readScript(QStringLiteral("tests/coverage/qml_coverage.sh"));
    QVERIFY2(!src.isEmpty(), "tests/coverage/qml_coverage.sh unreadable");

    const int destroy = lineOffset(src, QStringLiteral("rm -rf \"$STAGE\""));
    const int build = lineOffset(src, QStringLiteral("DESTDIR=\"$STAGE_NEW\""));
    QVERIFY2(build != -1, "qml_coverage.sh should install into a scratch stage first");
    QVERIFY2(destroy == -1 || build < destroy,
             "qml_coverage.sh destroys the stage before the step that rebuilds it");
}

void ScriptGuardTest::manualRunners_doNotDefaultToTheStaleBuildTree()
{
    // ctest passes BUILD explicitly, so these defaults only ever bite someone running the
    // script by hand -- which is exactly when a two-month-old tree is hardest to notice.
    for (const QString &rel : {QStringLiteral("tests/manual/qml_load_compile.sh"),
                               QStringLiteral("tests/manual/qml_interaction_test.sh"),
                               QStringLiteral("tests/manual/qml_pkg_test.sh")}) {
        const QString src = readScript(rel);
        QVERIFY2(!src.isEmpty(), qPrintable(rel + QStringLiteral(" unreadable")));
        QVERIFY2(!src.contains(QStringLiteral("${BUILD:-$REPO/build}")),
                 qPrintable(rel + QStringLiteral(" still defaults to the stale $REPO/build")));
    }
}

void ScriptGuardTest::buildScripts_configureWithTheMandatoryQtPathsFlag()
{
    // README calls -DKDE_INSTALL_USE_QT_SYS_PATHS=ON mandatory, and it is: without it the QML
    // packages land where Plasma does not look and the dock comes up with no widgets. A build
    // script that omits it produces an install that looks successful and does not work, which
    // is why install.sh was worse than having no script at all.
    static const QRegularExpression configureLine(QStringLiteral("^[ \\t]*(?:sudo[ \\t]+)?cmake\\b.*-DCMAKE_.*$"),
                                                  QRegularExpression::MultilineOption);

    const QStringList scripts = QDir(QStringLiteral(REPO_ROOT)).entryList(QStringList() << QStringLiteral("*.sh"), QDir::Files);
    QVERIFY2(!scripts.isEmpty(), "no top-level shell scripts found, REPO_ROOT is wrong");

    for (const QString &rel : scripts) {
        const QString src = readScript(rel);
        QVERIFY2(!src.isEmpty(), qPrintable(rel + QStringLiteral(" unreadable")));

        QRegularExpressionMatchIterator it = configureLine.globalMatch(src);
        while (it.hasNext()) {
            const QString line = it.next().captured().trimmed();
            QVERIFY2(line.contains(QStringLiteral("KDE_INSTALL_USE_QT_SYS_PATHS")),
                     qPrintable(QStringLiteral("%1 configures without KDE_INSTALL_USE_QT_SYS_PATHS: %2").arg(rel, line)));
        }
    }
}

QTEST_MAIN(ScriptGuardTest)
#include "scriptguardtest.moc"
