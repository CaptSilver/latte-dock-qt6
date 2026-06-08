/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Guards the Qt6 Binding.restoreMode freeze sites. Qt6 changed the default
// restoreMode from RestoreNone to RestoreBindingOrValue, so a "Binding { when: }"
// meant to FREEZE its target's last value on deactivation instead RESETS it.
// Latte uses when-gated bindings in these files to hold indices/positions while
// an update is blocked (drag/reorder) or a relocation animation runs. Every such
// binding must carry restoreMode: Binding.RestoreNone. This test reads the real
// QML and fails if any when-gated Binding in them is missing it — the exact
// regression the migration sweep kept reintroducing. (Behavioral testing needs
// the full containment context, so this guards the source structurally.)

#include <QFile>
#include <QObject>
#include <QString>
#include <QtTest>

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

    void checkFile(const QString &path, int minFreezeBindings)
    {
        const QString src = readFile(path);
        QVERIFY2(!src.isEmpty(), qPrintable(QStringLiteral("could not read %1").arg(path)));

        int frozen = 0;
        const QList<QString> blocks = bindingBlocks(src);
        for (const QString &block : blocks) {
            // A when-gated Binding is a freeze site in these files.
            if (!block.contains(QStringLiteral("when:"))) {
                continue;
            }
            ++frozen;
            QVERIFY2(block.contains(QStringLiteral("restoreMode: Binding.RestoreNone")),
                     qPrintable(QStringLiteral("%1 has a when-gated Binding without "
                                               "restoreMode: Binding.RestoreNone:\n%2")
                                    .arg(path, block.left(160))));
        }

        QVERIFY2(frozen >= minFreezeBindings,
                 qPrintable(QStringLiteral("%1: expected at least %2 when-gated bindings, found %3")
                                .arg(path).arg(minFreezeBindings).arg(frozen)));
    }

private Q_SLOTS:
    void indexerFreezesIndices();
    void positionShortcutsFreezeDuringDrag();
    void layoutsContainerFreezesDuringRelocation();
};

void BindingRestoreModeTest::indexerFreezesIndices()
{
    checkFile(QStringLiteral(INDEXER_QML_PATH), 6);
}

void BindingRestoreModeTest::positionShortcutsFreezeDuringDrag()
{
    checkFile(QStringLiteral(POSITIONSHORTCUTS_QML_PATH), 2);
}

void BindingRestoreModeTest::layoutsContainerFreezesDuringRelocation()
{
    checkFile(QStringLiteral(LAYOUTSCONTAINER_QML_PATH), 2);
}

QTEST_GUILESS_MAIN(BindingRestoreModeTest)

#include "bindingrestoremodetest.moc"
