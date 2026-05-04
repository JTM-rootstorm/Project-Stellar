#pragma once

#include <string>
#include <vector>

namespace stellar::scripting {

/** @brief Primitive script value type accepted by initial engine event output. */
enum class ScriptValueType {
    /** @brief No value is present for this script field. */
    kNil,
    /** @brief Field stores a boolean value. */
    kBoolean,
    /** @brief Field stores a numeric value. */
    kNumber,
    /** @brief Field stores a string value. */
    kString,
};

/** @brief Primitive key/value emitted by script code through safe engine APIs. */
struct ScriptField {
    /** @brief Stable field key. */
    std::string key;

    /** @brief Type of the stored script value. */
    ScriptValueType type = ScriptValueType::kNil;

    /** @brief Boolean value when type is kBoolean. */
    bool bool_value = false;

    /** @brief Numeric value when type is kNumber. */
    double number_value = 0.0;

    /** @brief String value when type is kString. */
    std::string string_value;
};

/** @brief Event emitted by a script and later validated/applied by native server code. */
struct ScriptOutputEvent {
    /** @brief Stable event name. */
    std::string name;

    /** @brief Deterministically ordered primitive event fields. */
    std::vector<ScriptField> fields;
};

} // namespace stellar::scripting
