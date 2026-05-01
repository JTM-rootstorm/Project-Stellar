#include "stellar/scripting/ScriptRegistry.hpp"

#include <utility>

namespace stellar::scripting {

void ScriptRegistry::set_script(std::string script_id, std::string source) {
    scripts_[std::move(script_id)] = std::move(source);
}

const std::string* ScriptRegistry::find_script(std::string_view script_id) const noexcept {
    const auto found = scripts_.find(std::string(script_id));
    if (found == scripts_.end()) {
        return nullptr;
    }
    return &found->second;
}

} // namespace stellar::scripting
