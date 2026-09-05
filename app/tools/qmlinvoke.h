/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef QMLINVOKE_H
#define QMLINVOKE_H

// Qt
#include <QMetaMethod>
#include <QMetaObject>
#include <QObject>

#include <utility>

namespace Latte {

//! Invokes @p signature on @p target when its metaobject declares it, and answers whether
//! the call actually happened. QML declares untyped function arguments as QVariant, so the
//! signatures read like "updateBadge(QVariant,QVariant)".
//!
//! Deliberately silent on a miss: the callers walk every child item of every applet looking
//! for the one that answers, so most items legitimately have no such method and a warning
//! here would fire once per item per keystroke. A caller that wants a diagnostic logs it
//! itself when this returns false.
//!
//! @p args are forwarded straight into the invocation and never stored, because Q_ARG keeps
//! only a pointer to a temporary that dies at the end of the caller's full-expression.
template <typename... Args>
bool invokeIfPresent(QObject *target, const char *signature, Args &&...args)
{
    if (!target) {
        return false;
    }

    const QMetaObject *metaObject = target->metaObject();

    if (!metaObject) {
        return false;
    }

    const int methodIndex = metaObject->indexOfMethod(signature);

    if (methodIndex == -1) {
        return false;
    }

    return metaObject->method(methodIndex).invoke(target, std::forward<Args>(args)...);
}

}

#endif
