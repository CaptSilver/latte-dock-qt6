/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef WINDOWINDEX_H
#define WINDOWINDEX_H

// local
#include "windowinfowrap.h"

// Qt
#include <QHash>

namespace Latte {
namespace WindowSystem {

//! id -> window* index so windowFor() resolves in O(1) instead of scanning the
//! by-value windows() list on every request path. Generic over the window pointer
//! so the container is unit-testable without a live KWayland PlasmaWindow. The
//! management's windows() list stays authoritative -- windowFor() falls back to a
//! scan on a miss -- so the index is a fast path, never the source of truth.
template <typename Window>
class WindowIndex
{
public:
    void insert(const WindowId &id, Window *window)
    {
        if (id.isValid() && window) {
            m_index.insert(id, window);
        }
    }

    void remove(const WindowId &id)
    {
        m_index.remove(id);
    }

    Window *lookup(const WindowId &id) const
    {
        return m_index.value(id, nullptr);
    }

    void clear()
    {
        m_index.clear();
    }

private:
    QHash<WindowId, Window *> m_index;
};

}
}

#endif
