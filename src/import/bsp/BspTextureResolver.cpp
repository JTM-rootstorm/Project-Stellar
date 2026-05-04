#include "BspTextureResolver.hpp"

#include "BspImportDiagnostics.hpp"
#include "DeveloperTextures.hpp"

#include <cctype>
#include <utility>

namespace stellar::import::bsp::detail {

std::string texture_name_for(const BspMap &map, const Face &face) {
  if (face.texinfo >= map.texinfos.size()) {
    return "__missing_texture";
  }
  const std::int32_t miptex = map.texinfos[face.texinfo].miptex;
  if (miptex >= 0 && static_cast<std::size_t>(miptex) < map.texture_names.size() &&
      !map.texture_names[static_cast<std::size_t>(miptex)].empty()) {
    return map.texture_names[static_cast<std::size_t>(miptex)];
  }
  return "__texture_" + std::to_string(miptex);
}

const Miptex *texture_for_face(const BspMap &map, const Face &face) noexcept {
  if (face.texinfo >= map.texinfos.size()) {
    return nullptr;
  }
  const std::int32_t miptex = map.texinfos[face.texinfo].miptex;
  if (miptex < 0 || static_cast<std::size_t>(miptex) >= map.textures.size()) {
    return nullptr;
  }
  return &map.textures[static_cast<std::size_t>(miptex)];
}

std::string texture_lookup_key(std::string_view name) {
  std::string key;
  key.reserve(name.size());
  for (const char ch : name) {
    key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return key;
}

TextureBuildResult texture_asset_index(
    stellar::assets::LevelGeometryAsset &geometry,
    std::map<std::string, std::size_t> &textures,
    const std::unordered_map<std::string, WadTextureAsset> &wad_textures,
    const std::string &texture_name, const Miptex *miptex,
    const std::string &source_uri, ImportReport *report) {
  if (texture_name.empty()) {
    return {};
  }
  const auto existing = textures.find(texture_name);
  if (existing != textures.end()) {
    const auto &texture = geometry.textures[existing->second];
    if (texture.image_index.has_value() && *texture.image_index < geometry.images.size()) {
      const auto &image = geometry.images[*texture.image_index];
      return TextureBuildResult{.texture_index = existing->second,
                                .texel_size = std::array<std::uint32_t, 2>{
                                    image.width, image.height}};
    }
    return TextureBuildResult{.texture_index = existing->second};
  }

  if (miptex != nullptr && miptex->has_embedded_pixels && !miptex->pixels.empty()) {
    stellar::assets::ImageAsset image{};
    image.name = miptex->name;
    image.width = miptex->width;
    image.height = miptex->height;
    image.format = stellar::assets::ImageFormat::kR8G8B8A8;
    image.source_uri = source_uri + "#miptex/" + miptex->name;
    image.pixels.reserve(miptex->pixels.size() * 4);
    for (const std::uint8_t index : miptex->pixels) {
      image.pixels.push_back(index);
      image.pixels.push_back(index);
      image.pixels.push_back(index);
      image.pixels.push_back(255U);
    }
    const std::size_t image_index = geometry.images.size();
    geometry.images.push_back(std::move(image));

    const std::size_t texture_index = geometry.textures.size();
    geometry.textures.push_back(stellar::assets::TextureAsset{
        .name = miptex->name,
        .image_index = image_index,
        .sampler_index = std::nullopt,
        .color_space = stellar::assets::TextureColorSpace::kSrgb});
    textures.emplace(texture_name, texture_index);
    return TextureBuildResult{.texture_index = texture_index,
                              .texel_size = std::array<std::uint32_t, 2>{
                                  miptex->width, miptex->height}};
  }

  if (const auto wad = wad_textures.find(texture_lookup_key(texture_name));
      wad != wad_textures.end()) {
    stellar::assets::ImageAsset image = wad->second.image;
    image.name = texture_name;
    const std::size_t image_index = geometry.images.size();
    const std::array<std::uint32_t, 2> texel_size{image.width, image.height};
    geometry.images.push_back(std::move(image));

    stellar::assets::SamplerAsset sampler;
    sampler.name = texture_name + "_wad_repeat";
    sampler.mag_filter = stellar::assets::TextureFilter::kNearest;
    sampler.min_filter = stellar::assets::TextureFilter::kNearest;
    sampler.wrap_s = stellar::assets::TextureWrapMode::kRepeat;
    sampler.wrap_t = stellar::assets::TextureWrapMode::kRepeat;
    const std::size_t sampler_index = geometry.samplers.size();
    geometry.samplers.push_back(std::move(sampler));

    const std::size_t texture_index = geometry.textures.size();
    geometry.textures.push_back(stellar::assets::TextureAsset{
        .name = texture_name,
        .image_index = image_index,
        .sampler_index = sampler_index,
        .color_space = stellar::assets::TextureColorSpace::kSrgb});
    textures.emplace(texture_name, texture_index);
    return TextureBuildResult{.texture_index = texture_index, .texel_size = texel_size};
  }

  if (auto developer_texture = make_developer_texture(texture_name, source_uri)) {
    const std::size_t image_index = geometry.images.size();
    const std::array<std::uint32_t, 2> texel_size{developer_texture->image.width,
                                                 developer_texture->image.height};
    geometry.images.push_back(std::move(developer_texture->image));

    const std::size_t sampler_index = geometry.samplers.size();
    geometry.samplers.push_back(std::move(developer_texture->sampler));

    const std::size_t texture_index = geometry.textures.size();
    developer_texture->texture.image_index = image_index;
    developer_texture->texture.sampler_index = sampler_index;
    geometry.textures.push_back(std::move(developer_texture->texture));
    textures.emplace(texture_name, texture_index);
    return TextureBuildResult{.texture_index = texture_index, .texel_size = texel_size};
  }

  if (miptex != nullptr) {
    add_warning(report, DiagnosticCode::kMissingTexture, source_uri,
                source_uri + ": BSP texture '" + texture_name +
                    "' references external WAD data; using fallback material",
                static_cast<std::size_t>(LumpIndex::kTextures));
  }
  return {};
}

std::size_t material_index(stellar::assets::LevelGeometryAsset &geometry,
                           std::map<std::string, std::size_t> &materials,
                           const std::string &name,
                           std::optional<std::size_t> texture_index,
                           std::optional<std::size_t> lightmap_index) {
  const std::string key = name + "|tex=" +
                          (texture_index ? std::to_string(*texture_index) : "none") +
                          "|lm=" +
                          (lightmap_index ? std::to_string(*lightmap_index) : "none");
  const auto existing = materials.find(key);
  if (existing != materials.end()) {
    return existing->second;
  }
  const std::size_t index = geometry.materials.size();
  geometry.materials.push_back(stellar::assets::LevelSurfaceMaterial{
      .name = name,
      .texture_index = texture_index,
      .lightmap_index = lightmap_index,
      .source_name = name});
  materials.emplace(key, index);
  return index;
}

} // namespace stellar::import::bsp::detail
