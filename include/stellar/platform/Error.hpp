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

    /** @brief Suggested process exit code for command-line callers. */
    int exit_code = 1;

    /** @brief Construct an error from an owned diagnostic string. */
    explicit Error(std::string msg, int code = 1)
        : message(std::move(msg)), exit_code(code) {}

    /** @brief Construct an error from a string literal or null-terminated diagnostic. */
    explicit Error(const char* msg, int code = 1) : message(msg), exit_code(code) {}
};

} // namespace stellar::platform
