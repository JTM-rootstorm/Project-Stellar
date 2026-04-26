#pragma once

#include <string>
#include <utility>

namespace stellar::platform {

/**
 * @brief Platform-agnostic error type for fallible operations.
 */
struct Error {
    std::string message;

    explicit Error(std::string msg) : message(std::move(msg)) {}
    explicit Error(const char* msg) : message(msg) {}
};

} // namespace stellar::platform
