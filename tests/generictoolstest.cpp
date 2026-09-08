/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../app/settings/generic/generictools.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QStringList>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionButton>
#include <QStyleOptionMenuItem>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QWidget>
#include <QtTest>

using namespace Latte;

//! True when some opaque pixel is dominated by the given channel, used to assert a
//! solid-colour fill survived scaling/antialiasing without pinning an exact RGB.
static bool hasGreenishPixel(const QImage &img)
{
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRgb px = img.pixel(x, y);
            if (qAlpha(px) > 200 && qGreen(px) > 120 && qGreen(px) > qRed(px) && qGreen(px) > qBlue(px)) {
                return true;
            }
        }
    }
    return false;
}

//! Bounding box of everything non-transparent. The draw helpers below are asserted
//! through this: that something was painted holds for any implementation at all, while
//! the rectangle the ink lands in does not.
static QRect paintedBounds(const QImage &img)
{
    int left = img.width(), right = -1, top = img.height(), bottom = -1;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(img.pixel(x, y)) != 0) {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    }
    return right < 0 ? QRect() : QRect(left, top, right - left + 1, bottom - top + 1);
}

//! Paints one helper onto its own transparent canvas the size of the given rect, so the
//! placement it chose can be read back as a bounding box.
template <typename Draw>
static QImage paintedOn(const QRect &rect, Draw draw)
{
    QImage img(rect.width(), rect.height(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    draw(&p);
    p.end();
    return img;
}

//! Ink per colour channel across the whole canvas. Antialiased glyphs leave few fully
//! opaque pixels, so which colour a text run was drawn in is read off the totals rather
//! than off any one pixel.
struct InkSums
{
    qint64 red = 0;
    qint64 green = 0;
    qint64 blue = 0;
};

static InkSums inkSums(const QImage &img)
{
    InkSums sums;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRgb px = img.pixel(x, y);
            sums.red += qRed(px);
            sums.green += qGreen(px);
            sums.blue += qBlue(px);
        }
    }
    return sums;
}

static QImage paintedByIcon(const QStyleOption &option, const QString &icon, Qt::AlignmentFlag alignment)
{
    return paintedOn(option.rect, [&](QPainter *p) { drawIcon(p, option, icon, alignment); });
}

static QImage paintedByLayoutIcon(const QStyleOption &option, const QString &icon, Qt::AlignmentFlag alignment)
{
    return paintedOn(option.rect, [&](QPainter *p) { drawLayoutIcon(p, option, false, icon, alignment); });
}

//! The background-file branch of drawLayoutIcon, fed a path that does not exist.
static QImage paintedByLayoutBackground(const QStyleOption &option, Qt::AlignmentFlag alignment)
{
    return paintedOn(option.rect, [&](QPainter *p) { drawLayoutIcon(p, option, true, QStringLiteral("/definitely/missing-bg.png"), alignment); });
}

static QImage paintedByColorSchemeIcon(const QStyleOption &option, Qt::AlignmentFlag alignment)
{
    return paintedOn(option.rect, [&](QPainter *p) { drawColorSchemeIcon(p, option, QColor(220, 0, 0), QColor(0, 0, 220), alignment); });
}

//! The viewitem overload of drawFormattedText, which reads its alignment off
//! displayAlignment.
static QImage paintedByFormattedText(QStyleOptionViewItem option, const QString &text, Qt::AlignmentFlag alignment)
{
    option.text = text;
    option.displayAlignment = alignment | Qt::AlignVCenter;
    return paintedOn(option.rect, [&](QPainter *p) { drawFormattedText(p, option, 1.0); });
}

// Real-object tests for the Settings::Generic free helpers in generictools.cpp.
// Most take a QStyleOption + QPainter; we feed real options and assert the
// observable outputs (state-flag predicates, alignment mapping, color-group
// priority, list subtraction, geometry rects) and smoke-paint the painters into
// a QImage-backed QPainter under offscreen QPA so they exercise real code paths.
class GenericToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();

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

    void drawChangesIndicatorPaintsRightEdge();
    void drawScreenReturnsAvailableRect();
    void drawFormattedTextUsesHighlightColorWhenSelected();

    // remaining-rect helpers not yet exercised
    void remainedFromFormattedText_data();
    void remainedFromFormattedText();
    void remainedFromFormattedTextRtlFlipsSide();
    void remainedFromIconRtlKeepsX();
    void remainedFromColorSchemeIconCenteredReturnsFull();
    void remainedFromColorSchemeIconDelegatesWhenAligned();
    void remainedFromLayoutIconLeftDelegates();
    void primitiveCheckBoxWidthIsPositive();
    void remainedFromCheckBoxShrinksFromLeft();
    void remainedFromCheckBoxRtlKeepsX();
    void remainedFromChangesIndicatorRtlShiftsX();
    void remainedFromScreenDrawingRtlKeepsX();

    // drawing helpers not yet exercised
    void drawIconPaintsThemedIcon();
    void drawLayoutIconBackgroundStrokesOutline();
    void drawLayoutIconThemedPaints();
    void drawColorSchemeIconPaintsColors();
    void drawCheckBoxPlacement_data();
    void drawCheckBoxPlacement();
    void drawBackgroundViewItemFillsRowWhenSelected();
    void drawBackgroundMenuItemFillsRowWhenSelected();
    void drawFormattedTextCentersInk();
    void drawFormattedTextRightAlignsInkToTheEdge();
    void drawFormattedTextMenuOverloadFollowsLayoutDirection();
    void drawChangesIndicatorRtlPaintsLeftEdge();
    void drawScreenFollowsLayoutDirection();
    void drawScreenMultipleScreensOutlinesBehindTheScreen();
    void drawScreenPortraitGeometryPaintsNarrowerScreen();

    // the icon slot itself: every icon painter must place its target identically
    void iconPaintersShareTheSlot_data();
    void iconPaintersShareTheSlot();
    void remainedFromIconExactSlot();
    void remainedFromIconCenteredIgnoresDirection();

private:
    QTemporaryDir *m_iconDir = nullptr;
    QString m_themeIcon;
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
        << int(QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus)
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

void GenericToolsTest::drawChangesIndicatorPaintsRightEdge()
{
    // The dot is placed by arithmetic on the indicator constants alone - no style, no
    // font - so the rect it inks is fixed: a 6px circle inset by the 5px margin from the
    // right edge of a 200x30 row, one pixel wider each way for the stroke.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    const QImage img = paintedOn(opt.rect, [&](QPainter *p) { drawChangesIndicator(p, opt); });

    QCOMPARE(paintedBounds(img), QRect(189, 12, 7, 7));
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

void GenericToolsTest::drawFormattedTextUsesHighlightColorWhenSelected()
{
    // A selected row draws its text in HighlightedText, an unselected one in Text. Give
    // the two roles colours nothing else can produce and read the ink back: red means the
    // helper took the Text branch, blue the HighlightedText one.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    opt.palette.setColor(QPalette::Text, QColor(255, 0, 0));
    opt.palette.setColor(QPalette::HighlightedText, QColor(0, 0, 255));

    const InkSums plain = inkSums(paintedByFormattedText(opt, QStringLiteral("Hello"), Qt::AlignLeft));
    QVERIFY(plain.red > 0);
    QCOMPARE(plain.blue, 0);

    opt.state |= QStyle::State_Selected;
    const QImage highlighted = paintedByFormattedText(opt, QStringLiteral("Hello"), Qt::AlignLeft);
    const InkSums selected = inkSums(highlighted);
    QVERIFY(selected.blue > 0);
    QCOMPARE(selected.red, 0);

    // Left-aligned text starts at the left edge of the row, a margin of glyph bearing aside.
    QVERIFY(paintedBounds(highlighted).left() < 10);
}

void GenericToolsTest::initTestCase()
{
    // Stand up a throwaway icon theme with a solid-green icon so the icon-drawing
    // helpers resolve deterministically instead of depending on the host theme.
    m_iconDir = new QTemporaryDir();
    QVERIFY(m_iconDir->isValid());
    // A theme lives at <searchPath>/<themeName>/, so build it under a named subdir.
    const QString themeRoot = m_iconDir->path() + QStringLiteral("/lattetest");
    m_themeIcon = QStringLiteral("latte-generictools-testicon");

    const QList<int> sizes{16, 22, 32, 48};
    QStringList dirs;
    for (int s : sizes) {
        const QString rel = QStringLiteral("%1x%1/apps").arg(s);
        dirs << rel;
        QVERIFY(QDir().mkpath(themeRoot + QLatin1Char('/') + rel));
        QImage icon(s, s, QImage::Format_ARGB32);
        icon.fill(QColor(0, 180, 0));
        QVERIFY(icon.save(themeRoot + QLatin1Char('/') + rel + QLatin1Char('/') + m_themeIcon + QStringLiteral(".png")));
    }

    QString index = QStringLiteral("[Icon Theme]\nName=lattetest\nDirectories=%1\n").arg(dirs.join(QLatin1Char(',')));
    for (int s : sizes) {
        index += QStringLiteral("[%1x%1/apps]\nSize=%1\nType=Fixed\nContext=Applications\n").arg(s);
    }
    QFile idx(themeRoot + QStringLiteral("/index.theme"));
    QVERIFY(idx.open(QIODevice::WriteOnly | QIODevice::Text));
    idx.write(index.toUtf8());
    idx.close();

    QIcon::setThemeSearchPaths(QStringList{m_iconDir->path()});
    QIcon::setThemeName(QStringLiteral("lattetest"));
    QVERIFY2(QIcon::hasThemeIcon(m_themeIcon), "the test icon theme did not resolve");
}

void GenericToolsTest::cleanup()
{
    // Several helpers branch on qApp->layoutDirection(); reset it so an RTL test
    // can't leak into the next one.
    qApp->setLayoutDirection(Qt::LeftToRight);
}

void GenericToolsTest::remainedFromFormattedText_data()
{
    QTest::addColumn<int>("alignment");
    QTest::addColumn<bool>("fullRect");
    QTest::addColumn<bool>("xShifts");

    // Left (LTR): the remaining rect starts after the text slot.
    QTest::newRow("left") << int(Qt::AlignLeft) << false << true;
    // Right (LTR): x stays put, width shrinks by the text slot.
    QTest::newRow("right") << int(Qt::AlignRight) << false << false;
    // Center short-circuits to the full rect.
    QTest::newRow("center") << int(Qt::AlignHCenter) << true << false;
}

void GenericToolsTest::remainedFromFormattedText()
{
    QFETCH(int, alignment);
    QFETCH(bool, fullRect);
    QFETCH(bool, xShifts);

    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(10, 5, 200, 30));
    QRect r = Latte::remainedFromFormattedText(opt, QStringLiteral("Hello"), Qt::AlignmentFlag(alignment));

    if (fullRect) {
        QCOMPARE(r, opt.rect);
        return;
    }

    QVERIFY(r.width() < opt.rect.width());
    QCOMPARE(r.height(), opt.rect.height());
    if (xShifts) {
        QVERIFY(r.x() > opt.rect.x());
    } else {
        QCOMPARE(r.x(), opt.rect.x());
    }
}

void GenericToolsTest::remainedFromFormattedTextRtlFlipsSide()
{
    // In RTL, AlignLeft flips to the right-hand computation, so x stays put
    // (the mirror of the LTR "left" row above).
    qApp->setLayoutDirection(Qt::RightToLeft);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(10, 5, 200, 30));
    QRect r = Latte::remainedFromFormattedText(opt, QStringLiteral("Hello"), Qt::AlignLeft);
    QCOMPARE(r.x(), opt.rect.x());
    QVERIFY(r.width() < opt.rect.width());
}

void GenericToolsTest::remainedFromIconRtlKeepsX()
{
    // LTR AlignLeft shifts x right (covered elsewhere); RTL flips it so x is kept.
    qApp->setLayoutDirection(Qt::RightToLeft);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    QRect r = remainedFromIcon(opt, Qt::AlignLeft);
    QCOMPARE(r.x(), 0);
    QVERIFY(r.width() < 200);
    QCOMPARE(r.height(), 30);
}

void GenericToolsTest::remainedFromColorSchemeIconCenteredReturnsFull()
{
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(3, 4, 200, 30));
    QCOMPARE(remainedFromColorSchemeIcon(opt, Qt::AlignHCenter), QRect(3, 4, 200, 30));
}

void GenericToolsTest::remainedFromColorSchemeIconDelegatesWhenAligned()
{
    // Non-centered just delegates to remainedFromIcon.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    QCOMPARE(remainedFromColorSchemeIcon(opt, Qt::AlignLeft), remainedFromIcon(opt, Qt::AlignLeft));
}

void GenericToolsTest::remainedFromLayoutIconLeftDelegates()
{
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    QCOMPARE(remainedFromLayoutIcon(opt, Qt::AlignLeft), remainedFromIcon(opt, Qt::AlignLeft));
}

void GenericToolsTest::primitiveCheckBoxWidthIsPositive()
{
    QStyleOptionButton opt;
    opt.rect = QRect(0, 0, 200, 30);
    QVERIFY(primitiveCheckBoxWidth(opt) > 0);
}

void GenericToolsTest::remainedFromCheckBoxShrinksFromLeft()
{
    QStyleOptionButton opt;
    opt.rect = QRect(0, 0, 200, 30);
    QRect r = remainedFromCheckBox(opt, Qt::AlignLeft);
    QVERIFY(r.width() < 200);
    QVERIFY(r.x() > 0); // LTR left: remaining starts past the checkbox slot
    QCOMPARE(r.height(), 30);
}

void GenericToolsTest::remainedFromCheckBoxRtlKeepsX()
{
    qApp->setLayoutDirection(Qt::RightToLeft);
    QStyleOptionButton opt;
    opt.rect = QRect(0, 0, 200, 30);
    QRect r = remainedFromCheckBox(opt, Qt::AlignLeft);
    QCOMPARE(r.x(), 0);
    QVERIFY(r.width() < 200);
}

void GenericToolsTest::remainedFromChangesIndicatorRtlShiftsX()
{
    // RTL reserves the indicator slot on the left, so x moves right by 6+2*5=16.
    qApp->setLayoutDirection(Qt::RightToLeft);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(10, 5, 200, 30));
    QRect r = remainedFromChangesIndicator(opt);
    QCOMPARE(r.x(), 10 + 16);
    QCOMPARE(r.width(), 200 - 16);
}

void GenericToolsTest::remainedFromScreenDrawingRtlKeepsX()
{
    qApp->setLayoutDirection(Qt::RightToLeft);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 300, 40));
    QRect r = remainedFromScreenDrawing(opt, false);
    QCOMPARE(r.x(), 0);
    QVERIFY(r.width() < 300);
}

void GenericToolsTest::drawIconPaintsThemedIcon()
{
    QImage img(200, 30, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));

    drawIcon(&p, opt, m_themeIcon, Qt::AlignLeft);
    p.end();

    QVERIFY(hasGreenishPixel(img));
}

void GenericToolsTest::drawLayoutIconBackgroundStrokesOutline()
{
    // Background-file branch with an image that does not exist: the brush is empty, so what
    // is left is the stroked ellipse. Read the row through the middle of the slot - which
    // iconPaintersShareTheSlot pins - and it has ink at both ends and a hole between them.
    // A filled circle, or a plain rectangle standing in for the missing image, would paint
    // the middle too.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    const QImage img = paintedByLayoutBackground(opt, Qt::AlignLeft);

    const QRect slot(3, 1, 28, 28);
    const int midRow = slot.y() + slot.height() / 2;

    QVERIFY(qAlpha(img.pixel(slot.left(), midRow)) > 0);
    QVERIFY(qAlpha(img.pixel(slot.right(), midRow)) > 0);
    QCOMPARE(qAlpha(img.pixel(slot.center().x(), midRow)), 0);
}

void GenericToolsTest::drawLayoutIconThemedPaints()
{
    QImage img(200, 30, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));

    drawLayoutIcon(&p, opt, false, m_themeIcon, Qt::AlignLeft);
    p.end();

    QVERIFY(hasGreenishPixel(img));
}

void GenericToolsTest::drawColorSchemeIconPaintsColors()
{
    QImage img(200, 30, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));

    const QColor textColor(220, 0, 0); // red foreground square
    const QColor backColor(0, 0, 220); // blue background square
    drawColorSchemeIcon(&p, opt, textColor, backColor, Qt::AlignLeft);
    p.end();

    bool hasRed = false;
    bool hasBlue = false;
    for (int y = 0; y < img.height() && !(hasRed && hasBlue); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRgb px = img.pixel(x, y);
            if (qAlpha(px) < 200) {
                continue;
            }
            if (qRed(px) > 120 && qRed(px) > qGreen(px) && qRed(px) > qBlue(px)) {
                hasRed = true;
            }
            if (qBlue(px) > 120 && qBlue(px) > qRed(px) && qBlue(px) > qGreen(px)) {
                hasBlue = true;
            }
        }
    }
    QVERIFY(hasRed);
    QVERIFY(hasBlue);
}

void GenericToolsTest::drawCheckBoxPlacement_data()
{
    QTest::addColumn<Qt::AlignmentFlag>("alignment");
    QTest::addColumn<Qt::LayoutDirection>("direction");
    QTest::addColumn<bool>("leading"); //! indicator against the left edge of the row

    QTest::newRow("left-ltr") << Qt::AlignLeft << Qt::LeftToRight << true;
    QTest::newRow("right-ltr") << Qt::AlignRight << Qt::LeftToRight << false;
    QTest::newRow("left-rtl") << Qt::AlignLeft << Qt::RightToLeft << false;
    QTest::newRow("right-rtl") << Qt::AlignRight << Qt::RightToLeft << true;
}

void GenericToolsTest::drawCheckBoxPlacement()
{
    // Unlike the icon painters, the checkbox slot is sized by the widget style through
    // QStyle::sizeFromContents, so the indicator rect is whatever the style hands back and
    // is no invariant of this code. What the helper does decide is which end of the row it
    // goes to, and that RTL swaps the two ends - so only the end is asserted.
    QFETCH(Qt::AlignmentFlag, alignment);
    QFETCH(Qt::LayoutDirection, direction);
    QFETCH(bool, leading);

    qApp->setLayoutDirection(direction);
    QStyleOptionButton opt;
    opt.rect = QRect(0, 0, 200, 30);
    opt.state = QStyle::State_Enabled | QStyle::State_On;
    opt.palette = QApplication::palette();

    const QRect ink = paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { drawCheckBox(p, opt, alignment); }));
    QVERIFY(!ink.isEmpty());

    if (leading) {
        QVERIFY2(ink.right() < opt.rect.width() / 3, qPrintable(QStringLiteral("indicator at %1..%2").arg(ink.left()).arg(ink.right())));
    } else {
        QVERIFY2(ink.left() > opt.rect.width() / 2, qPrintable(QStringLiteral("indicator at %1..%2").arg(ink.left()).arg(ink.right())));
    }
}

void GenericToolsTest::drawBackgroundViewItemFillsRowWhenSelected()
{
    // drawBackground(viewitem) routes through option.widget->style(), so it needs a real
    // widget. It also rewrites the option before handing it over - text cleared, focus
    // stripped - and the selection has to survive that: a selected row is filled edge to
    // edge, an otherwise identical unselected one is left alone.
    QWidget host;
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled | QStyle::State_Selected | QStyle::State_Active, QRect(0, 0, 200, 30));
    opt.widget = &host;
    opt.palette = host.palette();

    QCOMPARE(paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { drawBackground(p, opt); })), opt.rect);

    opt.state &= ~QStyle::State_Selected;
    QVERIFY(paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { drawBackground(p, opt); })).isNull());
}

void GenericToolsTest::drawBackgroundMenuItemFillsRowWhenSelected()
{
    // Same claim for the menu-item overload, which clears the text and calls the style
    // that was passed in rather than the one on the option.
    QStyleOptionMenuItem opt;
    opt.rect = QRect(0, 0, 200, 30);
    opt.state = QStyle::State_Enabled | QStyle::State_Selected;
    opt.menuItemType = QStyleOptionMenuItem::Normal;
    opt.palette = QApplication::palette();

    QCOMPARE(paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { drawBackground(p, QApplication::style(), opt); })), opt.rect);

    opt.state &= ~QStyle::State_Selected;
    QVERIFY(paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { drawBackground(p, QApplication::style(), opt); })).isNull());
}

void GenericToolsTest::drawFormattedTextCentersInk()
{
    // The rendered width depends on the host font, so the ink is asserted as a
    // relationship rather than a pixel column: centered text leaves the same gap on both
    // sides of the row. The right-aligned test below pins the other branch with the same
    // string, and between them the two branches cannot be swapped unnoticed.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    const QRect ink = paintedBounds(paintedByFormattedText(opt, QStringLiteral("Aligned"), Qt::AlignHCenter));

    QVERIFY(!ink.isEmpty());
    const int leftGap = ink.left() - opt.rect.left();
    const int rightGap = opt.rect.right() - ink.right();
    QVERIFY2(qAbs(leftGap - rightGap) <= 2, qPrintable(QStringLiteral("gaps %1 / %2").arg(leftGap).arg(rightGap)));
    QVERIFY(leftGap > 20); //! clear of both edges, so neither side branch can pass this
}

void GenericToolsTest::drawFormattedTextRightAlignsInkToTheEdge()
{
    // The same string, right-aligned: the ink ends against the right edge of the row and
    // starts past its midpoint.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    const QRect ink = paintedBounds(paintedByFormattedText(opt, QStringLiteral("Aligned"), Qt::AlignRight));

    QVERIFY(!ink.isEmpty());
    const int rightGap = opt.rect.right() - ink.right();
    QVERIFY2(rightGap <= 10, qPrintable(QStringLiteral("right gap %1").arg(rightGap)));
    QVERIFY(ink.left() > opt.rect.center().x());
}

void GenericToolsTest::drawFormattedTextMenuOverloadFollowsLayoutDirection()
{
    // The menu overload always asks for AlignLeft, so the side the text ends up on is
    // decided by the layout direction alone.
    QStyleOptionMenuItem opt;
    opt.rect = QRect(0, 0, 200, 30);
    opt.state = QStyle::State_Enabled;
    opt.text = QStringLiteral("Menu");
    opt.palette = QApplication::palette();

    const QRect ltr = paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { drawFormattedText(p, opt, 1.0); }));
    QVERIFY(!ltr.isEmpty());
    QVERIFY(ltr.left() < 10);

    qApp->setLayoutDirection(Qt::RightToLeft);
    const QRect rtl = paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { drawFormattedText(p, opt, 1.0); }));
    QCOMPARE(rtl.size(), ltr.size());
    QVERIFY2(opt.rect.right() - rtl.right() <= 10, qPrintable(QStringLiteral("right gap %1").arg(opt.rect.right() - rtl.right())));
}

void GenericToolsTest::drawChangesIndicatorRtlPaintsLeftEdge()
{
    // RTL parks the same dot the same distance in from the other edge.
    qApp->setLayoutDirection(Qt::RightToLeft);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    const QImage img = paintedOn(opt.rect, [&](QPainter *p) { drawChangesIndicator(p, opt); });

    QCOMPARE(paintedBounds(img), QRect(5, 12, 7, 7));
}

void GenericToolsTest::drawScreenFollowsLayoutDirection()
{
    // The screen icon sits on the leading edge of the row: left under LTR, right under
    // RTL, with the same drawing either way.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 300, 40));

    const QRect ltr = paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { drawScreen(p, opt, false, QRect(0, 0, 1920, 1080)); }));
    QVERIFY(!ltr.isEmpty());
    QVERIFY(ltr.right() < opt.rect.center().x());

    qApp->setLayoutDirection(Qt::RightToLeft);
    const QRect rtl = paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { drawScreen(p, opt, false, QRect(0, 0, 1920, 1080)); }));
    QVERIFY(rtl.left() > opt.rect.center().x());
    QCOMPARE(rtl.size(), ltr.size());
}

void GenericToolsTest::drawScreenMultipleScreensOutlinesBehindTheScreen()
{
    // The multiple-screens hint adds two lines up and to the left of the screen, as if a
    // second screen were stacked behind it. It must not move the screen itself, so the
    // ink grows on the top-left only and the available rect handed back is unchanged.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 300, 40));
    QRect singleAvail;
    QRect multipleAvail;

    const QRect single = paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { singleAvail = drawScreen(p, opt, false, QRect(0, 0, 1920, 1080)); }));
    const QRect multiple = paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { multipleAvail = drawScreen(p, opt, true, QRect(0, 0, 1920, 1080)); }));

    QVERIFY(multiple.left() < single.left());
    QVERIFY(multiple.top() < single.top());
    QCOMPARE(multiple.right(), single.right());
    QCOMPARE(multiple.bottom(), single.bottom());
    QCOMPARE(multipleAvail, singleAvail);
}

void GenericToolsTest::drawScreenPortraitGeometryPaintsNarrowerScreen()
{
    // A portrait screen geometry is drawn as a portrait screen: the same row height, but
    // the outline and the available rect inside it are narrower and taller.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 300, 40));
    QRect landscapeAvail;
    QRect portraitAvail;

    const QRect landscape = paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { landscapeAvail = drawScreen(p, opt, false, QRect(0, 0, 1920, 1080)); }));
    const QRect portrait = paintedBounds(paintedOn(opt.rect, [&](QPainter *p) { portraitAvail = drawScreen(p, opt, false, QRect(0, 0, 1080, 1920)); }));

    QVERIFY(portrait.width() < landscape.width());
    QVERIFY(portrait.height() > landscape.height());
    QVERIFY(portraitAvail.width() < landscapeAvail.width());
    QVERIFY(portraitAvail.height() > landscapeAvail.height());
}

void GenericToolsTest::iconPaintersShareTheSlot_data()
{
    QTest::addColumn<Qt::AlignmentFlag>("alignment");
    QTest::addColumn<Qt::LayoutDirection>("direction");
    QTest::addColumn<QRect>("painted");

    // Geometry measured from the run, not derived: a 200x30 row with the default margins
    // puts a 28px icon at x=3 on the leading edge and x=169 on the trailing one, and RTL
    // swaps which alignment lands where.
    QTest::newRow("left-ltr") << Qt::AlignLeft << Qt::LeftToRight << QRect(3, 1, 28, 28);
    QTest::newRow("right-ltr") << Qt::AlignRight << Qt::LeftToRight << QRect(169, 1, 28, 28);
    QTest::newRow("left-rtl") << Qt::AlignLeft << Qt::RightToLeft << QRect(169, 1, 28, 28);
    QTest::newRow("right-rtl") << Qt::AlignRight << Qt::RightToLeft << QRect(3, 1, 28, 28);
    // Centered is where the painters used to diverge: drawIcon had no centered branch and
    // dropped the icon at the right edge, and in RTL it mapped AlignHCenter onto AlignLeft
    // and parked it at the left one. A centered slot has no side, so both directions pin
    // the same rect.
    QTest::newRow("center-ltr") << Qt::AlignHCenter << Qt::LeftToRight << QRect(86, 1, 28, 28);
    QTest::newRow("center-rtl") << Qt::AlignHCenter << Qt::RightToLeft << QRect(86, 1, 28, 28);
}

void GenericToolsTest::iconPaintersShareTheSlot()
{
    // Four painters fill the same slot: a themed icon, a layout icon, the layout
    // background circle and the colour-scheme swatches. They all route through
    // iconTargetRect, so asserting they agree with each other is a tautology no
    // implementation can fail - the placement is pinned against absolute geometry.
    QFETCH(Qt::AlignmentFlag, alignment);
    QFETCH(Qt::LayoutDirection, direction);
    QFETCH(QRect, painted);

    qApp->setLayoutDirection(direction);
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));

    QCOMPARE(paintedBounds(paintedByIcon(opt, m_themeIcon, alignment)), painted);
    QCOMPARE(paintedBounds(paintedByLayoutIcon(opt, m_themeIcon, alignment)), painted);
    QCOMPARE(paintedBounds(paintedByLayoutBackground(opt, alignment)), painted);
    QCOMPARE(paintedBounds(paintedByColorSchemeIcon(opt, alignment)), painted);
}

void GenericToolsTest::remainedFromIconExactSlot()
{
    // Pin the whole slot arithmetic: lenmargin 3 + iconsize 28 + lenmargin 3 = 34
    // reserved. Left-aligned pushes the remaining rect past the slot, right-aligned
    // only shrinks it - deriving one from the icon target rect would lose a margin.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    QCOMPARE(remainedFromIcon(opt, Qt::AlignLeft), QRect(34, 0, 166, 30));
    QCOMPARE(remainedFromIcon(opt, Qt::AlignRight), QRect(0, 0, 166, 30));

    qApp->setLayoutDirection(Qt::RightToLeft);
    QCOMPARE(remainedFromIcon(opt, Qt::AlignLeft), QRect(0, 0, 166, 30));
    QCOMPARE(remainedFromIcon(opt, Qt::AlignRight), QRect(34, 0, 166, 30));
}

void GenericToolsTest::remainedFromIconCenteredIgnoresDirection()
{
    // A centered slot has no side to mirror, so the layout direction must not move
    // the remaining rect. No production caller reaches this path today - both
    // wrappers hand back the full rect for AlignHCenter - but the default argument
    // makes it reachable.
    QStyleOptionViewItem opt = makeOption(QStyle::State_Enabled, QRect(0, 0, 200, 30));
    const QRect ltr = remainedFromIcon(opt, Qt::AlignHCenter);

    qApp->setLayoutDirection(Qt::RightToLeft);
    QCOMPARE(remainedFromIcon(opt, Qt::AlignHCenter), ltr);
}

QTEST_MAIN(GenericToolsTest)

#include "generictoolstest.moc"
