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
//
// The nested-kwin guards below are the same idea applied to a copy that had been made three
// times. The render, e2e and coverage harnesses each hand-rolled the same kwin_wayland
// incantation, and the copies had already drifted apart: all three resolved a Vulkan ICD
// manifest by glob order, which picks the 32-bit one and leaves a 64-bit process with no
// driver at all. That killed the render gate outright while the other two only got away with
// it because their process never asked for Vulkan.

#include "sourcereader.h"

#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <tuple>

using namespace LatteTest;

class ScriptGuardTest : public QObject
{
    Q_OBJECT

private:
    // Offset of the first line matching `needle`, or -1. Used to assert ordering
    // between two steps of a script.
    static int lineOffset(const QString &src, const QString &needle)
    {
        return src.indexOf(needle);
    }

    //! Repo-relative path of every *.sh under tests/, sorted. The "defined once" guards have to
    //! see the whole tree: a hand-listed set is exactly what a fourth copy would slip past.
    static QStringList testShellScripts()
    {
        const QDir repo(QStringLiteral(REPO_ROOT));
        QStringList rel;
        QDirIterator it(repoPath(QStringLiteral("tests")), QStringList() << QStringLiteral("*.sh"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            rel << repo.relativeFilePath(it.next());
        }
        rel.sort();
        return rel;
    }

    //! `src` with its whole-line comments removed. The guards below forbid a token from appearing
    //! outside its one home, and every one of those tokens is also the clearest word to use when
    //! explaining the rule -- matching prose would make the guards punish good comments.
    static QString codeOnly(const QString &src)
    {
        QStringList kept;
        const QStringList lines = src.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (!line.trimmed().startsWith(QLatin1Char('#'))) {
                kept << line;
            }
        }
        return kept.join(QLatin1Char('\n'));
    }

    static QStringList shellScriptsContaining(const QString &needle)
    {
        QStringList hits;
        const QStringList scripts = testShellScripts();
        for (const QString &rel : scripts) {
            if (codeOnly(readRepoFile(rel)).contains(needle)) {
                hits << rel;
            }
        }
        return hits;
    }

    //! Asserts `needle` occurs in exactly `owner` and nowhere else under tests/.
    static void verifyOnlyOwnerHas(const QString &needle, const QString &owner)
    {
        const QStringList hits = shellScriptsContaining(needle);
        QVERIFY2(hits == QStringList{owner},
                 qPrintable(QStringLiteral("`%1` should live only in %2, found it in: %3")
                                .arg(needle, owner, hits.isEmpty() ? QStringLiteral("(nowhere)") : hits.join(QStringLiteral(", ")))));
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
    void nestedKwinLaunchers_pickAnArchMatchedVulkanIcd();
    void nestedKwinLaunchers_shareOneLauncher();
    void vulkanIcd_prefersTheArchMatchedManifest();
    void vulkanIcd_fallsBackToTheUnsuffixedManifest();
    void vulkanIcd_failsLoudlyWithNoManifest();
    void renderGateScenes_pinTheirFontSize();
    void dockCtl_refusesTheRealSessionBus();
    void sandboxHome_overridesInheritedXdgPaths();
    void liveHarnesses_seedTheirHomeThroughTheHelper();
    void nestedKwinLauncher_doesNotForceTheVulkanRhi();
    void nestedKwinLauncher_propagatesTheSessionExitCode_data();
    void nestedKwinLauncher_propagatesTheSessionExitCode();
    void dockCtl_isDefinedOnce();
    void dockCtl_doesNotSwallowMutatingCallErrors();
};

void ScriptGuardTest::uninstall_doesNotReadTheStaleBuildManifest()
{
    const QString src = readRepoFile(QStringLiteral("uninstall.sh"));
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
    const QString src = readRepoFile(QStringLiteral("tests/e2e/run.sh"));
    QVERIFY2(!src.isEmpty(), "tests/e2e/run.sh unreadable");

    QVERIFY2(!src.contains(QStringLiteral("BUILD=\"$REPO/build\"")),
             "tests/e2e/run.sh still pins $REPO/build with no override");
    QVERIFY2(src.contains(QStringLiteral("BUILD=\"${BUILD:-")),
             "tests/e2e/run.sh should take BUILD from the environment");
}

void ScriptGuardTest::sceneprobeRunner_stagesFromTheSelectedBuildDir()
{
    const QString src = readRepoFile(QStringLiteral("tests/sceneprobe/run.sh"));
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
    const QString src = readRepoFile(QStringLiteral("tests/sceneprobe/run.sh"));
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
    const QString src = readRepoFile(QStringLiteral("tests/coverage/qml_coverage.sh"));
    QVERIFY2(!src.isEmpty(), "tests/coverage/qml_coverage.sh unreadable");

    // Every other path in this script is overridable; this one was not, which tied the
    // QML stage to a clang tree that only cxx_coverage.sh knows how to build.
    QVERIFY2(src.contains(QStringLiteral("COV_BUILD=\"${COV_BUILD:-")),
             "qml_coverage.sh should take the staging build dir from the environment");
}

void ScriptGuardTest::qmlCoverage_stagesBeforeDestroyingThePreviousStage()
{
    const QString src = readRepoFile(QStringLiteral("tests/coverage/qml_coverage.sh"));
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
        const QString src = readRepoFile(rel);
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
        const QString src = readRepoFile(rel);
        QVERIFY2(!src.isEmpty(), qPrintable(rel + QStringLiteral(" unreadable")));

        QRegularExpressionMatchIterator it = configureLine.globalMatch(src);
        while (it.hasNext()) {
            const QString line = it.next().captured().trimmed();
            QVERIFY2(line.contains(QStringLiteral("KDE_INSTALL_USE_QT_SYS_PATHS")),
                     qPrintable(QStringLiteral("%1 configures without KDE_INSTALL_USE_QT_SYS_PATHS: %2").arg(rel, line)));
        }
    }
}

void ScriptGuardTest::nestedKwinLaunchers_pickAnArchMatchedVulkanIcd()
{
    // `ls lvp_icd.*.json | head -1` sorts i686 ahead of x86_64 and Mesa ships both, so the glob
    // handed a 64-bit process the 32-bit manifest. That is not a missing-file error the script
    // could catch: the loader finds the manifest, finds no usable driver behind it, and
    // vkCreateInstance returns ERROR_INCOMPATIBLE_DRIVER -- "QVulkanInstance::create failed
    // (err -9)", with the whole render gate down and nothing pointing at the ICD.
    const QStringList globbers = shellScriptsContaining(QStringLiteral("_icd.*.json"));
    QVERIFY2(globbers.isEmpty(),
             qPrintable(QStringLiteral("these resolve a Vulkan ICD by glob order: %1").arg(globbers.join(QStringLiteral(", ")))));

    const QString helper = readRepoFile(QStringLiteral("tests/lib/nested_kwin.sh"));
    QVERIFY2(!helper.isEmpty(), "tests/lib/nested_kwin.sh unreadable");
    QVERIFY2(helper.contains(QStringLiteral("uname -m")),
             "the shared launcher should match the ICD manifest to the machine architecture");
}

//! Runs vulkan_icd against a seeded directory through the VULKAN_ICD_DIR seam and returns
//! {exitCode, stdout, stderr}. Sourcing is safe: nested_kwin.sh only defines functions.
static std::tuple<int, QString, QString> runVulkanIcd(const QString &dir, const QString &driver)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("VULKAN_ICD_DIR"), dir);

    QProcess p;
    p.setProcessEnvironment(env);
    p.start(QStringLiteral("bash"),
            {QStringLiteral("-c"),
             QStringLiteral(". %1; vulkan_icd %2").arg(repoPath(QStringLiteral("tests/lib/nested_kwin.sh")), driver)});
    if (!p.waitForFinished(30000)) {
        return {-1, QString(), QStringLiteral("vulkan_icd did not terminate")};
    }
    return {p.exitCode(),
            QString::fromUtf8(p.readAllStandardOutput()).trimmed(),
            QString::fromUtf8(p.readAllStandardError()).trimmed()};
}

static void seedIcd(const QString &dir, const QString &name)
{
    QFile f(QStringLiteral("%1/%2").arg(dir, name));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("{}\n");
}

void ScriptGuardTest::sandboxHome_overridesInheritedXdgPaths()
{
    // Exporting HOME is not enough to sandbox a harness. Qt resolves its config through
    // QStandardPaths, which prefers XDG_CONFIG_HOME over $HOME/.config -- and the distrobox
    // exports XDG_CONFIG_HOME=/home/<user>/.config. A harness that seeds only HOME therefore
    // reads and writes the developer's REAL config while believing it is isolated. That is how
    // a test widget ended up in a live Latte layout and crashed the dock.
    //
    // The decoy below is that inherited value: the helper has to override it, not defer to it.
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());
    QTemporaryDir decoy;
    QVERIFY(decoy.isValid());

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &var : {QStringLiteral("XDG_CONFIG_HOME"), QStringLiteral("XDG_DATA_HOME"),
                               QStringLiteral("XDG_CACHE_HOME"), QStringLiteral("XDG_STATE_HOME")}) {
        env.insert(var, decoy.path());
    }

    QProcess p;
    p.setProcessEnvironment(env);
    p.start(QStringLiteral("bash"),
            {QStringLiteral("-c"),
             QStringLiteral(". %1; seed_sandbox_home %2 || exit 3; "
                            "printf 'HOME=%s\\nXDG_CONFIG_HOME=%s\\nXDG_DATA_HOME=%s\\nXDG_CACHE_HOME=%s\\nXDG_STATE_HOME=%s\\n' "
                            "\"$HOME\" \"$XDG_CONFIG_HOME\" \"$XDG_DATA_HOME\" \"$XDG_CACHE_HOME\" \"$XDG_STATE_HOME\"")
                 .arg(repoPath(QStringLiteral("tests/lib/nested_kwin.sh")), sandbox.path())});
    QVERIFY2(p.waitForFinished(30000), "seed_sandbox_home did not terminate");
    const QString out = QString::fromUtf8(p.readAllStandardOutput());
    QCOMPARE(p.exitCode(), 0);

    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QVERIFY2(lines.size() == 5, qPrintable(QStringLiteral("expected five paths, got: %1").arg(out)));

    for (const QString &line : lines) {
        const QString value = line.section(QLatin1Char('='), 1);
        QVERIFY2(value.startsWith(sandbox.path()),
                 qPrintable(QStringLiteral("%1 must live under the sandbox %2, not the inherited %3")
                                .arg(line, sandbox.path(), decoy.path())));
    }
}

void ScriptGuardTest::liveHarnesses_seedTheirHomeThroughTheHelper()
{
    // Getting this right once in a shared helper only holds if the harnesses use it. Seeding
    // HOME by hand is what left the XDG variables pointing at the real config.
    for (const QString &rel : {QStringLiteral("tests/e2e/run.sh"), QStringLiteral("tests/coverage/live_capture.sh")}) {
        const QString src = readRepoFile(rel);
        QVERIFY2(!src.isEmpty(), qPrintable(QStringLiteral("%1 unreadable").arg(rel)));
        QVERIFY2(src.contains(QStringLiteral("seed_sandbox_home")),
                 qPrintable(QStringLiteral("%1 must sandbox through seed_sandbox_home, not a bare HOME export").arg(rel)));
    }
}

void ScriptGuardTest::dockCtl_refusesTheRealSessionBus()
{
    // dctl() drives whatever dock owns org.kde.lattedock on the bus `busctl --user` resolves
    // to. Inside the nested session that is the harness's own dock; outside it, it is the
    // developer's LIVE dock -- and wait_for_dock() cannot tell the difference, because its
    // poll succeeds instantly against the real bus where a dock is always present. A harness
    // that sources this outside the compositor therefore reports success and then drives the
    // user's desktop: saving their layout, opening their settings window. Refuse instead.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("LATTE_NESTED_SESSION"));

    QProcess p;
    p.setProcessEnvironment(env);
    p.start(QStringLiteral("bash"),
            {QStringLiteral("-c"),
             QStringLiteral(". %1; dctl Ping").arg(repoPath(QStringLiteral("tests/lib/dockctl.sh")))});
    QVERIFY2(p.waitForFinished(30000), "dctl did not terminate");

    const QString err = QString::fromUtf8(p.readAllStandardError());
    QVERIFY2(p.exitCode() != 0,
             "dctl ran against the caller's own session bus -- outside the nested compositor that is the live dock");
    QVERIFY2(err.contains(QStringLiteral("LATTE_NESTED_SESSION")),
             qPrintable(QStringLiteral("expected a refusal naming the marker, got: %1").arg(err)));
}

void ScriptGuardTest::renderGateScenes_pinTheirFontSize()
{
    // A render-gate scene is compared pixel-exact on lavapipe (tolerance {0, 0.0}), so any Text
    // that does not set font.pixelSize bakes the HOST's default font size into the reference.
    // That is not a property of the code under test, and it drifts: badgeeffect.qml was blessed
    // when the default resolved to 12px, the rebuilt box resolves it to 13px, and the scene
    // failed with 211 differing pixels while five sibling text scenes -- every one of which pins
    // its size -- kept matching exactly.
    const QString dir = repoPath(QStringLiteral("tests/sceneprobe/scenes"));
    QDirIterator it(dir, QStringList() << QStringLiteral("*.qml"), QDir::Files);

    QStringList offenders;
    int scanned = 0;

    while (it.hasNext()) {
        const QString path = it.next();
        const QString src = readFile(path);
        QVERIFY2(!src.isEmpty(), qPrintable(QStringLiteral("%1 unreadable").arg(path)));
        ++scanned;

        if (!src.contains(QRegularExpression(QStringLiteral("\\bText\\s*\\{")))) {
            continue;
        }
        if (!src.contains(QStringLiteral("font.pixelSize"))) {
            offenders << QFileInfo(path).fileName();
        }
    }

    QVERIFY2(scanned > 0, "no render-gate scenes found");
    QVERIFY2(offenders.isEmpty(),
             qPrintable(QStringLiteral("these scenes render Text without pinning font.pixelSize, so their reference encodes the host's default font: %1")
                            .arg(offenders.join(QStringLiteral(", ")))));
}

void ScriptGuardTest::vulkanIcd_prefersTheArchMatchedManifest()
{
    // The defect this replaced: `ls lvp_icd.*.json | head -1` sorts i686 ahead of x86_64, and Mesa
    // ships both, so a 64-bit process got the 32-bit manifest. The loader then finds no usable
    // driver behind it and vkCreateInstance returns ERROR_INCOMPATIBLE_DRIVER, with nothing in the
    // failure pointing at the ICD. Seed both and require the machine's own architecture to win.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString arch = QSysInfo::currentCpuArchitecture() == QStringLiteral("x86_64")
        ? QStringLiteral("x86_64") : QSysInfo::currentCpuArchitecture();
    seedIcd(tmp.path(), QStringLiteral("lvp_icd.i686.json"));
    seedIcd(tmp.path(), QStringLiteral("lvp_icd.%1.json").arg(arch));
    seedIcd(tmp.path(), QStringLiteral("lvp_icd.json"));

    const auto [code, out, err] = runVulkanIcd(tmp.path(), QStringLiteral("lvp"));
    QCOMPARE(code, 0);
    QCOMPARE(out, QStringLiteral("%1/lvp_icd.%2.json").arg(tmp.path(), arch));
}

void ScriptGuardTest::vulkanIcd_fallsBackToTheUnsuffixedManifest()
{
    // Distros that ship one manifest per driver have no architecture suffix at all.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    seedIcd(tmp.path(), QStringLiteral("lvp_icd.json"));

    const auto [code, out, err] = runVulkanIcd(tmp.path(), QStringLiteral("lvp"));
    QCOMPARE(code, 0);
    QCOMPARE(out, QStringLiteral("%1/lvp_icd.json").arg(tmp.path()));
}

void ScriptGuardTest::vulkanIcd_failsLoudlyWithNoManifest()
{
    // Returning 0 with an empty path would hand the caller an empty VK_ICD_FILENAMES and the
    // render gate would fail somewhere further down with no idea why.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const auto [code, out, err] = runVulkanIcd(tmp.path(), QStringLiteral("lvp"));
    QVERIFY2(code != 0, "vulkan_icd exited 0 with no manifest to report");
    QVERIFY2(out.isEmpty(), "vulkan_icd printed a path it did not find");
    QVERIFY2(err.contains(QStringLiteral("lvp")), qPrintable(QStringLiteral("expected a message naming the driver, got: %1").arg(err)));
}

void ScriptGuardTest::nestedKwinLaunchers_shareOneLauncher()
{
    // Three harnesses had three copies of the launch line and they had already drifted --
    // different sizes, timeouts, and outer env. Pin the count at one so a fourth cannot be
    // added quietly.
    verifyOnlyOwnerHas(QStringLiteral("kwin_wayland"), QStringLiteral("tests/lib/nested_kwin.sh"));
}

void ScriptGuardTest::nestedKwinLauncher_doesNotForceTheVulkanRhi()
{
    const QString src = readRepoFile(QStringLiteral("tests/lib/nested_kwin.sh"));
    QVERIFY2(!src.isEmpty(), "tests/lib/nested_kwin.sh unreadable");

    // The render probe picks its own RHI backend. If the shared launcher forced Vulkan too, the
    // e2e and coverage harnesses -- which run the real latte-dock on the default OpenGL RHI --
    // would be dragged onto Vulkan the moment they started sharing this code.
    QVERIFY2(!src.contains(QStringLiteral("QSG_RHI_BACKEND")),
             "the shared launcher must not choose an RHI backend for its callers");
}

void ScriptGuardTest::nestedKwinLauncher_propagatesTheSessionExitCode_data()
{
    QTest::addColumn<int>("sessionExitCode");

    QTest::newRow("success") << 0;
    QTest::newRow("failure") << 7;
}

void ScriptGuardTest::nestedKwinLauncher_propagatesTheSessionExitCode()
{
    // The launcher this replaced echoed $? into a tempfile and re-exited with it, on the premise
    // -- stated in its header comment -- that kwin's own exit code says nothing about the
    // session's. That is not how --exit-with-session behaves, and nobody had ever checked. Assert
    // the contract directly, so the next person to touch the launcher finds out from a test.
    if (QStandardPaths::findExecutable(QStringLiteral("kwin_wayland")).isEmpty()
        || QStandardPaths::findExecutable(QStringLiteral("dbus-run-session")).isEmpty()) {
        QSKIP("needs kwin_wayland and dbus-run-session");
    }

    QFETCH(int, sessionExitCode);

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const auto writeScript = [&tmp](const QString &name, const QString &body) {
        const QString path = tmp.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return QString();
        }
        f.write(body.toUtf8());
        f.close();
        f.setPermissions(f.permissions() | QFileDevice::ExeOwner);
        return path;
    };

    const QString session = writeScript(QStringLiteral("session.sh"),
                                        QStringLiteral("#!/bin/bash\nexit %1\n").arg(sessionExitCode));
    QVERIFY2(!session.isEmpty(), "could not write the session script");

    const QString driver = writeScript(QStringLiteral("driver.sh"),
                                       QStringLiteral("#!/bin/bash\nset -u\n. %1\nlaunch_nested_kwin --width 64 --height 64 --timeout 60 -- %2\n")
                                           .arg(repoPath(QStringLiteral("tests/lib/nested_kwin.sh")), session));
    QVERIFY2(!driver.isEmpty(), "could not write the driver script");

    QProcess p;
    p.setWorkingDirectory(tmp.path());
    p.start(QStringLiteral("bash"), {driver});
    QVERIFY2(p.waitForFinished(120000), "the nested kwin session did not terminate");

    QCOMPARE(p.exitStatus(), QProcess::NormalExit);
    QCOMPARE(p.exitCode(), sessionExitCode);
}

void ScriptGuardTest::dockCtl_isDefinedOnce()
{
    verifyOnlyOwnerHas(QStringLiteral("busctl --user call org.kde.lattedock"), QStringLiteral("tests/lib/dockctl.sh"));
}

void ScriptGuardTest::dockCtl_doesNotSwallowMutatingCallErrors()
{
    const QString src = codeOnly(readRepoFile(QStringLiteral("tests/lib/dockctl.sh")));
    QVERIFY2(!src.isEmpty(), "tests/lib/dockctl.sh unreadable");

    const int def = src.indexOf(QStringLiteral("busctl --user call org.kde.lattedock"));
    QVERIFY2(def != -1, "no dctl definition in the shared helper");
    const QString line = src.mid(def, src.indexOf(QLatin1Char('\n'), def) - def);

    // One of the two copies appended 2>/dev/null. Hoisting that version would erase the errors
    // from addApplet and triggerAppletAction -- the two calls the e2e harness deliberately
    // leaves unmuted, because when they fail their stderr is the only clue why.
    QVERIFY2(!line.contains(QStringLiteral("2>/dev/null")),
             "the shared dctl swallows stderr; read-only callers should mute it per-site instead");
}

QTEST_MAIN(ScriptGuardTest)
#include "scriptguardtest.moc"
