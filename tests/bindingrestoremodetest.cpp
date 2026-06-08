/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Guards the Qt6 Binding.restoreMode freeze sites. Qt6 changed the default
// restoreMode from RestoreNone to RestoreBindingOrValue, so a "Binding { when: }"
// meant to FREEZE its target's last value on deactivation instead RESETS it.
// The files below hold indices/sizes/positions/flags steady while an update is
// blocked (drag/reorder/reparent), a relocation animation runs, or a config view
// switches — every when-gated Binding in them must carry
// restoreMode: Binding.RestoreNone. These files were triaged as uniformly
// freeze-intent (their when: gates are transients, not feature toggles), so the
// per-file invariant holds. This reads the real QML and fails on any when-gated
// Binding missing it — the regression the migration sweep kept reintroducing.
// (Behavioral testing needs the full containment context, so this is structural.)

#include <QFile>
#include <QObject>
#include <QString>
#include <QtTest>

struct FreezeFile {
    const char *relPath;
    int minWhenGated; // lower bound so a gutted/refactored file fails loudly
};

class BindingRestoreModeTest : public QObject
{
    Q_OBJECT

private:
    static QString readFile(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }
        return QString::fromUtf8(f.readAll());
    }

    // Extract the body of every `Binding { ... }` element via brace matching, so
    // we are not fooled by `Binding.RestoreNone` references or sibling elements.
    static QList<QString> bindingBlocks(const QString &src)
    {
        QList<QString> blocks;
        const QString marker = QStringLiteral("Binding");
        int idx = 0;
        while ((idx = src.indexOf(marker, idx)) != -1) {
            const int after = idx + marker.size();
            const int brace = src.indexOf(QLatin1Char('{'), after);
            // Only a Binding ELEMENT: nothing but whitespace between name and '{'.
            if (brace == -1 || !src.mid(after, brace - after).trimmed().isEmpty()) {
                idx = after;
                continue;
            }
            int depth = 0;
            int i = brace;
            for (; i < src.size(); ++i) {
                if (src.at(i) == QLatin1Char('{')) {
                    ++depth;
                } else if (src.at(i) == QLatin1Char('}')) {
                    if (--depth == 0) {
                        ++i;
                        break;
                    }
                }
            }
            blocks << src.mid(brace, i - brace);
            idx = i;
        }
        return blocks;
    }

private Q_SLOTS:
    void freezeBindingsCarryRestoreNone_data();
    void freezeBindingsCarryRestoreNone();
};

void BindingRestoreModeTest::freezeBindingsCarryRestoreNone_data()
{
    QTest::addColumn<QString>("relPath");
    QTest::addColumn<int>("minWhenGated");

    static const FreezeFile files[] = {
        {"declarativeimports/abilities/client/Indexer.qml", 6},
        {"declarativeimports/abilities/items/basicitem/HiddenSpacer.qml", 1},
        {"containment/package/contents/ui/layouts/LayoutsContainer.qml", 3},
        {"containment/package/contents/ui/abilities/privates/PositionShortcutsPrivate.qml", 2},
        {"containment/package/contents/ui/abilities/privates/AnimationsPrivate.qml", 1},
        {"containment/package/contents/ui/abilities/privates/IndexerPrivate.qml", 6},
        {"containment/package/contents/ui/abilities/privates/MyViewPrivate.qml", 1},
        {"containment/package/contents/ui/abilities/privates/ThinTooltipPrivate.qml", 1},
        {"containment/package/contents/ui/abilities/privates/ParabolicEffectPrivate.qml", 1},
        {"containment/package/contents/ui/abilities/privates/LaunchersPrivate.qml", 2},
        {"containment/package/contents/ui/abilities/privates/layouter/AppletsContainer.qml", 8},
        {"plasmoid/package/contents/ui/main.qml", 3},
        {"shell/package/contents/configuration/pages/AppearanceConfig.qml", 2},
    };

    for (const FreezeFile &f : files) {
        QTest::newRow(f.relPath) << QStringLiteral("%1/%2").arg(QStringLiteral(REPO_ROOT), QString::fromUtf8(f.relPath))
                                 << f.minWhenGated;
    }
}

void BindingRestoreModeTest::freezeBindingsCarryRestoreNone()
{
    QFETCH(QString, relPath);
    QFETCH(int, minWhenGated);

    const QString src = readFile(relPath);
    QVERIFY2(!src.isEmpty(), qPrintable(QStringLiteral("could not read %1").arg(relPath)));

    int frozen = 0;
    const QList<QString> blocks = bindingBlocks(src);
    for (const QString &block : blocks) {
        if (!block.contains(QStringLiteral("when:"))) {
            continue;
        }
        ++frozen;
        QVERIFY2(block.contains(QStringLiteral("restoreMode: Binding.RestoreNone")),
                 qPrintable(QStringLiteral("%1 has a when-gated Binding without "
                                           "restoreMode: Binding.RestoreNone:\n%2")
                                .arg(relPath, block.left(160))));
    }

    QVERIFY2(frozen >= minWhenGated,
             qPrintable(QStringLiteral("%1: expected at least %2 when-gated bindings, found %3")
                            .arg(relPath).arg(minWhenGated).arg(frozen)));
}

QTEST_GUILESS_MAIN(BindingRestoreModeTest)

#include "bindingrestoremodetest.moc"
