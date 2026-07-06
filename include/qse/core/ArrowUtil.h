#pragma once

#include <arrow/status.h>
#include <stdexcept>

namespace qse {

/**
 * @brief Throws if an Arrow operation failed. Use to consume the
 * [[nodiscard]] Status from builder Append/Finish calls, where a failure
 * (allocation, type mismatch) must not be silently ignored.
 */
inline void throw_if_not_ok(const arrow::Status& status) {
    if (!status.ok()) {
        throw std::runtime_error(status.ToString());
    }
}

} // namespace qse
