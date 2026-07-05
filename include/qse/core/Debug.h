#pragma once

#include <cstdlib>

namespace qse {

/**
 * @brief Opt-in debug tracing, enabled by setting QSE_DEBUG=1 in the
 * environment. Hot-path logging must be gated on this check; suppressing
 * stdout globally (the old failbit hack) breaks every binary linking qse.
 */
inline bool qse_debug_enabled() {
    static const bool enabled = std::getenv("QSE_DEBUG") != nullptr;
    return enabled;
}

} // namespace qse
