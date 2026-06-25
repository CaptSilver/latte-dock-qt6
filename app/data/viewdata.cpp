/*
    SPDX-FileCopyrightText: 2021 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "viewdata.h"

namespace Latte {
namespace Data {

const int View::ISCLONEDNULL = -1;

View::View()
    : Generic()
{
}

View::View(View &&o)
    : Generic(o),
      isActive(o.isActive),
      isMoveOrigin(o.isMoveOrigin),
      isMoveDestination(o.isMoveDestination),
      onPrimary(o.onPrimary),
      isClonedFrom(o.isClonedFrom),
      screen(o.screen),
      screenEdgeMargin(o.screenEdgeMargin),
      screensGroup(o.screensGroup),
      maxLength(o.maxLength),
      edge(o.edge),
      alignment(o.alignment),
      m_state(o.m_state),
      m_originFile(o.m_originFile),
      m_originLayout(o.m_originLayout),
      m_originView(o.m_originView),
      errors(o.errors),
      warnings(o.warnings),
      subcontainments(o.subcontainments)
{
}

View::View(const View &o)
    : Generic(o),
      isActive(o.isActive),
      isMoveOrigin(o.isMoveOrigin),
      isMoveDestination(o.isMoveDestination),
      onPrimary(o.onPrimary),
      isClonedFrom(o.isClonedFrom),
      screen(o.screen),
      screenEdgeMargin(o.screenEdgeMargin),
      screensGroup(o.screensGroup),
      maxLength(o.maxLength),
      edge(o.edge),
      alignment(o.alignment),
      m_state(o.m_state),
      m_originFile(o.m_originFile),
      m_originLayout(o.m_originLayout),
      m_originView(o.m_originView),
      errors(o.errors),
      warnings(o.warnings),
      subcontainments(o.subcontainments)
{
}

View::View(const QString &newid, const QString &newname)
    : Generic(newid, newname)
{
}

View &View::operator=(const View &rhs)
{
    id = rhs.id;
    name = rhs.name;
    isActive = rhs.isActive;
    isMoveOrigin = rhs.isMoveOrigin;
    isMoveDestination = rhs.isMoveDestination;
    onPrimary = rhs.onPrimary;
    isClonedFrom = rhs.isClonedFrom;
    screen = rhs.screen;
    screenEdgeMargin = rhs.screenEdgeMargin;
    screensGroup = rhs.screensGroup;
    maxLength = rhs.maxLength;
    edge = rhs.edge;
    alignment = rhs.alignment;
    m_state = rhs.m_state;
    m_originFile = rhs.m_originFile;
    m_originLayout = rhs.m_originLayout;
    m_originView = rhs.m_originView;
    errors = rhs.errors;
    warnings = rhs.warnings;

    subcontainments = rhs.subcontainments;

    return (*this);
}

View &View::operator=(View &&rhs)
{
    id = rhs.id;
    name = rhs.name;
    isActive = rhs.isActive;
    isMoveOrigin = rhs.isMoveOrigin;
    isMoveDestination = rhs.isMoveDestination;
    onPrimary = rhs.onPrimary;
    isClonedFrom = rhs.isClonedFrom;
    screen = rhs.screen;
    screenEdgeMargin = rhs.screenEdgeMargin;
    screensGroup = rhs.screensGroup;
    maxLength = rhs.maxLength;
    edge = rhs.edge;
    alignment = rhs.alignment;
    m_state = rhs.m_state;
    m_originFile = rhs.m_originFile;
    m_originLayout = rhs.m_originLayout;
    m_originView = rhs.m_originView;
    errors = rhs.errors;
    warnings = rhs.warnings;

    subcontainments = rhs.subcontainments;

    return (*this);
}

bool View::operator==(const View &rhs) const
{
    return (id == rhs.id)
            && (name == rhs.name)
            //&& (isActive == rhs.isActive) /*Disabled because this is not needed in order to track view changes for saving*/
            //&& (isMoveOrigin == rhs.isMoveOrigin) /*Disabled because this is not needed in order to track view changes for saving*/
            //&& (isMoveDestination == rhs.isMoveDestination) /*Disabled because this is not needed in order to track view changes for saving*/
            && (onPrimary == rhs.onPrimary)
            && (isClonedFrom == rhs.isClonedFrom)
            && (screen == rhs.screen)
            && (screenEdgeMargin == rhs.screenEdgeMargin)
            && (screensGroup == rhs.screensGroup)
            && (maxLength == rhs.maxLength)
            && (edge == rhs.edge)
            && (alignment == rhs.alignment)
            && (m_state == rhs.m_state)
            && (m_originFile == rhs.m_originFile)
            && (m_originLayout == rhs.m_originLayout)
            && (m_originView == rhs.m_originView)
            //&& (errors == rhs.errors) /*Disabled because this is not needed in order to track view changes for saving*/
            //&& (warnings == rhs.warnings) /*Disabled because this is not needed in order to track view changes for saving*/
            && (subcontainments == rhs.subcontainments);
}

bool View::operator!=(const View &rhs) const
{
    return !(*this == rhs);
}

View::operator QString() const
{
    QString result;

    result += id;
    result += QStringLiteral(" : ");
    result += isActive ? QStringLiteral("Active") : QStringLiteral("Inactive");
    result += QStringLiteral(" : ");
    if (m_state==OriginFromLayout && isMoveOrigin && isMoveDestination) {
        result += QStringLiteral(" ↑↓ ");
    } else if (m_state==OriginFromLayout && isMoveOrigin) {
        result += QStringLiteral(" ↑ ");
    } else if (m_state==OriginFromLayout && isMoveDestination) {
        result += QStringLiteral(" ↓ ");
    } else {
        result += QStringLiteral(" - ");
    }

    result += QStringLiteral(" : ");

    if (m_state == IsInvalid) {
        result += QStringLiteral("IsInvalid");
    } else if (m_state == IsCreated) {
        result += QStringLiteral("IsCreated");
    } else if (m_state == OriginFromViewTemplate) {
        result += QStringLiteral("OriginFromViewTemplate");
    } else if (m_state == OriginFromLayout) {
        result += QStringLiteral("OriginFromLayout");
    }

    result += QStringLiteral(" : ");
    if (isCloned()) {
        result += QStringLiteral("Cloned from:") + QString::number(isClonedFrom);
    } else {
        result += QStringLiteral("Original");
    }

    result += QStringLiteral(" : ");
    if (screensGroup == Latte::Types::SingleScreenGroup) {
        result += onPrimary ? QStringLiteral("Primary") : QStringLiteral("Explicit");
    } else if (screensGroup == Latte::Types::AllScreensGroup) {
        result += QStringLiteral("All Screens");
    } else if (screensGroup == Latte::Types::AllSecondaryScreensGroup) {
        result += QStringLiteral("All Secondary Screens");
    }

    result += QStringLiteral(" : ");
    result += QString::number(screen);
    result += QStringLiteral(" : ");
    if (edge == Plasma::Types::BottomEdge) {
        result += QStringLiteral("BottomEdge");
    } else if (edge == Plasma::Types::TopEdge) {
        result += QStringLiteral("TopEdge");
    } else if (edge == Plasma::Types::LeftEdge) {
        result += QStringLiteral("LeftEdge");
    } else if (edge == Plasma::Types::RightEdge) {
        result += QStringLiteral("RightEdge");
    }

    result += QStringLiteral(" : ");

    if (alignment == Latte::Types::Center) {
        result += QStringLiteral("CenterAlignment");
    } else if (alignment == Latte::Types::Left) {
        result += QStringLiteral("LeftAlignment");
    } else if (alignment == Latte::Types::Right) {
        result += QStringLiteral("RightAlignment");
    } else if (alignment == Latte::Types::Top) {
        result += QStringLiteral("TopAlignment");
    } else if (alignment == Latte::Types::Bottom) {
        result += QStringLiteral("BottomAlignment");
    } else if (alignment == Latte::Types::Justify) {
        result += QStringLiteral("JustifyAlignment");
    }

    result += QStringLiteral(" : ");
    result += QString::number(maxLength) + QStringLiteral("%");

    result += QStringLiteral(" || ");
    result += QStringLiteral("{") + static_cast<QString>(subcontainments) + QStringLiteral("}");

    return result;
}

bool View::isCreated() const
{
    return m_state == IsCreated;
}

bool View::isOriginal() const
{
    return !isCloned();
}

bool View::isCloned() const
{
    return isClonedFrom != ISCLONEDNULL;
}

bool View::isValid() const
{
    return m_state != IsInvalid;
}

bool View::isHorizontal() const
{
    return !isVertical();
}

bool View::isVertical() const
{
    return (edge == Plasma::Types::LeftEdge || edge == Plasma::Types::RightEdge);
}

bool View::hasViewTemplateOrigin() const
{
    return m_state == OriginFromViewTemplate;
}

bool View::hasLayoutOrigin() const
{
    return m_state == OriginFromLayout;
}

bool View::hasSubContainment(const QString &subId) const
{
    return subcontainments.containsId(subId);
}

QString View::originFile() const
{
    return m_originFile;
}

QString View::originLayout() const
{
    return m_originLayout;
}

QString View::originView() const
{
    return m_originView;
}

View::State View::state() const
{
    return m_state;
}

void View::setState(View::State state, QString file, QString layout, QString view)
{
    m_state = state;
    m_originFile = file;
    m_originLayout = layout;
    m_originView = view;
}

}
}
