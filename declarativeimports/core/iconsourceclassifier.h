/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QIcon>
#include <QImage>
#include <QLatin1String>
#include <QString>
#include <QUrl>
#include <QVariant>

namespace Latte {
namespace IconSourceClassifier {

enum class SourceKind { LocalFile, SvgOrIconName, Icon, Image, Clear };

//! Source-name derivation: toString(), but prefer a non-empty QIcon::name() when the
//! variant holds a QIcon (mirrors iconitem.cpp ~75-80). classify() calls this, not
//! re-derives, so a named QIcon doesn't misroute to the Icon branch.
inline QString sourceName(const QVariant &source)
{
    QString name = source.toString();
    if (source.canConvert<QIcon>() && !source.value<QIcon>().name().isEmpty()) {
        name = source.value<QIcon>().name();
    }
    return name;
}

//! The setSource() if/else ladder reduced to a classification. Derives the (possibly
//! QIcon-overridden) name first, branches on its emptiness, then QIcon before QImage.
inline SourceKind classify(const QVariant &source)
{
    const QString name = sourceName(source);
    if (!name.isEmpty()) {
        return QUrl(name).isLocalFile() ? SourceKind::LocalFile : SourceKind::SvgOrIconName;
    }
    if (source.canConvert<QIcon>())  { return SourceKind::Icon; }
    if (source.canConvert<QImage>()) { return SourceKind::Image; }
    return SourceKind::Clear;
}

//! setLastValidSourceName guard (~:207): true == do NOT record this as the last valid name.
inline bool isFilteredSourceName(const QString &name)
{
    return name.isEmpty() || name == QLatin1String("application-x-executable");
}

//! isValid() (~:303) over the resolved members' null-ness, as a value struct.
struct ResolvedIcon { bool hasIcon{false}; bool hasSvg{false}; bool hasImage{false}; };
inline bool isValid(const ResolvedIcon &r) { return r.hasIcon || r.hasSvg || r.hasImage; }

}
}
