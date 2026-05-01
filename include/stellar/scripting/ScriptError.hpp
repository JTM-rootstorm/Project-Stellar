#pragma once

#include <string>

namespace stellar::scripting {

/** @brief Error produced while loading or executing a script. */
struct ScriptError {
    /** @brief Stable script identifier, path, or marker-provided script id. */
    std::string script_id;

    /** @brief Lua table/function name or engine operation that failed. */
    std::string operation;

    /** @brief Human-readable diagnostic message. */
    std::string message;
};

} // namespace stellar::scripting
