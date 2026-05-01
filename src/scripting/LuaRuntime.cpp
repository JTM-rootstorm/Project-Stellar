#include "stellar/scripting/LuaRuntime.hpp"

#include "stellar/scripting/ScriptSandbox.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace stellar::scripting {
namespace {

constexpr char kRuntimeRegistryKey = 's';
constexpr const char* kInstructionBudgetMessage = "Lua instruction budget exceeded";

std::string lua_error_message(lua_State* state, int index) {
    const char* message = lua_tostring(state, index);
    if (message == nullptr) {
        return "unknown Lua error";
    }
    return message;
}

void push_field_value(lua_State* state, const ScriptField& field) {
    switch (field.type) {
    case ScriptValueType::kNil:
        lua_pushnil(state);
        break;
    case ScriptValueType::kBoolean:
        lua_pushboolean(state, field.bool_value ? 1 : 0);
        break;
    case ScriptValueType::kNumber:
        lua_pushnumber(state, field.number_value);
        break;
    case ScriptValueType::kString:
        lua_pushlstring(state, field.string_value.data(), field.string_value.size());
        break;
    }
}

void push_context(lua_State* state, const ScriptCallContext& context) {
    lua_newtable(state);

    lua_pushinteger(state, static_cast<lua_Integer>(context.tick));
    lua_setfield(state, -2, "tick");

    lua_pushlstring(state, context.source_name.data(), context.source_name.size());
    lua_setfield(state, -2, "source_name");

    for (const ScriptField& field : context.fields) {
        push_field_value(state, field);
        lua_setfield(state, -2, field.key.c_str());
    }
}

} // namespace

struct LuaRuntime::Impl {
    lua_State* state = nullptr;
    LuaRuntimeConfig config{};
    int remaining_budget = 0;
    std::vector<ScriptOutputEvent> output_events;

    explicit Impl(LuaRuntimeConfig runtime_config) : state(luaL_newstate()), config(runtime_config) {}

    ~Impl() noexcept {
        if (state != nullptr) {
            lua_close(state);
            state = nullptr;
        }
    }
};

namespace {

LuaRuntime::Impl* runtime_from_state(lua_State* state) {
    lua_pushlightuserdata(state, const_cast<char*>(&kRuntimeRegistryKey));
    lua_gettable(state, LUA_REGISTRYINDEX);
    auto* runtime = static_cast<LuaRuntime::Impl*>(lua_touserdata(state, -1));
    lua_pop(state, 1);
    return runtime;
}

void instruction_budget_hook(lua_State* state, lua_Debug*) {
    LuaRuntime::Impl* runtime = runtime_from_state(state);
    if (runtime == nullptr) {
        luaL_error(state, "%s", kInstructionBudgetMessage);
        return;
    }
    --runtime->remaining_budget;
    if (runtime->remaining_budget < 0) {
        luaL_error(state, "%s", kInstructionBudgetMessage);
    }
}

void begin_budget(LuaRuntime::Impl& runtime) {
    runtime.remaining_budget = std::max(0, runtime.config.instruction_budget);
    lua_sethook(runtime.state, instruction_budget_hook, LUA_MASKCOUNT, 1);
}

void end_budget(LuaRuntime::Impl& runtime) {
    lua_sethook(runtime.state, nullptr, 0, 0);
    runtime.remaining_budget = 0;
}

std::expected<void, ScriptError> protected_call(LuaRuntime::Impl& runtime,
                                                std::string_view script_id,
                                                std::string_view operation,
                                                int argument_count) {
    begin_budget(runtime);
    const int result = lua_pcall(runtime.state, argument_count, 0, 0);
    end_budget(runtime);
    if (result != LUA_OK) {
        ScriptError error{std::string(script_id), std::string(operation),
                          lua_error_message(runtime.state, -1)};
        lua_pop(runtime.state, 1);
        lua_settop(runtime.state, 0);
        return std::unexpected(std::move(error));
    }
    return {};
}

std::expected<ScriptField, std::string> read_field(lua_State* state, std::string key, int value_index) {
    ScriptField field{};
    field.key = std::move(key);
    const int type = lua_type(state, value_index);
    switch (type) {
    case LUA_TNIL:
        field.type = ScriptValueType::kNil;
        break;
    case LUA_TBOOLEAN:
        field.type = ScriptValueType::kBoolean;
        field.bool_value = lua_toboolean(state, value_index) != 0;
        break;
    case LUA_TNUMBER:
        field.type = ScriptValueType::kNumber;
        field.number_value = lua_tonumber(state, value_index);
        break;
    case LUA_TSTRING: {
        field.type = ScriptValueType::kString;
        std::size_t length = 0;
        const char* value = lua_tolstring(state, value_index, &length);
        if (value != nullptr) {
            field.string_value.assign(value, length);
        }
        break;
    }
    default:
        return std::unexpected("stellar.emit_event only accepts nil, boolean, number, or string fields");
    }
    return field;
}

int emit_event(lua_State* state) {
    LuaRuntime::Impl* runtime = runtime_from_state(state);
    if (runtime == nullptr) {
        return luaL_error(state, "stellar runtime is not installed");
    }

    std::size_t name_length = 0;
    const char* name = luaL_checklstring(state, 1, &name_length);

    ScriptOutputEvent event{};
    event.name.assign(name, name_length);

    if (!lua_isnoneornil(state, 2)) {
        luaL_checktype(state, 2, LUA_TTABLE);
        lua_pushnil(state);
        while (lua_next(state, 2) != 0) {
            if (lua_type(state, -2) != LUA_TSTRING) {
                return luaL_error(state, "stellar.emit_event field keys must be strings");
            }
            std::size_t key_length = 0;
            const char* key = lua_tolstring(state, -2, &key_length);
            auto field = read_field(state, std::string(key, key_length), -1);
            if (!field) {
                return luaL_error(state, "%s", field.error().c_str());
            }
            event.fields.push_back(std::move(*field));
            lua_pop(state, 1);
        }
    }

    std::sort(event.fields.begin(), event.fields.end(), [](const ScriptField& lhs,
                                                            const ScriptField& rhs) {
        return lhs.key < rhs.key;
    });
    runtime->output_events.push_back(std::move(event));
    return 0;
}

void install_engine_api(lua_State* state) {
    lua_newtable(state);
    lua_pushcfunction(state, emit_event);
    lua_setfield(state, -2, "emit_event");
    lua_setglobal(state, "stellar");
}

} // namespace

LuaRuntime::LuaRuntime(LuaRuntimeConfig config) : impl_(new Impl(config)) {
    if (impl_->state == nullptr) {
        return;
    }

    lua_pushlightuserdata(impl_->state, const_cast<char*>(&kRuntimeRegistryKey));
    lua_pushlightuserdata(impl_->state, impl_);
    lua_settable(impl_->state, LUA_REGISTRYINDEX);

    install_restricted_sandbox(impl_->state);
    install_engine_api(impl_->state);
}

LuaRuntime::~LuaRuntime() noexcept {
    delete impl_;
    impl_ = nullptr;
}

LuaRuntime::LuaRuntime(LuaRuntime&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

LuaRuntime& LuaRuntime::operator=(LuaRuntime&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

std::expected<void, ScriptError> LuaRuntime::load_script(std::string_view script_id,
                                                         std::string_view source) {
    if (impl_ == nullptr || impl_->state == nullptr) {
        return std::unexpected(ScriptError{std::string(script_id), "load_script",
                                           "Lua runtime is not initialized"});
    }
    if (!source.empty() && static_cast<unsigned char>(source.front()) == 0x1bU) {
        return std::unexpected(ScriptError{std::string(script_id), "load_script",
                                           "Lua bytecode loading is disabled"});
    }

    const int load_result = luaL_loadbuffer(impl_->state, source.data(), source.size(),
                                            std::string(script_id).c_str());
    if (load_result != LUA_OK) {
        ScriptError error{std::string(script_id), "load_script", lua_error_message(impl_->state, -1)};
        lua_pop(impl_->state, 1);
        lua_settop(impl_->state, 0);
        return std::unexpected(std::move(error));
    }

    return protected_call(*impl_, script_id, "load_script", 0);
}

std::expected<void, ScriptError> LuaRuntime::call_table_function(
    std::string_view script_id,
    std::string_view table_name,
    std::string_view function_name,
    const ScriptCallContext& context) noexcept {
    if (impl_ == nullptr || impl_->state == nullptr) {
        return std::unexpected(ScriptError{std::string(script_id), std::string(function_name),
                                           "Lua runtime is not initialized"});
    }

    lua_getglobal(impl_->state, std::string(table_name).c_str());
    if (!lua_istable(impl_->state, -1)) {
        lua_pop(impl_->state, 1);
        return {};
    }

    lua_getfield(impl_->state, -1, std::string(function_name).c_str());
    if (!lua_isfunction(impl_->state, -1)) {
        lua_pop(impl_->state, 2);
        return {};
    }

    lua_remove(impl_->state, -2);
    push_context(impl_->state, context);
    return protected_call(*impl_, script_id, function_name, 1);
}

std::vector<ScriptOutputEvent> LuaRuntime::drain_output_events() {
    if (impl_ == nullptr) {
        return {};
    }
    std::vector<ScriptOutputEvent> events;
    events.swap(impl_->output_events);
    return events;
}

bool LuaRuntime::has_table(std::string_view table_name) const noexcept {
    if (impl_ == nullptr || impl_->state == nullptr) {
        return false;
    }

    lua_getglobal(impl_->state, std::string(table_name).c_str());
    const bool is_table = lua_istable(impl_->state, -1);
    lua_pop(impl_->state, 1);
    return is_table;
}

} // namespace stellar::scripting
