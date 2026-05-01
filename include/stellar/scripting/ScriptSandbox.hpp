#pragma once

struct lua_State;

namespace stellar::scripting {

/** @brief Install the restricted standard libraries and engine API into a Lua state. */
void install_restricted_sandbox(lua_State* state);

} // namespace stellar::scripting
