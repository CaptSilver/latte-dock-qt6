/*
    SPDX-FileCopyrightText: 2020 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "environment.h"

// Qt
#include <QDebug>
#include <QProcess>

// Plasma
#include <Plasma/plasma_version.h>

#define LONGDURATION 240
#define SHORTDURATION 40

namespace Latte{

const int Environment::SeparatorLength;

Environment::Environment(QObject *parent)
    : QObject(parent)
{
}

int Environment::separatorLength() const
{
    return SeparatorLength;
}

uint Environment::shortDuration() const
{
    return SHORTDURATION;
}

uint Environment::longDuration() const
{
    return LONGDURATION;
}

uint Environment::frameworksVersion() const
{
    return PLASMA_VERSION;
}

uint Environment::plasmaDesktopVersion()
{
    if (m_plasmaDesktopVersion == -1) {
        m_plasmaDesktopVersion = identifyPlasmaDesktopVersion();
    }

    return m_plasmaDesktopVersion;
}

uint Environment::makeVersion(uint major, uint minor, uint release) const
{
    return (((major) << 16) | ((minor) << 8) | (release));
}

uint Environment::identifyPlasmaDesktopVersion()
{
    //! Identify Plasma Desktop version
    QStringList plasmaDesktopVersionParts = QStringLiteral(PLASMA_WORKSPACE_VERSION).split(QLatin1Char('.'));

    if (plasmaDesktopVersionParts.count() == 3) {
        qDebug() << " /////////////////////////";
        uint maj = plasmaDesktopVersionParts[0].toUInt();
        uint min = plasmaDesktopVersionParts[1].toUInt();
        uint rel = plasmaDesktopVersionParts[2].toUInt();

        if (maj > 0) {
            uint desktopVersion = makeVersion(maj, min, rel);

            QString message(QStringLiteral("Plasma Desktop version:  ") + QString::number(maj) + QStringLiteral(".")
                    + QString::number(min) + QStringLiteral(".") + QString::number(rel)
                    + QStringLiteral(" (") + QString::number(desktopVersion) + QStringLiteral(")"));
            qDebug() << message;
            qDebug() << " /////////////////////////";

            return desktopVersion;
        }

        qDebug() << " /////////////////////////";
    }

    return 0;
}

}
