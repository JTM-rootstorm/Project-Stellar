#include "stellar/scripting/ScriptSandbox.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace stellar::scripting {

void install_restricted_sandbox(lua_State* state) {
    if (state == nullptr) {
        return;
    }

    luaL_requiref(state, "_G", luaopen_base, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(state, 1);

    lua_pushnil(state);
    lua_setglobal(state, "io");
    lua_pushnil(state);
    lua_setglobal(state, "os");
    lua_pushnil(state);
    lua_setglobal(state, "package");
    lua_pushnil(state);
    lua_setglobal(state, "debug");
    lua_pushnil(state);
    lua_setglobal(state, "dofile");
    lua_pushnil(state);
    lua_setglobal(state, "loadfile");
    lua_pushnil(state);
    lua_setglobal(state, "require");
    lua_pushnil(state);
    lua_setglobal(state, "load");
}

} // namespace stellar::scripting
