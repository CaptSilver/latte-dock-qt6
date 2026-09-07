/*
    SPDX-FileCopyrightText: 2019 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "generictools.h"

// Qt
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QStyle>
#include <QTextDocument>
#include <QWidget>

namespace Latte {

const int ICONMARGIN = 1;
const int INDICATORCHANGESLENGTH = 6;
const int INDICATORCHANGESMARGIN = 5;
const int MARGIN = 2;

namespace {

//! The icon slot resolved out of the -1 sentinels: the icon square plus the length
//! margin on either side of it.
struct IconMetrics
{
    int lenmargin;
    int thickmargin;
    int iconsize;
    int total;
};

IconMetrics iconMetrics(const QStyleOption &option, int lengthMargin, int thickMargin)
{
    IconMetrics metrics;
    metrics.lenmargin = (lengthMargin == -1 ? ICONMARGIN + MARGIN : lengthMargin);
    metrics.thickmargin = (thickMargin == -1 ? ICONMARGIN : thickMargin);
    metrics.iconsize = option.rect.height() - 2*metrics.thickmargin;
    metrics.total = metrics.iconsize + 2*metrics.lenmargin;
    return metrics;
}

//! Where the alignment ends up on screen: RTL mirrors the two sides, but a centered
//! slot has no side to mirror and must stay put.
Qt::AlignmentFlag visualAlignment(Qt::AlignmentFlag alignment)
{
    if (qApp->layoutDirection() == Qt::LeftToRight || alignment == Qt::AlignHCenter) {
        return alignment;
    }

    return alignment == Qt::AlignLeft ? Qt::AlignRight : Qt::AlignLeft;
}

QRect iconTargetRect(const QStyleOption &option, Qt::AlignmentFlag alignment, int lengthMargin, int thickMargin)
{
    const IconMetrics metrics = iconMetrics(option, lengthMargin, thickMargin);
    const Qt::AlignmentFlag curalign = visualAlignment(alignment);

    int x = option.rect.x() + metrics.lenmargin;

    if (curalign == Qt::AlignRight) {
        x += option.rect.width() - metrics.total;
    } else if (curalign != Qt::AlignLeft) {
        //! anything not pinned to a side is centered in the row
        x += (option.rect.width() - metrics.total)/2;
    }

    return QRect(x, option.rect.y() + metrics.thickmargin, metrics.iconsize, metrics.iconsize);
}

}

bool isEnabled(const QStyleOption &option)
{
    if (option.state & QStyle::State_Enabled) {
        return true;
    }

    return false;
}

bool isActive(const QStyleOption &option)
{
    if (option.state & QStyle::State_Active) {
        return true;
    }

    return false;
}

bool isSelected(const QStyleOption &option)
{
    if (option.state & QStyle::State_Selected) {
        return true;
    }

    return false;
}

bool isHovered(const QStyleOption &option)
{
    if (option.state & QStyle::State_MouseOver) {
        return true;
    }

    return false;
}

bool isFocused(const QStyleOption &option)
{
    if (option.state & QStyle::State_HasFocus) {
        return true;
    }

    return false;
}

bool isTextCentered(const QStyleOptionViewItem &option)
{
    if (option.displayAlignment & Qt::AlignHCenter) {
        return true;
    }

    return false;
}

Qt::AlignmentFlag horizontalAlignment(Qt::Alignment alignments)
{
    if (alignments & Qt::AlignHCenter) {
        return Qt::AlignHCenter;
    } else if (alignments & Qt::AlignRight) {
        return Qt::AlignRight;
    }

    return Qt::AlignLeft;
}

QPalette::ColorGroup colorGroup(const QStyleOption &option)
{
    if (!isEnabled(option)) {
        return QPalette::Disabled;
    }

    if (isActive(option) || isFocused(option)) {
        return QPalette::Active;
    }

    if (!isActive(option) && isSelected(option)) {
        return QPalette::Inactive;
    }

    return QPalette::Normal;
}

QStringList subtracted(const QStringList &original, const QStringList &current)
{
    QStringList subtract;

    for(int i=0; i<original.count(); ++i) {
        if (!current.contains(original[i])) {
            subtract << original[i];
        }
    }

    return subtract;
}

void drawFormattedText(QPainter *painter, const QStyleOptionViewItem &option, const float textOpacity)
{
    drawFormattedText(painter, option, option.text, horizontalAlignment(option.displayAlignment), textOpacity);
}

void drawFormattedText(QPainter *painter, const QStyleOptionMenuItem &option, const float textOpacity)
{
    drawFormattedText(painter, option, option.text, Qt::AlignLeft, textOpacity);
}

QRect remainedFromFormattedText(const QStyleOption &option, const QString &text, Qt::AlignmentFlag alignment)
{
    QString css = QStringLiteral("body {}");

    QTextDocument doc;
    doc.setDefaultStyleSheet(css);
    doc.setHtml(QStringLiteral("<body>") + text + QStringLiteral("</body>"));

    //we need an offset to be in the same vertical center of TextEdit
    int textWidth = doc.size().width() + MARGIN;

    Qt::AlignmentFlag curalign = alignment;

    if (qApp->layoutDirection() == Qt::LeftToRight || (curalign == Qt::AlignHCenter)) {
        curalign = alignment;
    } else {
        curalign = alignment == Qt::AlignLeft ? Qt::AlignRight : Qt::AlignLeft;
    }

    if (alignment == Qt::AlignHCenter) {
        return option.rect;
    } else if (curalign == Qt::AlignRight) {
        return QRect(option.rect.x(), option.rect.y(), option.rect.width() - textWidth, option.rect.height());
    } else {
        return QRect(option.rect.x() + textWidth, option.rect.y(), option.rect.width() - textWidth, option.rect.height());
    }
}

void drawFormattedText(QPainter *painter, const QStyleOption &option, const QString &text, Qt::AlignmentFlag alignment, const float textOpacity)
{
    painter->save();

    QPalette::ColorRole applyColor = Latte::isSelected(option) ? QPalette::HighlightedText : QPalette::Text;
    QBrush nBrush = option.palette.brush(Latte::colorGroup(option), applyColor);

    QColor brushColor = nBrush.color();
    brushColor.setAlphaF(textOpacity);

    QString css = QStringLiteral("body { color : %1;}").arg(brushColor.name(QColor::HexArgb));

    QTextDocument doc;
    doc.setDefaultStyleSheet(css);
    doc.setHtml(QStringLiteral("<body>") + text + QStringLiteral("</body>"));

    //we need an offset to be in the same vertical center of TextEdit
    int offsetY = ((option.rect.height() - doc.size().height()) / 2);
    int textWidth = doc.size().width();
    int textY = option.rect.top() + offsetY + 1;

    Qt::AlignmentFlag curalign = alignment;

    if (qApp->layoutDirection() == Qt::LeftToRight || (curalign == Qt::AlignHCenter)) {
        curalign = alignment;
    } else {
        curalign = alignment == Qt::AlignLeft ? Qt::AlignRight : Qt::AlignLeft;
    }

    if (alignment == Qt::AlignHCenter) {
        int textX = qMax(0, (option.rect.width() / 2) - (textWidth/2));
        painter->translate(option.rect.left() + textX, textY);
    } else if (curalign == Qt::AlignRight) {
        painter->translate(qMax(option.rect.left(), option.rect.right() - textWidth), textY);
    } else {
        painter->translate(option.rect.left(), textY);
    }

    QRect clip(0, 0, option.rect.width(), option.rect.height());
    doc.drawContents(painter, clip);

    painter->restore();
}

void drawBackground(QPainter *painter, const QStyleOptionViewItem &option)
{
    QStyleOptionViewItem backOption = option;
    backOption.text = QStringLiteral("");

    //! Remove the focus dotted lines
    backOption.state = (option.state & ~QStyle::State_HasFocus);

    option.widget->style()->drawControl(QStyle::CE_ItemViewItem, &backOption, painter);
}

void drawBackground(QPainter *painter, const QStyle *style, const QStyleOptionMenuItem &option)
{
    QStyleOptionMenuItem backOption = option;
    backOption.text = QStringLiteral("");
    //! Remove the focus dotted lines
    //   iconOption.state = (option.state & ~QStyle::State_HasFocus);

    style->drawControl(QStyle::CE_MenuItem, &backOption, painter);
}

void initMenuItemOption(QStyleOptionMenuItem &option, const QWidget *widget, const QAction *action)
{
    //! initFrom() carries the rect, state, palette and font metrics of the hosting widget
    option.initFrom(widget);

    //! Breeze rebuilds the width of a Normal item out of these two and ignores the size handed
    //! to sizeFromContents(), so leaving them unset collapses every row to the same stub width
    option.text = action->text();
    option.font = widget->font();

    option.menuItemType = QStyleOptionMenuItem::Normal;

    //! these widgets lay out a radio button and reserve its column while painting, whether or
    //! not the action itself is checkable, so the measured row has to reserve it as well
    option.menuHasCheckableItems = true;
    option.checkType = QStyleOptionMenuItem::Exclusive;
    option.checked = action->isChecked();

    option.maxIconWidth = widget->style()->pixelMetric(QStyle::PM_SmallIconSize, &option, widget);
}

QRect remainedFromLayoutIcon(const QStyleOption &option, Qt::AlignmentFlag alignment, int lengthMargin, int thickMargin)
{
    if (alignment == Qt::AlignHCenter) {
        return option.rect;
    }

    return remainedFromIcon(option, alignment, lengthMargin, thickMargin);
}

void drawLayoutIcon(QPainter *painter, const QStyleOption &option, const bool &isBackgroundFile, const QString &iconName, Qt::AlignmentFlag alignment, int lengthMargin, int thickMargin)
{
    bool active = Latte::isActive(option);
    bool selected = Latte::isSelected(option);
    bool focused = Latte::isFocused(option);

    const QRect target = iconTargetRect(option, alignment, lengthMargin, thickMargin);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (isBackgroundFile) {
        int backImageMargin = 1; //most icon themes provide 1-2px. padding around icons //OLD CALCS: ICONMARGIN; //qMin(target.height()/4, ICONMARGIN+1);
        QRect backTarget(target.x() + backImageMargin, target.y() + backImageMargin, target.width() - 2*backImageMargin, target.height() - 2*backImageMargin);

        QPixmap backImage(iconName);
        backImage = backImage.copy(backTarget);

        QPalette::ColorRole textColorRole = selected ? QPalette::HighlightedText : QPalette::Text;

        QBrush imageBrush(backImage);
        QPen pen; pen.setWidth(1);
        pen.setColor(option.palette.color(Latte::colorGroup(option), textColorRole));

        painter->setBrush(imageBrush);
        painter->setPen(pen);

        painter->drawEllipse(backTarget);
    } else {
        QIcon::Mode mode = ((active && (selected || focused)) ? QIcon::Selected : QIcon::Normal);

        painter->drawPixmap(target, QIcon::fromTheme(iconName).pixmap(target.height(), target.height(), mode));
    }

    painter->restore();
}

QRect remainedFromColorSchemeIcon(const QStyleOption &option, Qt::AlignmentFlag alignment, int lengthMargin, int thickMargin)
{
    if (alignment == Qt::AlignHCenter) {
        return option.rect;
    }

    return remainedFromIcon(option, alignment, lengthMargin, thickMargin);
}

void drawColorSchemeIcon(QPainter *painter, const QStyleOption &option, const QColor &textColor, const QColor &backgroundColor, Qt::AlignmentFlag alignment, int lengthMargin, int thickMargin)
{
    bool active = Latte::isActive(option);
    bool selected = Latte::isSelected(option);
    bool focused = Latte::isFocused(option);

    const QRect target = iconTargetRect(option, alignment, lengthMargin, thickMargin);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    int backImageMargin = 0; //most icon themes provide 1-2px. padding around icons //OLD CALCS: ICONMARGIN; //qMin(target.height()/4, ICONMARGIN+1);
    QRect backTarget(target.x() + backImageMargin, target.y() + backImageMargin, target.width() - 2*backImageMargin, target.height() - 2*backImageMargin);

    QPalette::ColorRole textColorRole = selected ? QPalette::HighlightedText : QPalette::Text;

    QBrush colorbrush(backgroundColor);
    QPen pen; pen.setWidth(1);
    pen.setColor(option.palette.color(Latte::colorGroup(option), textColorRole));

    painter->setBrush(colorbrush);
    painter->setPen(pen);

    int rectsize = 0.7 * backTarget.width();
    int gap = backTarget.width() - rectsize;

    painter->drawRect(backTarget.right() - rectsize, backTarget.bottom() - rectsize, rectsize, rectsize);

    colorbrush.setColor(textColor);
    painter->setBrush(colorbrush);
    painter->drawRect(backTarget.x(), backTarget.y(), rectsize, rectsize);

    painter->restore();
}


QRect remainedFromIcon(const QStyleOption &option, Qt::AlignmentFlag alignment, int lengthMargin, int thickMargin)
{
    //! The reserved slot includes both length margins, so this is deliberately not
    //! derived from the icon target rect - that one starts a margin further in.
    const int total = iconMetrics(option, lengthMargin, thickMargin).total;
    const Qt::AlignmentFlag curalign = visualAlignment(alignment);

    QRect optionRemainedRect = (curalign == Qt::AlignLeft) ? QRect(option.rect.x() + total, option.rect.y(), option.rect.width() - total, option.rect.height()) :
                                                             QRect(option.rect.x(), option.rect.y(), option.rect.width() - total, option.rect.height());

    return optionRemainedRect;
}

void drawIcon(QPainter *painter, const QStyleOption &option, const QString &icon, Qt::AlignmentFlag alignment, int lengthMargin, int thickMargin)
{
    bool active = Latte::isActive(option);
    bool selected = Latte::isSelected(option);
    bool focused = Latte::isFocused(option);

    QIcon::Mode mode = ((active && (selected || focused)) ? QIcon::Selected : QIcon::Normal);

    const QRect target = iconTargetRect(option, alignment, lengthMargin, thickMargin);

    painter->drawPixmap(target, QIcon::fromTheme(icon).pixmap(target.height(), target.height(), mode));
}

int primitiveCheckBoxWidth(const QStyleOptionButton &option, const QWidget *widget)
{
    QStyleOption copt;
    copt.rect = option.rect;
    int w = QApplication::style()->sizeFromContents(QStyle::CT_CheckBox, &copt, QSize(0, option.rect.height()), widget).width();
    w = w > 0 ? w : option.rect.height() - 2*MARGIN;
    return w;
}

QRect remainedFromCheckBox(const QStyleOptionButton &option, Qt::AlignmentFlag alignment, const QWidget *widget)
{
    int length = primitiveCheckBoxWidth(option, widget) - MARGIN;
    Qt::AlignmentFlag curalign = alignment;

    if (qApp->layoutDirection() == Qt::LeftToRight) {
        curalign = alignment;
    } else {
        curalign = alignment == Qt::AlignLeft ? Qt::AlignRight : Qt::AlignLeft;
    }

    QRect optionRemainedRect = (curalign == Qt::AlignLeft) ? QRect(option.rect.x() + length, option.rect.y(), option.rect.width() - length, option.rect.height()) :
                                                             QRect(option.rect.x(), option.rect.y(), option.rect.width() - length, option.rect.height());

    return optionRemainedRect;
}

void drawCheckBox(QPainter *painter, const QStyleOptionButton &option, Qt::AlignmentFlag alignment, const QWidget *widget)
{
    int length = primitiveCheckBoxWidth(option, widget) - MARGIN;
    QStyleOptionButton optionbtn = option;

    Qt::AlignmentFlag curalign = alignment;

    if (qApp->layoutDirection() == Qt::LeftToRight) {
        curalign = alignment;
    } else {
        curalign = alignment == Qt::AlignLeft ? Qt::AlignRight : Qt::AlignLeft;
    }

    QRect changesrect = (curalign == Qt::AlignLeft) ? QRect(option.rect.x() + MARGIN, option.rect.y(), length - MARGIN, option.rect.height()) :
                                                      QRect(option.rect.x() + option.rect.width() - length, option.rect.y(), length - MARGIN, option.rect.height());

    optionbtn.rect = changesrect;

    QApplication::style()->drawControl(QStyle::CE_CheckBox, &optionbtn, painter, widget);
}

QRect remainedFromChangesIndicator(const QStyleOptionViewItem &option)
{
    int tsize{INDICATORCHANGESLENGTH + INDICATORCHANGESMARGIN*2};

    QRect optionRemainedRect = (qApp->layoutDirection() == Qt::RightToLeft) ? QRect(option.rect.x() + tsize, option.rect.y(), option.rect.width() - tsize, option.rect.height()) :
                                                                              QRect(option.rect.x(), option.rect.y(), option.rect.width() - tsize, option.rect.height());

    return optionRemainedRect;
}

void drawChangesIndicator(QPainter *painter, const QStyleOptionViewItem &option)
{
    //! draw changes circle indicator
    int csize{INDICATORCHANGESLENGTH};
    int tsize{INDICATORCHANGESLENGTH + INDICATORCHANGESMARGIN*2};

    painter->save();

    QRect changesRect = (qApp->layoutDirection() == Qt::RightToLeft) ? QRect(option.rect.x() + INDICATORCHANGESMARGIN, option.rect.y() + option.rect.height()/2 - csize/2, csize, csize) :
                                                                       QRect(option.rect.x() + option.rect.width() - csize - INDICATORCHANGESMARGIN, option.rect.y() + option.rect.height()/2 - csize/2, csize, csize);

    QColor plasmaOrange(246, 116, 0); //orangish color used from plasma systemsettings #f67400
    QBrush backBrush(plasmaOrange);
    QPen pen; pen.setWidth(1);
    pen.setColor(plasmaOrange);

    painter->setBrush(backBrush);
    painter->setPen(pen);
    painter->drawEllipse(changesRect);

    painter->restore();
}

int screenMaxLength(const QStyleOption &option, const int &maxIconSize)
{
    int icon_length = maxIconSize >= 0 ? qMin(option.rect.height(), maxIconSize) : option.rect.height();

    int scr_maxlength = icon_length * 1.7;

    //! provide odd screen_maxlength
    if (scr_maxlength % 2 == 0) {
        scr_maxlength--;
    }

    return scr_maxlength;
}

QRect remainedFromScreenDrawing(const QStyleOption &option, bool drawMultipleScreens, const int &maxIconSize)
{
    int total_length = screenMaxLength(option, maxIconSize) + MARGIN * 2 + 1;

    QRect optionRemainedRect = (qApp->layoutDirection() == Qt::RightToLeft) ? QRect(option.rect.x(), option.rect.y(), option.rect.width() - total_length, option.rect.height()) :
                                                                              QRect(option.rect.x() + total_length, option.rect.y(), option.rect.width() - total_length, option.rect.height());

    return optionRemainedRect;
}

QRect drawScreen(QPainter *painter, const QStyleOption &option, bool drawMultipleScreens, QRect screenGeometry, const int &maxIconSize, const float brushOpacity)
{
    float scr_ratio = (float)screenGeometry.width() / (float)screenGeometry.height();
    bool isVertical = (scr_ratio < 1.0);

    int scr_maxlength = screenMaxLength(option, maxIconSize);
    int scr_maxthickness = maxIconSize >= 0 ? qMin(maxIconSize, option.rect.height() - MARGIN * 2) : option.rect.height() - MARGIN * 2;

    int total_length = scr_maxlength + MARGIN * 2;
    int pen_width = 2;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    float scr_maxratio = ((float)scr_maxlength) / (float)(scr_maxthickness);
    scr_ratio = qMin(qMax((float)0.75, scr_ratio), (float)scr_maxratio);
    int scr_height = (!isVertical ? scr_maxthickness - MARGIN * 4 : scr_maxthickness - MARGIN * 2);
    int scr_width = scr_ratio * scr_height;

    //! provide even screen width and height
    if (scr_width % 2 == 1) {
        scr_width++;
    }

    //! provide even screen width and height
    if (scr_height % 2 == 0) {
        scr_height++;
    }

    int topmargin = (option.rect.height() - scr_maxthickness) / 2;

    QRect screenMaximumRect = (qApp->layoutDirection() == Qt::RightToLeft) ?
                QRect(option.rect.x() + option.rect.width() - scr_maxlength - MARGIN, option.rect.y() + topmargin, scr_maxlength, scr_maxthickness - 1) :
                QRect(option.rect.x() + MARGIN , option.rect.y() + topmargin, scr_maxlength, scr_maxthickness - 1);

    int topScreenMargin = (screenMaximumRect.height() - scr_height) / 2;
    int leftScreenMargin = (screenMaximumRect.width() - scr_width) / 2;

    QRect screenRect(screenMaximumRect.x() + leftScreenMargin + MARGIN/2, screenMaximumRect.y() + topScreenMargin, scr_width, scr_height);

    QRect screenAvailableRect(screenRect.x() + pen_width - 1, screenRect.y() + pen_width - 1, screenRect.width() - pen_width - 1, screenRect.height() - pen_width - 1);

    bool selected = Latte::isSelected(option);
    QPalette::ColorRole textColorRole = selected ? QPalette::HighlightedText : QPalette::Text;

    QPen pen; pen.setWidth(pen_width);
    QColor pencolor = option.palette.color(Latte::colorGroup(option), textColorRole);
    pencolor.setAlphaF(brushOpacity);
    pen.setColor(pencolor);

    painter->setPen(pen);
    painter->drawRect(screenRect);

    //! draw screen base
    pen.setWidth(1);
    painter->setPen(pen);
    painter->setRenderHint(QPainter::Antialiasing, false);

    //! draw multiple
    if (drawMultipleScreens) {
        int multiplemargin = 3;
        int curx = screenRect.x()-multiplemargin;
        painter->drawLine(screenRect.x() - multiplemargin, screenRect.y() - multiplemargin,
                          screenRect.x() - multiplemargin, screenRect.y() - multiplemargin + screenRect.height());
        painter->drawLine(screenRect.x() - multiplemargin, screenRect.y() - multiplemargin,
                          screenRect.x() - multiplemargin + screenRect.width(), screenRect.y() - multiplemargin);
    }

    int basex = screenRect.x() + (screenRect.width()/2) - 4;
    int basey = screenRect.y() + screenRect.height() + 2;

    painter->drawLine(basex , basey, basex + 8, basey);

    // debug screen maximum available rect
    //painter->drawRect(screenMaximumRect);

    painter->restore();

    return screenAvailableRect;
}

}

