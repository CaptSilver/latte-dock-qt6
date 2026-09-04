/*
    SPDX-FileCopyrightText: 2021 Michail Vourlakos <mvourlakos@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ERRORINFORMATIONDATA_H
#define ERRORINFORMATIONDATA_H

//! local
#include "genericdata.h"
#include "appletdata.h"

//! Qt
#include <QMetaType>
#include <QString>

namespace Latte {
namespace Data {

class ErrorInformation : public Generic
{
public:
    ErrorInformation();
    ErrorInformation(ErrorInformation &&o) = default;
    ErrorInformation(const ErrorInformation &o) = default;

    //! error data
    Data::Applet containment;
    Data::Applet applet;

    bool isValid() const;

    //! Operators
    ErrorInformation &operator=(const ErrorInformation &rhs) = default;
    ErrorInformation &operator=(ErrorInformation &&rhs) = default;
    bool operator==(const ErrorInformation &rhs) const;
    bool operator!=(const ErrorInformation &rhs) const;
};

typedef ErrorInformation WarningInformation;

}
}

Q_DECLARE_METATYPE(Latte::Data::ErrorInformation)

#endif
