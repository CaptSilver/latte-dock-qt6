/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../app/settings/generic/generictools.h"

#include <QImage>
#include <QPainter>
#include <QRect>
#include <QStringList>
#include <QStyleOption>
#include <QStyleOptionViewItem>
#include <QtTest>

using namespace Latte;

// Real-object tests for the Settings::Generic free helpers in generictools.cpp.
// Most take a QStyleOption + QPainter; we feed real options and assert the
// observable outputs (state-flag predicates, alignment mapping, color-group
// priority, list subtraction, geometry rects) and smoke-paint the painters into
// a QImage-backed QPainter under offscreen QPA so they exercise real code paths.
class GenericToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void statePredicates_data();
    void statePredicates();

    void isTextCenteredReadsDisplayAlignment();

    void horizontalAlignment_data();
    void horizontalAlignment();

    void colorGroupDisabledWins();
    void colorGroupActiveBeforeSelected();
    void colorGroupFocusedMapsActive();
    void colorGroupInactiveSelected();
    void colorGroupNormal();

    void subtractedRemovesGone();
    void subtractedEmptyWhenSubset();
    void subtractedKeepsDuplicatesPerOriginal();

    void screenMaxLengthIsOdd_data();
    void screenMaxLengthIsOdd();
    void screenMaxLengthClampedByMaxIcon();

    void remainedFromChangesIndicatorShrinksWidth();
    void remainedFromIconShiftsLeftAligned();
    void remainedFromIconDefaultThickMarginResolves();
    void remainedFromLayoutIconCenteredReturnsFull();
    void remainedFromScreenDrawingShrinks();

    void drawChangesIndicatorPaints();
    void drawScreenReturnsAvailableRect();
    void drawFormattedTextDoesNotCrash();
};

static QStyleOptionViewItem makeOption(QStyle::State state, const QRect &rect = QRect(0, 0, 200, 30))
{
    QStyleOptionViewItem opt;
    opt.state = state;
    opt.rect = rect;
    return opt;
}

void GenericToolsTest::statePredicates_data()
{
    QTest::addColumn<int>("state");
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<bool>("active");
    QTest::addColumn<bool>("selected");
    QTest::addColumn<bool>("hovered");
    QTest::addColumn<bool>("focused");

    QTest::newRow("none") << int(QStyle::State_None) << false << false << false << false << false;
    QTest::newRow("enabled") << int(QStyle::State_Enabled) << true << false << false << false << false;
    QTest::newRow("active") << int(QStyle::State_Active) << false << true << false << false << false;
    QTest::newRow("selected") << int(QStyle::State_Selected) << false << false << true << false << false;
    QTest::newRow("mouseover") << int(QStyle::State_MouseOver) << false << false << false << true << false;
    QTest::newRow("focus") << int(QStyle::State_HasFocus) << false << false << false << false << true;
    QTest::newRow("all")
        << int(QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Selected
               | QStyle::State_MouseOver | QStyle::State_HasFocus)
        << true << true << true << true << true;
}

void GenericToolsTest::statePredicates()
{
    QFETCH(int, state);
    QFETCH(bool, enabled);
    QFETCH(bool, active);
    QFETCH(bool, selected);
    QFETCH(bool, hovered);
    QFETCH(bool, focused);

    QStyleOptionViewItem opt = makeOption(QStyle::State(state));

    QCOMPARE(isEnabled(opt), enabled);
    QCOMPARE(isActive(opt), active);
    QCOMPARE(isSelected(opt), selected);
    QCOMPARE(isHovered(opt), hovered);
    QCOMPARE(isFocused(opt), focused);
}

void GenericToolsTest::isTextCenteredReadsDisplayAlignment()
{
    QStyleOptionViewItem centered = makeOption(QStyle::State_Enabled);
    centered.displayAlignment = Qt::AlignHCenter | Qt::AlignVCenter;
    QVERIFY(isTextCentered(centered));

    QStyleOptionViewItem left = makeOption(QStyle::State_Enabled);
    left.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    QVERIFY(!isTextCentered(left));
}

void GenericToolsTest::horizontalAlignment_data()
{
    QTest::addColumn<Qt::Alignment>("in");
    QTest::addColumn<Qt::AlignmentFlag>("out");

    QTest::newRow("hcenter") << Qt::Alignment(Qt::AlignHCenter | Qt::AlignVCenter) << Qt::AlignHCenter;
    QTest::newRow("right") << Qt::Alignment(Qt::AlignRight | Qt::AlignBottom) << Qt::AlignRight;
    QTest::newRow("left") << Qt::Alignment(Qt::AlignLeft | Qt::AlignTop) << Qt::AlignLeft;
    // No horizontal bit set falls through to Left.
    QTest::newRow("vertical-only") << Qt::Alignment(Qt::AlignVCenter) << Qt::AlignLeft;
    // HCenter takes precedence over Right when both are set.
    QTest::newRow("center-beats-right") << Qt::Alignment(Qt::AlignHCenter | Qt::AlignRight) << Qt::AlignHCenter;
}

void GenericToolsTest::horizontalAlignment()
{
    QFETCH(Qt::Alignment, in);
    QFETCH(Qt::AlignmentFlag, out);
    QCOMPARE(Latte::horizontalAlignment(in), out);
}

void GenericToolsTest::colorGroupDisabledWins()
{
    // Not enabled -> Disabled, regardless of every other flag being set.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Active | QStyle::State_Selected | QStyle::State_HasFocus);
    QCOMPARE(colorGroup(opt), QPalette::Disabled);
}

void GenericToolsTest::colorGroupActiveBeforeSelected()
{
    // Enabled + Active + Selected -> Active (active is checked before the inactive+selected branch).
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Selected);
    QCOMPARE(colorGroup(opt), QPalette::Active);
}

void GenericToolsTest::colorGroupFocusedMapsActive()
{
    // Focus alone (no Active) still maps to Active.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled | QStyle::State_HasFocus);
    QCOMPARE(colorGroup(opt), QPalette::Active);
}

void GenericToolsTest::colorGroupInactiveSelected()
{
    // Selected but not active/focused -> Inactive.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled | QStyle::State_Selected);
    QCOMPARE(colorGroup(opt), QPalette::Inactive);
}

void GenericToolsTest::colorGroupNormal()
{
    // Enabled, nothing else -> Normal.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled);
    QCOMPARE(colorGroup(opt), QPalette::Normal);
}

void GenericToolsTest::subtractedRemovesGone()
{
    QStringList original{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    QStringList current{QStringLiteral("b")};
    QCOMPARE(subtracted(original, current), (QStringList{QStringLiteral("a"), QStringLiteral("c")}));
}

void GenericToolsTest::subtractedEmptyWhenSubset()
{
    QStringList original{QStringLiteral("x"), QStringLiteral("y")};
    QStringList current{QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z")};
    QVERIFY(subtracted(original, current).isEmpty());
}

void GenericToolsTest::subtractedKeepsDuplicatesPerOriginal()
{
    // Each original entry is tested independently, so a value absent from current
    // appears as many times as it occurs in original.
    QStringList original{QStringLiteral("dup"), QStringLiteral("dup")};
    QStringList current;
    QCOMPARE(subtracted(original, current), (QStringList{QStringLiteral("dup"), QStringLiteral("dup")}));
}

void GenericToolsTest::screenMaxLengthIsOdd_data()
{
    QTest::addColumn<int>("height");
    QTest::newRow("h30") << 30;
    QTest::newRow("h31") << 31;
    QTest::newRow("h40") << 40;
    QTest::newRow("h100") << 100;
}

void GenericToolsTest::screenMaxLengthIsOdd()
{
    QFETCH(int, height);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, height));
    int len = screenMaxLength(opt);
    // The helper guarantees an odd length (even results are decremented).
    QVERIFY(len % 2 == 1);
    // And it scales ~1.7x the icon height (within the odd-rounding slack).
    QVERIFY(len <= int(height * 1.7));
    QVERIFY(len >= int(height * 1.7) - 1);
}

void GenericToolsTest::screenMaxLengthClampedByMaxIcon()
{
    // maxIconSize smaller than the row height caps the icon length used.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 100));
    int unclamped = screenMaxLength(opt, -1);
    int clamped = screenMaxLength(opt, 20);
    QVERIFY(clamped < unclamped);
    QVERIFY(clamped % 2 == 1);
}

void GenericToolsTest::remainedFromChangesIndicatorShrinksWidth()
{
    // Indicator reserves a fixed slot (length 6 + 2*5 margins = 16); remaining
    // width drops by that. Default LTR keeps x unchanged.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(10, 5, 200, 30));
    QRect r = remainedFromChangesIndicator(opt);
    QCOMPARE(r.x(), 10);
    QCOMPARE(r.y(), 5);
    QCOMPARE(r.height(), 30);
    QCOMPARE(r.width(), 200 - 16);
}

void GenericToolsTest::remainedFromIconShiftsLeftAligned()
{
    // Left-aligned (LTR): the remaining rect starts after the icon slot and is
    // narrower by the same amount. Slot = iconsize + 2*lenmargin, with the default
    // -1 thickMargin resolving to ICONMARGIN.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    QRect r = remainedFromIcon(opt, Qt::AlignLeft);
    QVERIFY(r.x() > 0);
    QCOMPARE(r.width(), 200 - r.x());
    QCOMPARE(r.height(), 30);
}

void GenericToolsTest::remainedFromIconDefaultThickMarginResolves()
{
    // thickMargin defaults to -1, which must resolve to ICONMARGIN for BOTH the
    // icon size and the offset. Passing -1 must therefore yield the same rect as
    // passing the resolved margin explicitly; the bug sized the icon from the raw
    // -1 sentinel (height - 2*(-1)) so the two diverged.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    const QRect resolvedDefault = remainedFromIcon(opt, Qt::AlignLeft, -1, -1);
    const QRect explicitMargin = remainedFromIcon(opt, Qt::AlignLeft, -1, 1); // 1 == ICONMARGIN
    QCOMPARE(resolvedDefault, explicitMargin);
}

void GenericToolsTest::remainedFromLayoutIconCenteredReturnsFull()
{
    // Centered alignment short-circuits to the full rect (no slot reserved).
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(3, 4, 200, 30));
    QRect r = remainedFromLayoutIcon(opt, Qt::AlignHCenter);
    QCOMPARE(r, QRect(3, 4, 200, 30));
}

void GenericToolsTest::remainedFromScreenDrawingShrinks()
{
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 300, 40));
    QRect r = remainedFromScreenDrawing(opt, false);
    QVERIFY(r.width() < 300);
    QVERIFY(r.x() > 0); // LTR: shifted right past the screen icon
    QCOMPARE(r.height(), 40);
}

void GenericToolsTest::drawChangesIndicatorPaints()
{
    QImage img(200, 30, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));

    drawChangesIndicator(&p, opt);
    p.end();

    // The orange dot lands at the right edge; somewhere in the image must now be
    // non-transparent.
    bool painted = false;
    for (int y = 0; y < img.height() && !painted; ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(img.pixel(x, y)) != 0) {
                painted = true;
                break;
            }
        }
    }
    QVERIFY(painted);
}

void GenericToolsTest::drawScreenReturnsAvailableRect()
{
    QImage img(300, 40, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 300, 40));

    QRect avail = drawScreen(&p, opt, false, QRect(0, 0, 1920, 1080));
    p.end();

    // The returned available rect is non-empty and sits inside the option rect.
    QVERIFY(avail.isValid());
    QVERIFY(avail.width() > 0);
    QVERIFY(avail.height() > 0);
    QVERIFY(opt.rect.contains(avail.topLeft()));
}

void GenericToolsTest::drawFormattedTextDoesNotCrash()
{
    QImage img(200, 30, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled | QStyle::State_Selected, QRect(0, 0, 200, 30));
    opt.text = QStringLiteral("Hello");
    opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;

    // Drives the HighlightedText branch (Selected) plus the QTextDocument render.
    drawFormattedText(&p, opt, 1.0);
    p.end();

    bool painted = false;
    for (int y = 0; y < img.height() && !painted; ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(img.pixel(x, y)) != 0) {
                painted = true;
                break;
            }
        }
    }
    QVERIFY(painted);
}

QTEST_MAIN(GenericToolsTest)

#include "generictoolstest.moc"
