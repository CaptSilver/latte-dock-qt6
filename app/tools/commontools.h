/*
    SPDX-FileCopyrightText: 2018 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COMMONTOOLS_H
#define COMMONTOOLS_H

// Qt
#include <QColor>
#include <QRect>
#include <QString>

namespace Latte {

float colorBrightness(QColor color);
float colorBrightness(QRgb rgb);
float colorBrightness(float r, float g, float b);

float colorLumina(QColor color);
float colorLumina(QRgb rgb);
float colorLumina(float r, float g, float b);

//! Fixed-width numeric key: callers concatenate variable-width text after it, so a
//! lexicographic compare only matches numeric order while the width stays constant.
QString sortKeyPrefix(int priority);

QString rectToString(const QRect &rect);
QRect stringToRect(const QString &str);

//! returns the standard path found that contains the subPath,
//! searching the user's own data dirs ahead of the system ones
QString standardPath(QString subPath);

QString configPath();
}

#endif
