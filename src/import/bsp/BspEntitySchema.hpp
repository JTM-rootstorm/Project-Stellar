#pragma once

#include "BspBinary.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace stellar::import::bsp::detail {

struct CanonicalEntityName {
  std::string value;
  std::string_view key;
};

extern const std::array<std::string_view, 4> kEntityNameKeys;

[[nodiscard]] const std::string *value_for(const Entity &entity,
                                           std::string_view key) noexcept;
[[nodiscard]] const std::string *value_for_alias(const Entity &entity,
                                                 std::string_view key,
                                                 std::string_view alias) noexcept;
[[nodiscard]] std::string string_or(const Entity &entity, std::string_view key,
                                    std::string fallback = {});
[[nodiscard]] std::string string_or_alias(const Entity &entity,
                                          std::string_view key,
                                          std::string_view alias,
                                          std::string fallback = {});
[[nodiscard]] std::optional<CanonicalEntityName>
canonical_entity_name_for(const Entity &entity);
[[nodiscard]] std::string canonical_entity_name_or(const Entity &entity,
                                                   std::string fallback);
[[nodiscard]] std::optional<std::array<float, 3>> parse_vec3(const std::string *text);
[[nodiscard]] std::optional<std::array<float, 2>> parse_vec2(const std::string *text);
[[nodiscard]] std::optional<bool> parse_bool_like(const std::string *text);
[[nodiscard]] std::string lower_ascii(std::string_view text);
[[nodiscard]] std::optional<float> parse_float_value(const std::string *text);

} // namespace stellar::import::bsp::detail
