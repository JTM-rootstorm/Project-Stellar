#pragma once

#include <string>
#include <utility>

namespace stellar::platform {

/**
 * @brief Platform-agnostic error type for fallible operations.
 */
struct Error {
    /** @brief Human-readable platform diagnostic message. */
    std::string message;

    /** @brief Construct an error from an owned diagnostic string. */
    explicit Error(std::string msg) : message(std::move(msg)) {}

    /** @brief Construct an error from a string literal or null-terminated diagnostic. */
    explicit Error(const char* msg) : message(msg) {}
};

} // namespace stellar::platform
