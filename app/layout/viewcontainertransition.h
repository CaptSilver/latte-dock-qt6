/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWCONTAINERTRANSITION_H
#define VIEWCONTAINERTRANSITION_H

// Qt
#include <QHash>

namespace Latte {
namespace Layout {

//! GenericLayout parks each view in one of two maps: active (m_latteViews) and
//! waiting (m_waitingLatteViews). These pure helpers move a view between the maps
//! when its containment is destroyed or toggles its destroyed flag, so the
//! container-transition bookkeeping is unit-testable without a live GenericLayout.
namespace ViewContainerTransition {

//! Move the value for key from one map to the other and return it. The value is
//! inserted into `to` even when absent from `from` (take() yields a default),
//! preserving the original unconditional insert.
template <typename Key, typename Value>
Value moveBetween(QHash<Key, Value> &from, QHash<Key, Value> &to, const Key &key)
{
    Value value = from.take(key);
    to.insert(key, value);
    return value;
}

//! Take the value from whichever map holds it, preferring the first (active) map to
//! match the original take-active-then-waiting fallback. Returns a default Value if
//! neither holds the key. (Where the original tested the taken pointer for null,
//! this tests map membership; the maps never store a null view, so the two are
//! equivalent in practice.)
template <typename Key, typename Value>
Value takeFromEither(QHash<Key, Value> &first, QHash<Key, Value> &second, const Key &key)
{
    if (first.contains(key)) {
        return first.take(key);
    }

    return second.take(key);
}

}
}
}

#endif
