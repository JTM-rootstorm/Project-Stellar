#include "Wad3Reader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <span>
#include <sstream>
#include <string>

namespace stellar::import::bsp::detail {
namespace {

constexpr std::uint8_t kMiptexType = 0x43;
constexpr std::uint32_t kPaletteSize = 256;

[[nodiscard]] std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset])) |
         (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 1])) << 8U) |
         (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 2])) << 16U) |
         (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 3])) << 24U);
}

[[nodiscard]] std::uint16_t read_u16(std::span<const std::byte> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(std::to_integer<unsigned char>(bytes[offset])) |
         static_cast<std::uint16_t>(
             static_cast<std::uint16_t>(std::to_integer<unsigned char>(bytes[offset + 1])) << 8U);
}

[[nodiscard]] std::string fixed_name(std::span<const std::byte> bytes, std::size_t offset) {
  std::string name;
  for (std::size_t i = 0; i < 16 && offset + i < bytes.size(); ++i) {
    const char ch = static_cast<char>(std::to_integer<unsigned char>(bytes[offset + i]));
    if (ch == '\0') {
      break;
    }
    name.push_back(ch);
  }
  return name;
}

[[nodiscard]] std::string lower_key(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char ch : text) {
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return out;
}

[[nodiscard]] std::vector<std::byte> read_file(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {};
  }
  std::vector<std::byte> bytes;
  char ch = 0;
  while (file.get(ch)) {
    bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
  }
  return bytes;
}

[[nodiscard]] bool has_parent_escape(const std::filesystem::path &path) {
  for (const auto &part : path) {
    if (part == "..") {
      return true;
    }
  }
  return false;
}

void append_env_roots(std::vector<std::filesystem::path> &roots, const char *value) {
  if (value == nullptr || *value == '\0') {
    return;
  }
  std::stringstream stream(value);
  std::string item;
  while (std::getline(stream, item, ':')) {
    if (!item.empty()) {
      roots.emplace_back(item);
    }
  }
}

[[nodiscard]] std::filesystem::path repo_dev_wad_root() {
  std::filesystem::path path = std::filesystem::current_path();
  for (int i = 0; i < 8; ++i) {
    const auto candidate = path / "tools" / "trenchbroom" / "Stellar" / "materials";
    if (std::filesystem::exists(candidate / "stellar_dev.wad")) {
      return candidate;
    }
    if (!path.has_parent_path() || path == path.parent_path()) {
      break;
    }
    path = path.parent_path();
  }
  return std::filesystem::current_path() / "tools" / "trenchbroom" / "Stellar" / "materials";
}

[[nodiscard]] std::optional<std::filesystem::path>
safe_join_existing(const std::filesystem::path &root, const std::filesystem::path &wad_path,
                   std::vector<std::string> &diagnostics) {
  if (wad_path.empty() || has_parent_escape(wad_path)) {
    diagnostics.push_back("rejected unsafe WAD path with parent escape: " + wad_path.string());
    return std::nullopt;
  }
  if (wad_path.is_absolute()) {
    diagnostics.push_back("rejected absolute WAD path without explicit allow: " + wad_path.string());
    return std::nullopt;
  }
  const auto candidate = (root / wad_path).lexically_normal();
  diagnostics.push_back("tried WAD path: " + candidate.string());
  if (std::filesystem::is_regular_file(candidate)) {
    return candidate;
  }
  return std::nullopt;
}

void parse_wad_file(const std::filesystem::path &path, WadResolveResult &result) {
  const std::vector<std::byte> owned = read_file(path);
  std::span<const std::byte> bytes{owned.data(), owned.size()};
  if (bytes.size() < 12) {
    result.diagnostics.push_back("WAD file is too small: " + path.string());
    return;
  }
  const std::string magic = fixed_name(bytes, 0).substr(0, 4);
  if (magic != "WAD3") {
    result.diagnostics.push_back("WAD file does not have WAD3 magic: " + path.string());
    return;
  }
  const std::uint32_t count = read_u32(bytes, 4);
  const std::uint32_t directory_offset = read_u32(bytes, 8);
  if (directory_offset > bytes.size() || static_cast<std::uint64_t>(directory_offset) +
                                        static_cast<std::uint64_t>(count) * 32ULL > bytes.size()) {
    result.diagnostics.push_back("WAD directory is outside file bounds: " + path.string());
    return;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::size_t entry = directory_offset + static_cast<std::size_t>(i) * 32U;
    const std::uint32_t filepos = read_u32(bytes, entry);
    const std::uint32_t disksize = read_u32(bytes, entry + 4);
    const std::uint8_t type = std::to_integer<std::uint8_t>(bytes[entry + 12]);
    const std::uint8_t compression = std::to_integer<std::uint8_t>(bytes[entry + 13]);
    const std::string name = fixed_name(bytes, entry + 16);
    if (type != kMiptexType || compression != 0 || name.empty()) {
      continue;
    }
    if (static_cast<std::uint64_t>(filepos) + disksize > bytes.size() || disksize < 40U) {
      result.diagnostics.push_back("WAD lump has invalid bounds: " + path.string() + "#" + name);
      continue;
    }
    const std::uint32_t width = read_u32(bytes, filepos + 16);
    const std::uint32_t height = read_u32(bytes, filepos + 20);
    const std::uint32_t pixels_offset = read_u32(bytes, filepos + 24);
    if (width == 0 || height == 0 || width > 4096 || height > 4096) {
      result.diagnostics.push_back("WAD texture has invalid dimensions: " + path.string() + "#" + name);
      continue;
    }
    const std::uint64_t pixel_count = static_cast<std::uint64_t>(width) * height;
    if (pixels_offset < 40U || static_cast<std::uint64_t>(pixels_offset) + pixel_count > disksize) {
      result.diagnostics.push_back("WAD texture has invalid mip offsets: " + path.string() + "#" + name);
      continue;
    }
    const std::size_t palette_count_offset =
        static_cast<std::size_t>(filepos + disksize - (kPaletteSize * 3U + 2U));
    if (read_u16(bytes, palette_count_offset) != kPaletteSize) {
      result.diagnostics.push_back("WAD texture has unsupported palette size: " + path.string() + "#" + name);
      continue;
    }
    const std::size_t palette_offset = palette_count_offset + 2U;
    stellar::assets::ImageAsset image{};
    image.name = name;
    image.width = width;
    image.height = height;
    image.format = stellar::assets::ImageFormat::kR8G8B8A8;
    image.source_uri = path.string() + "#wad3/" + name;
    image.pixels.reserve(static_cast<std::size_t>(pixel_count) * 4U);
    const std::size_t pixel_offset = static_cast<std::size_t>(filepos + pixels_offset);
    for (std::size_t p = 0; p < static_cast<std::size_t>(pixel_count); ++p) {
      const auto index = static_cast<std::size_t>(std::to_integer<std::uint8_t>(bytes[pixel_offset + p]));
      image.pixels.push_back(std::to_integer<std::uint8_t>(bytes[palette_offset + index * 3U]));
      image.pixels.push_back(std::to_integer<std::uint8_t>(bytes[palette_offset + index * 3U + 1U]));
      image.pixels.push_back(std::to_integer<std::uint8_t>(bytes[palette_offset + index * 3U + 2U]));
      image.pixels.push_back(255U);
    }
    result.textures.emplace(lower_key(name), WadTextureAsset{.image = std::move(image)});
  }
}

} // namespace

WadResolveContext make_wad_resolve_context(std::string_view source_uri) {
  WadResolveContext context{};
  std::filesystem::path source_path{std::string(source_uri)};
  context.map_directory =
      source_path.has_parent_path() ? source_path.parent_path() : std::filesystem::current_path();
  append_env_roots(context.search_roots, std::getenv("STELLAR_WAD_PATH"));
  append_env_roots(context.search_roots, std::getenv("STELLAR_TEXTURE_PATH"));
  context.search_roots.push_back(repo_dev_wad_root());
  context.allow_absolute_wad_paths = std::getenv("STELLAR_ALLOW_ABSOLUTE_WAD_PATHS") != nullptr;
  return context;
}

WadResolveResult resolve_wad_textures(std::string_view wad_key, const WadResolveContext &context) {
  WadResolveResult result{};
  std::stringstream stream{std::string(wad_key)};
  std::string item;
  while (std::getline(stream, item, ';')) {
    if (item.empty()) {
      continue;
    }
    std::filesystem::path wad_path{item};
    std::optional<std::filesystem::path> resolved;
    if (wad_path.is_absolute()) {
      if (context.allow_absolute_wad_paths && !has_parent_escape(wad_path)) {
        result.diagnostics.push_back("tried absolute WAD path: " + wad_path.string());
        if (std::filesystem::is_regular_file(wad_path)) {
          resolved = wad_path;
        }
      } else {
        result.diagnostics.push_back("rejected absolute WAD path without explicit allow: " + wad_path.string());
      }
    } else {
      resolved = safe_join_existing(context.map_directory, wad_path, result.diagnostics);
      for (const auto &root : context.search_roots) {
        if (resolved.has_value()) {
          break;
        }
        resolved = safe_join_existing(root, wad_path.filename(), result.diagnostics);
      }
    }
    if (resolved.has_value()) {
      parse_wad_file(*resolved, result);
    } else {
      result.diagnostics.push_back("missing external WAD: " + item);
    }
  }
  return result;
}

} // namespace stellar::import::bsp::detail
