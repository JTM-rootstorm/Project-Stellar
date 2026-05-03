#pragma once

#include <string>

namespace stellar::platform {

/**
 * @brief Return compact, non-secret display environment diagnostics for startup failures.
 */
[[nodiscard]] std::string display_environment_diagnostics();

/**
 * @brief Append display diagnostics and a launch hint to a platform startup error message.
 */
[[nodiscard]] std::string append_display_environment_diagnostics(std::string message);

} // namespace stellar::platform
