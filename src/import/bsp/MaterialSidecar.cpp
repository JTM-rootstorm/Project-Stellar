#include "MaterialSidecar.hpp"

#include "BspBinary.hpp"
#include "BspImportDiagnostics.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>

namespace stellar::import::bsp::detail {
namespace {

[[nodiscard]] std::string trim(std::string_view value) {
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.remove_prefix(1);
  }
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.remove_suffix(1);
  }
  return std::string(value);
}

[[nodiscard]] std::string strip_comments(std::string_view line) {
  bool quoted = false;
  for (std::size_t index = 0; index < line.size(); ++index) {
    const char ch = line[index];
    if (ch == '"') {
      quoted = !quoted;
      continue;
    }
    if (!quoted && ch == '#') {
      return std::string(line.substr(0, index));
    }
    if (!quoted && ch == '/' && index + 1 < line.size() && line[index + 1] == '/') {
      return std::string(line.substr(0, index));
    }
  }
  return std::string(line);
}

[[nodiscard]] std::string unquote(std::string value) {
  value = trim(value);
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

[[nodiscard]] bool is_drive_prefixed(std::string_view value) noexcept {
  return value.size() >= 2 && std::isalpha(static_cast<unsigned char>(value[0])) != 0 &&
         value[1] == ':';
}

[[nodiscard]] bool is_safe_relative_path(std::string_view raw) {
  if (raw.empty() || raw.front() == '/' || raw.front() == '\\' ||
      is_drive_prefixed(raw)) {
    return false;
  }
  std::string normalized(raw);
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  std::size_t begin = 0;
  for (std::size_t index = 0; index <= normalized.size(); ++index) {
    if (index == normalized.size() || normalized[index] == '/') {
      const std::string_view part(normalized.data() + begin, index - begin);
      if (part.empty() || part == "..") {
        return false;
      }
      begin = index + 1;
    }
  }
  return true;
}

[[nodiscard]] std::optional<float> parse_float(std::string_view text) {
  const std::string value = trim(text);
  float parsed = 0.0F;
  const char *begin = value.data();
  const char *end = value.data() + value.size();
  const auto result = std::from_chars(begin, end, parsed);
  if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(parsed)) {
    return std::nullopt;
  }
  return parsed;
}

[[nodiscard]] std::optional<int> parse_int(std::string_view text) {
  const std::string value = trim(text);
  int parsed = 0;
  const char *begin = value.data();
  const char *end = value.data() + value.size();
  const auto result = std::from_chars(begin, end, parsed);
  if (result.ec != std::errc{} || result.ptr != end) {
    return std::nullopt;
  }
  return parsed;
}

[[nodiscard]] std::optional<bool> parse_bool(std::string value) {
  value = trim(value);
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (value == "true" || value == "1" || value == "yes" || value == "on") {
    return true;
  }
  if (value == "false" || value == "0" || value == "no" || value == "off") {
    return false;
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<std::array<float, 3>> parse_vec3(std::string value) {
  std::replace(value.begin(), value.end(), ',', ' ');
  std::istringstream stream(value);
  std::array<float, 3> result{};
  for (float &component : result) {
    if (!(stream >> component) || !std::isfinite(component)) {
      return std::nullopt;
    }
  }
  std::string extra;
  if (stream >> extra) {
    return std::nullopt;
  }
  return result;
}

void add_sidecar_diagnostic(ImportReport *report, DiagnosticSeverity severity,
                            DiagnosticCode code, const std::string &source_uri,
                            std::string message) {
  if (report == nullptr) {
    return;
  }
  report->diagnostics.push_back(Diagnostic{
      .severity = severity,
      .code = code,
      .message = std::move(message),
      .source_uri = source_uri,
      .lump_index = static_cast<std::size_t>(LumpIndex::kTextures)});
  if (severity == DiagnosticSeverity::kError) {
    report->has_errors = true;
  }
}

[[nodiscard]] MaterialSidecarTextureRef texture_ref_from(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  return MaterialSidecarTextureRef{.relative_path = std::move(value)};
}

[[nodiscard]] bool set_texture_ref(std::optional<MaterialSidecarTextureRef> &slot,
                                   std::string value, ImportReport *report,
                                   const std::string &source_uri,
                                   const std::filesystem::path &path,
                                   std::size_t line_number,
                                   const std::string &key) {
  value = unquote(std::move(value));
  if (value.empty()) {
    slot = std::nullopt;
    return true;
  }
  if (!is_safe_relative_path(value)) {
    add_sidecar_diagnostic(
        report, DiagnosticSeverity::kError, DiagnosticCode::kMaterialSidecarUnsafePath,
        source_uri, source_uri + ": material sidecar '" + path.string() + "' line " +
                        std::to_string(line_number) + " key '" + key +
                        "' uses an unsafe texture path");
    return false;
  }
  slot = texture_ref_from(std::move(value));
  return true;
}

[[nodiscard]] float clamp_float(float value, float min_value, float max_value) noexcept {
  return std::max(min_value, std::min(max_value, value));
}

} // namespace

std::expected<std::string, std::string>
normalize_material_sidecar_key(std::string_view texture_name) {
  std::string key = trim(texture_name);
  std::replace(key.begin(), key.end(), '\\', '/');
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (key.empty() || key.front() == '/' || is_drive_prefixed(key)) {
    return std::unexpected("material texture key is empty or absolute");
  }
  std::size_t begin = 0;
  for (std::size_t index = 0; index <= key.size(); ++index) {
    if (index == key.size() || key[index] == '/') {
      const std::string_view part(key.data() + begin, index - begin);
      if (part.empty() || part == "..") {
        return std::unexpected("material texture key contains an unsafe path segment");
      }
      begin = index + 1;
    }
  }
  return key;
}

MaterialSidecarParseResult
parse_material_sidecar_file(const std::filesystem::path &path, bool strict,
                            const std::string &source_uri,
                            const std::string &texture_name,
                            ImportReport *report) {
  std::ifstream file(path);
  if (!file) {
    return {};
  }

  MaterialSidecar sidecar;
  bool saw_version = false;
  bool has_error = false;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(file, line)) {
    ++line_number;
    const std::string cleaned = trim(strip_comments(line));
    if (cleaned.empty()) {
      continue;
    }
    const std::size_t equals = cleaned.find('=');
    if (equals == std::string::npos) {
      add_sidecar_diagnostic(
          report, DiagnosticSeverity::kError, DiagnosticCode::kMaterialSidecarInvalid,
          source_uri, source_uri + ": material sidecar '" + path.string() + "' line " +
                          std::to_string(line_number) + " is missing '='");
      has_error = true;
      continue;
    }
    std::string key = trim(std::string_view(cleaned).substr(0, equals));
    const std::string value = trim(std::string_view(cleaned).substr(equals + 1));
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });

    const auto invalid_value = [&] {
      add_sidecar_diagnostic(
          report, DiagnosticSeverity::kError, DiagnosticCode::kMaterialSidecarInvalid,
          source_uri, source_uri + ": material sidecar '" + path.string() + "' line " +
                          std::to_string(line_number) + " has invalid value for key '" +
                          key + "'");
      has_error = true;
    };

    if (key == "version") {
      const auto version = parse_int(value);
      if (!version.has_value() || *version != 1) {
        invalid_value();
      } else {
        sidecar.version = *version;
        saw_version = true;
      }
    } else if (key == "name") {
      sidecar.name = unquote(value);
    } else if (key == "base_color") {
      has_error = !set_texture_ref(sidecar.base_color, value, report, source_uri,
                                   path, line_number, key) ||
                  has_error;
    } else if (key == "normal") {
      has_error = !set_texture_ref(sidecar.normal, value, report, source_uri, path,
                                   line_number, key) ||
                  has_error;
    } else if (key == "specular") {
      has_error = !set_texture_ref(sidecar.specular, value, report, source_uri,
                                   path, line_number, key) ||
                  has_error;
    } else if (key == "metallic_roughness") {
      has_error = !set_texture_ref(sidecar.metallic_roughness, value, report,
                                   source_uri, path, line_number, key) ||
                  has_error;
    } else if (key == "occlusion") {
      has_error = !set_texture_ref(sidecar.occlusion, value, report, source_uri,
                                   path, line_number, key) ||
                  has_error;
    } else if (key == "emissive") {
      has_error = !set_texture_ref(sidecar.emissive, value, report, source_uri,
                                   path, line_number, key) ||
                  has_error;
    } else if (key == "normal_scale") {
      if (const auto parsed = parse_float(value)) {
        sidecar.normal_scale = clamp_float(*parsed, 0.0F, 8.0F);
      } else {
        invalid_value();
      }
    } else if (key == "normal_light_strength") {
      if (const auto parsed = parse_float(value)) {
        sidecar.normal_light_strength = clamp_float(*parsed, 0.0F, 8.0F);
      } else {
        invalid_value();
      }
    } else if (key == "specular_factor") {
      if (const auto parsed = parse_float(value)) {
        sidecar.specular_factor = clamp_float(*parsed, 0.0F, 8.0F);
      } else {
        invalid_value();
      }
    } else if (key == "specular_power") {
      if (const auto parsed = parse_float(value)) {
        sidecar.specular_power = clamp_float(*parsed, 1.0F, 512.0F);
      } else {
        invalid_value();
      }
    } else if (key == "metallic_factor") {
      if (const auto parsed = parse_float(value)) {
        sidecar.metallic_factor = clamp_float(*parsed, 0.0F, 1.0F);
      } else {
        invalid_value();
      }
    } else if (key == "roughness_factor") {
      if (const auto parsed = parse_float(value)) {
        sidecar.roughness_factor = clamp_float(*parsed, 0.0F, 1.0F);
      } else {
        invalid_value();
      }
    } else if (key == "occlusion_strength") {
      if (const auto parsed = parse_float(value)) {
        sidecar.occlusion_strength = clamp_float(*parsed, 0.0F, 1.0F);
      } else {
        invalid_value();
      }
    } else if (key == "emissive_factor") {
      if (const auto parsed = parse_vec3(value)) {
        sidecar.emissive_factor = *parsed;
      } else {
        invalid_value();
      }
    } else if (key == "alpha_mode") {
      std::string mode = unquote(value);
      std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
      });
      if (mode == "opaque") {
        sidecar.alpha_mode = stellar::assets::AlphaMode::kOpaque;
      } else if (mode == "mask") {
        sidecar.alpha_mode = stellar::assets::AlphaMode::kMask;
      } else if (mode == "blend") {
        sidecar.alpha_mode = stellar::assets::AlphaMode::kBlend;
      } else {
        invalid_value();
      }
    } else if (key == "alpha_cutoff") {
      if (const auto parsed = parse_float(value)) {
        sidecar.alpha_cutoff = clamp_float(*parsed, 0.0F, 1.0F);
      } else {
        invalid_value();
      }
    } else if (key == "double_sided") {
      if (const auto parsed = parse_bool(value)) {
        sidecar.double_sided = *parsed;
      } else {
        invalid_value();
      }
    } else if (key == "unlit") {
      if (const auto parsed = parse_bool(value)) {
        sidecar.unlit = *parsed;
      } else {
        invalid_value();
      }
    } else {
      const DiagnosticSeverity severity =
          strict ? DiagnosticSeverity::kError : DiagnosticSeverity::kWarning;
      add_sidecar_diagnostic(
          report, severity, DiagnosticCode::kMaterialSidecarUnknownKey, source_uri,
          source_uri + ": material sidecar '" + path.string() + "' line " +
              std::to_string(line_number) + " has unknown key '" + key +
              "' for BSP texture '" + texture_name + "'");
      has_error = strict || has_error;
    }
  }

  if (!saw_version) {
    add_sidecar_diagnostic(report, DiagnosticSeverity::kError,
                           DiagnosticCode::kMaterialSidecarInvalid, source_uri,
                           source_uri + ": material sidecar '" + path.string() +
                               "' is missing required version = 1");
    has_error = true;
  }
  if (sidecar.name.empty()) {
    sidecar.name = texture_name;
  }
  if (has_error) {
    return MaterialSidecarParseResult{.sidecar = std::nullopt,
                                      .has_error = true};
  }
  add_sidecar_diagnostic(report, DiagnosticSeverity::kInfo,
                         DiagnosticCode::kMaterialSidecarLoaded, source_uri,
                         source_uri + ": loaded material sidecar '" + path.string() +
                             "' for BSP texture '" + texture_name + "'");
  return MaterialSidecarParseResult{.sidecar = std::move(sidecar),
                                    .has_error = false};
}

MaterialSidecarResolver::MaterialSidecarResolver(
    stellar::assets::LevelGeometryAsset &geometry, const LoadOptions &options,
    std::string source_uri, ImportReport *report)
    : geometry_(geometry), options_(options), source_uri_(std::move(source_uri)),
      report_(report) {}

std::optional<std::filesystem::path>
MaterialSidecarResolver::find_sidecar_path(const std::string &normalized_key) const {
  const std::filesystem::path relative =
      std::filesystem::path(normalized_key).concat(".stellar_material");
  for (const auto &root : options_.material_search_roots) {
    const std::filesystem::path candidate = root / relative;
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) {
      return candidate;
    }
  }
  return std::nullopt;
}

std::expected<std::size_t, stellar::platform::Error>
MaterialSidecarResolver::append_texture(
    const MaterialSidecarTextureRef &ref, const std::filesystem::path &sidecar_path,
    const std::string &texture_name, stellar::assets::TextureColorSpace color_space) {
  const std::filesystem::path image_path = sidecar_path.parent_path() / ref.relative_path;
  const std::string cache_key = std::filesystem::absolute(image_path).lexically_normal().string();
  if (const auto existing = loaded_textures_.find(cache_key);
      existing != loaded_textures_.end()) {
    return existing->second;
  }
  auto image = load_image_file_rgba8(image_path, image_path.stem().string());
  if (!image) {
    const DiagnosticSeverity severity = options_.strict_material_sidecars
                                            ? DiagnosticSeverity::kError
                                            : DiagnosticSeverity::kWarning;
    add_sidecar_diagnostic(
        report_, severity, DiagnosticCode::kMaterialSidecarTextureMissing,
        source_uri_, source_uri_ + ": material sidecar texture '" + image_path.string() +
                         "' for BSP texture '" + texture_name +
                         "' could not be loaded: " + image.error());
    if (options_.strict_material_sidecars) {
      return std::unexpected(
          stellar::platform::Error("Strict material sidecar texture load failed: " +
                                  image_path.string()));
    }
    return std::unexpected(stellar::platform::Error(image.error()));
  }

  const std::size_t image_index = geometry_.images.size();
  geometry_.images.push_back(std::move(*image));

  stellar::assets::SamplerAsset sampler;
  sampler.name = texture_name + "_sidecar_repeat";
  sampler.mag_filter = stellar::assets::TextureFilter::kLinear;
  sampler.min_filter = stellar::assets::TextureFilter::kLinearMipmapLinear;
  sampler.wrap_s = stellar::assets::TextureWrapMode::kRepeat;
  sampler.wrap_t = stellar::assets::TextureWrapMode::kRepeat;
  const std::size_t sampler_index = geometry_.samplers.size();
  geometry_.samplers.push_back(std::move(sampler));

  const std::size_t texture_index = geometry_.textures.size();
  geometry_.textures.push_back(stellar::assets::TextureAsset{
      .name = image_path.stem().string(),
      .image_index = image_index,
      .sampler_index = sampler_index,
      .color_space = color_space});
  loaded_textures_.emplace(cache_key, texture_index);
  return texture_index;
}

std::expected<std::optional<stellar::assets::MaterialAsset>,
              stellar::platform::Error>
MaterialSidecarResolver::resolve(
    std::string_view texture_name, std::optional<std::size_t> fallback_texture_index) {
  if (!options_.parse_material_sidecars) {
    return std::optional<stellar::assets::MaterialAsset>{};
  }
  const auto normalized_key = normalize_material_sidecar_key(texture_name);
  if (!normalized_key) {
    add_sidecar_diagnostic(
        report_, DiagnosticSeverity::kWarning, DiagnosticCode::kMaterialSidecarUnsafePath,
        source_uri_, source_uri_ + ": BSP texture name '" + std::string(texture_name) +
                         "' cannot be used for material sidecar lookup: " +
                         normalized_key.error());
    return std::optional<stellar::assets::MaterialAsset>{};
  }
  const auto sidecar_path = find_sidecar_path(*normalized_key);
  if (!sidecar_path.has_value()) {
    return std::optional<stellar::assets::MaterialAsset>{};
  }

  const std::string cache_key = sidecar_path->string();
  auto parsed = sidecar_cache_.find(cache_key);
  if (parsed == sidecar_cache_.end()) {
    parsed =
        sidecar_cache_
            .emplace(cache_key,
                     parse_material_sidecar_file(*sidecar_path,
                                                 options_.strict_material_sidecars,
                                                 source_uri_, std::string(texture_name),
                                                 report_))
            .first;
  }
  if (!parsed->second.sidecar.has_value()) {
    if (options_.strict_material_sidecars && parsed->second.has_error) {
      return std::unexpected(stellar::platform::Error(
          "Strict material sidecar parse failed: " + sidecar_path->string()));
    }
    return std::optional<stellar::assets::MaterialAsset>{};
  }

  const MaterialSidecar &sidecar = *parsed->second.sidecar;
  stellar::assets::MaterialAsset material;
  material.name = sidecar.name;
  material.normal_scale = sidecar.normal_scale;
  material.normal_light_strength = sidecar.normal_light_strength;
  material.specular_factor = sidecar.specular_factor;
  material.specular_power = sidecar.specular_power;
  material.metallic_factor = sidecar.metallic_factor;
  material.roughness_factor = sidecar.roughness_factor;
  material.occlusion_strength = sidecar.occlusion_strength;
  material.emissive_factor = sidecar.emissive_factor;
  material.alpha_mode = sidecar.alpha_mode;
  material.alpha_cutoff = sidecar.alpha_cutoff;
  material.double_sided = sidecar.double_sided.value_or(true);
  material.unlit = sidecar.unlit.value_or(false);

  if (sidecar.base_color.has_value()) {
    auto texture = append_texture(*sidecar.base_color, *sidecar_path,
                                  std::string(texture_name),
                                  stellar::assets::TextureColorSpace::kSrgb);
    if (!texture) {
      if (options_.strict_material_sidecars) {
        return std::unexpected(texture.error());
      }
      return std::optional<stellar::assets::MaterialAsset>{};
    }
    material.base_color_texture = stellar::assets::MaterialTextureSlot{
        .texture_index = *texture, .texcoord_set = 0};
  } else if (fallback_texture_index.has_value()) {
    material.base_color_texture = stellar::assets::MaterialTextureSlot{
        .texture_index = *fallback_texture_index, .texcoord_set = 0};
  }

  const auto apply_slot = [&](const std::optional<MaterialSidecarTextureRef> &ref,
                              std::optional<stellar::assets::MaterialTextureSlot> &slot,
                              stellar::assets::TextureColorSpace color_space)
      -> std::expected<void, stellar::platform::Error> {
    if (!ref.has_value()) {
      return {};
    }
    auto texture = append_texture(*ref, *sidecar_path, std::string(texture_name),
                                  color_space);
    if (!texture) {
      return std::unexpected(texture.error());
    }
    slot = stellar::assets::MaterialTextureSlot{.texture_index = *texture,
                                                .texcoord_set = 0};
    return {};
  };

  const auto linear = stellar::assets::TextureColorSpace::kLinear;
  const auto srgb = stellar::assets::TextureColorSpace::kSrgb;
  if (auto result = apply_slot(sidecar.normal, material.normal_texture, linear);
      !result) {
    if (options_.strict_material_sidecars) {
      return std::unexpected(result.error());
    }
    return std::optional<stellar::assets::MaterialAsset>{};
  }
  if (auto result = apply_slot(sidecar.specular, material.specular_texture, linear);
      !result) {
    if (options_.strict_material_sidecars) {
      return std::unexpected(result.error());
    }
    return std::optional<stellar::assets::MaterialAsset>{};
  }
  if (auto result = apply_slot(sidecar.metallic_roughness,
                               material.metallic_roughness_texture, linear);
      !result) {
    if (options_.strict_material_sidecars) {
      return std::unexpected(result.error());
    }
    return std::optional<stellar::assets::MaterialAsset>{};
  }
  if (auto result = apply_slot(sidecar.occlusion, material.occlusion_texture, linear);
      !result) {
    if (options_.strict_material_sidecars) {
      return std::unexpected(result.error());
    }
    return std::optional<stellar::assets::MaterialAsset>{};
  }
  if (auto result = apply_slot(sidecar.emissive, material.emissive_texture, srgb);
      !result) {
    if (options_.strict_material_sidecars) {
      return std::unexpected(result.error());
    }
    return std::optional<stellar::assets::MaterialAsset>{};
  }

  return std::optional<stellar::assets::MaterialAsset>{std::move(material)};
}

} // namespace stellar::import::bsp::detail
