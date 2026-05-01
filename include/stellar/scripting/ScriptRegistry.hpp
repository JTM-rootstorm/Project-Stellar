#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace stellar::scripting {

/** @brief In-memory registry of script sources keyed by stable script id. */
class ScriptRegistry {
public:
    /** @brief Add or replace a script source by id. */
    void set_script(std::string script_id, std::string source);

    /** @brief Find script source text by id, or return nullptr when absent. */
    [[nodiscard]] const std::string* find_script(std::string_view script_id) const noexcept;

private:
    std::unordered_map<std::string, std::string> scripts_;
};

} // namespace stellar::scripting
