#include "BspEntitySchema.hpp"

#include <cctype>
#include <sstream>

namespace stellar::import::bsp::detail {

const std::array<std::string_view, 4> kEntityNameKeys{
    "targetname", "_stellar_name", "stellar.name", "name"};

const std::string *value_for(const Entity &entity, std::string_view key) noexcept {
  for (const auto &pair : entity.pairs) {
    if (pair.key == key) {
      return &pair.value;
    }
  }
  return nullptr;
}

const std::string *value_for_alias(const Entity &entity, std::string_view key,
                                   std::string_view alias) noexcept {
  if (const std::string *value = value_for(entity, key)) {
    return value;
  }
  return value_for(entity, alias);
}

std::string string_or(const Entity &entity, std::string_view key,
                      std::string fallback) {
  if (const std::string *value = value_for(entity, key)) {
    return *value;
  }
  return fallback;
}

std::string string_or_alias(const Entity &entity, std::string_view key,
                            std::string_view alias, std::string fallback) {
  if (const std::string *value = value_for_alias(entity, key, alias)) {
    return *value;
  }
  return fallback;
}

std::optional<CanonicalEntityName> canonical_entity_name_for(const Entity &entity) {
  for (std::string_view key : kEntityNameKeys) {
    if (const std::string *value = value_for(entity, key);
        value != nullptr && !value->empty()) {
      return CanonicalEntityName{.value = *value, .key = key};
    }
  }
  return std::nullopt;
}

std::string canonical_entity_name_or(const Entity &entity, std::string fallback) {
  if (auto name = canonical_entity_name_for(entity)) {
    return name->value;
  }
  return fallback;
}

std::optional<std::array<float, 3>> parse_vec3(const std::string *text) {
  if (text == nullptr) {
    return std::nullopt;
  }
  std::istringstream stream(*text);
  std::array<float, 3> value{};
  if (stream >> value[0] >> value[1] >> value[2]) {
    std::string trailing;
    if (!(stream >> trailing)) {
      return value;
    }
  }
  return std::nullopt;
}

std::optional<std::array<float, 2>> parse_vec2(const std::string *text) {
  if (text == nullptr) {
    return std::nullopt;
  }
  std::istringstream stream(*text);
  std::array<float, 2> value{};
  if (stream >> value[0] >> value[1]) {
    std::string trailing;
    if (!(stream >> trailing)) {
      return value;
    }
  }
  return std::nullopt;
}

std::optional<bool> parse_bool_like(const std::string *text) {
  if (text == nullptr) {
    return std::nullopt;
  }
  std::string value;
  value.reserve(text->size());
  for (char character : *text) {
    value.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(character))));
  }
  if (value == "1" || value == "true" || value == "yes") {
    return true;
  }
  if (value == "0" || value == "false" || value == "no") {
    return false;
  }
  return std::nullopt;
}

std::string lower_ascii(std::string_view text) {
  std::string value;
  value.reserve(text.size());
  for (const char character : text) {
    value.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(character))));
  }
  return value;
}

std::optional<float> parse_float_value(const std::string *text) {
  if (text == nullptr) {
    return std::nullopt;
  }
  std::istringstream stream(*text);
  float value = 0.0F;
  if (stream >> value) {
    std::string trailing;
    if (!(stream >> trailing)) {
      return value;
    }
  }
  return std::nullopt;
}

} // namespace stellar::import::bsp::detail
