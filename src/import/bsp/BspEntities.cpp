#include "BspBinary.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace stellar::import::bsp::detail {
namespace {

class EntityParser {
public:
  EntityParser(std::string_view text, std::string_view source_uri)
      : text_(text), source_uri_(source_uri) {}

  [[nodiscard]] std::expected<std::vector<Entity>, stellar::platform::Error>
  parse() {
    std::vector<Entity> entities;
    skip_ws();
    while (!eof()) {
      if (peek() != '{') {
        return fail<std::vector<Entity>>("expected entity block opener");
      }
      advance();
      Entity entity{};
      skip_ws();
      while (!eof() && peek() != '}') {
        auto key = quoted_string();
        if (!key) {
          return std::unexpected(key.error());
        }
        skip_ws();
        auto value = quoted_string();
        if (!value) {
          return std::unexpected(value.error());
        }
        entity.pairs.push_back(
            EntityKeyValue{.key = std::move(*key), .value = std::move(*value)});
        skip_ws();
      }
      if (eof()) {
        return fail<std::vector<Entity>>("unterminated entity block");
      }
      advance();
      entities.push_back(std::move(entity));
      skip_ws();
    }
    return entities;
  }

private:
  [[nodiscard]] bool eof() const noexcept { return cursor_ >= text_.size(); }
  [[nodiscard]] char peek() const noexcept { return text_[cursor_]; }
  void advance() noexcept { ++cursor_; }
  void skip_ws() noexcept {
    while (!eof() && std::isspace(static_cast<unsigned char>(peek())) != 0) {
      advance();
    }
  }
  [[nodiscard]] std::expected<std::string, stellar::platform::Error>
  quoted_string() {
    skip_ws();
    if (eof() || peek() != '"') {
      return fail("expected quoted string");
    }
    advance();
    std::string out;
    while (!eof()) {
      const char ch = peek();
      advance();
      if (ch == '"') {
        return out;
      }
      if (ch == '\\') {
        if (eof()) {
          return fail("unterminated escape sequence");
        }
        const char escaped = peek();
        advance();
        if (escaped == '"' || escaped == '\\') {
          out.push_back(escaped);
        } else {
          out.push_back(escaped);
        }
      } else {
        out.push_back(ch);
      }
    }
    return fail("unterminated quoted string");
  }
  template <typename T = std::string>
  [[nodiscard]] std::expected<T, stellar::platform::Error>
  fail(std::string message) const {
    return std::unexpected(stellar::platform::Error(
        std::string(source_uri_) + ": BSP entities " + std::move(message) +
        " at byte " + std::to_string(cursor_)));
  }

  std::string_view text_;
  std::string_view source_uri_;
  std::size_t cursor_ = 0;
};

} // namespace

std::expected<std::vector<Entity>, stellar::platform::Error>
parse_entities(std::string_view text, std::string_view source_uri) {
  EntityParser parser{text, source_uri};
  return parser.parse();
}

} // namespace stellar::import::bsp::detail
