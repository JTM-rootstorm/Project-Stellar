#pragma once

#include "ImageFileLoader.hpp"
#include "stellar/assets/LevelAsset.hpp"
#include "stellar/import/bsp/Loader.hpp"

#include <array>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

namespace stellar::import::bsp::detail {

struct MaterialSidecarTextureRef {
  std::string relative_path;
};

struct MaterialSidecar {
  int version = 1;
  std::string name;
  std::optional<MaterialSidecarTextureRef> base_color;
  std::optional<MaterialSidecarTextureRef> normal;
  std::optional<MaterialSidecarTextureRef> specular;
  std::optional<MaterialSidecarTextureRef> metallic_roughness;
  std::optional<MaterialSidecarTextureRef> occlusion;
  std::optional<MaterialSidecarTextureRef> emissive;
  float normal_scale = 1.0F;
  float normal_light_strength = 0.0F;
  float specular_factor = 0.0F;
  float specular_power = 32.0F;
  float metallic_factor = 0.0F;
  float roughness_factor = 1.0F;
  float occlusion_strength = 1.0F;
  std::array<float, 3> emissive_factor{0.0F, 0.0F, 0.0F};
  stellar::assets::AlphaMode alpha_mode = stellar::assets::AlphaMode::kOpaque;
  float alpha_cutoff = 0.5F;
  std::optional<bool> double_sided;
  std::optional<bool> unlit;
};

struct MaterialSidecarParseResult {
  std::optional<MaterialSidecar> sidecar;
  bool has_error = false;
};

[[nodiscard]] std::expected<std::string, std::string>
normalize_material_sidecar_key(std::string_view texture_name);

[[nodiscard]] MaterialSidecarParseResult
parse_material_sidecar_file(const std::filesystem::path &path, bool strict,
                            const std::string &source_uri,
                            const std::string &texture_name,
                            ImportReport *report);

class MaterialSidecarResolver {
public:
  MaterialSidecarResolver(stellar::assets::LevelGeometryAsset &geometry,
                          const LoadOptions &options, std::string source_uri,
                          ImportReport *report);

  [[nodiscard]] std::expected<std::optional<stellar::assets::MaterialAsset>,
                              stellar::platform::Error>
  resolve(std::string_view texture_name,
          std::optional<std::size_t> fallback_texture_index);

private:
  [[nodiscard]] std::optional<std::filesystem::path>
  find_sidecar_path(const std::string &normalized_key) const;

  [[nodiscard]] std::expected<std::size_t, stellar::platform::Error>
  append_texture(const MaterialSidecarTextureRef &ref,
                 const std::filesystem::path &sidecar_path,
                 const std::string &texture_name,
                 stellar::assets::TextureColorSpace color_space);

  stellar::assets::LevelGeometryAsset &geometry_;
  const LoadOptions &options_;
  std::string source_uri_;
  ImportReport *report_ = nullptr;
  std::unordered_map<std::string, MaterialSidecarParseResult> sidecar_cache_;
  std::map<std::string, std::size_t> loaded_textures_;
};

} // namespace stellar::import::bsp::detail
