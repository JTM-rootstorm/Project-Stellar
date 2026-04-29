#include "ImporterPrivate.hpp"


namespace stellar::import::gltf {
namespace {

stellar::assets::TextureFilter to_texture_filter(cgltf_filter_type filter) {
    switch (filter) {
        case cgltf_filter_type_nearest:
            return stellar::assets::TextureFilter::kNearest;
        case cgltf_filter_type_linear:
            return stellar::assets::TextureFilter::kLinear;
        case cgltf_filter_type_nearest_mipmap_nearest:
            return stellar::assets::TextureFilter::kNearestMipmapNearest;
        case cgltf_filter_type_linear_mipmap_nearest:
            return stellar::assets::TextureFilter::kLinearMipmapNearest;
        case cgltf_filter_type_nearest_mipmap_linear:
            return stellar::assets::TextureFilter::kNearestMipmapLinear;
        case cgltf_filter_type_linear_mipmap_linear:
            return stellar::assets::TextureFilter::kLinearMipmapLinear;
        case cgltf_filter_type_undefined:
        default:
            return stellar::assets::TextureFilter::kUnspecified;
    }
}

stellar::assets::TextureWrapMode to_texture_wrap_mode(cgltf_wrap_mode mode) {
    switch (mode) {
        case cgltf_wrap_mode_clamp_to_edge:
            return stellar::assets::TextureWrapMode::kClampToEdge;
        case cgltf_wrap_mode_mirrored_repeat:
            return stellar::assets::TextureWrapMode::kMirroredRepeat;
        case cgltf_wrap_mode_repeat:
        default:
            return stellar::assets::TextureWrapMode::kRepeat;
    }
}

} // namespace

void classify_texture_color_spaces(stellar::assets::SceneAsset& scene) noexcept {
    for (auto& texture : scene.textures) {
        texture.color_space = stellar::assets::TextureColorSpace::kLinear;
    }

    auto mark_srgb = [&scene](const std::optional<stellar::assets::MaterialTextureSlot>& slot) {
        if (slot.has_value() && slot->texture_index < scene.textures.size()) {
            scene.textures[slot->texture_index].color_space =
                stellar::assets::TextureColorSpace::kSrgb;
        }
    };

    for (const auto& material : scene.materials) {
        mark_srgb(material.base_color_texture);
        mark_srgb(material.emissive_texture);
        // Normal, metallic-roughness, and occlusion textures remain linear non-color data. If a
        // glTF reuses one texture for color and non-color slots, the current one-upload-per-texture
        // asset model cannot assign per-use color spaces; color usage wins until texture views are
        // split during upload in a later renderer phase.
    }
}

std::expected<stellar::assets::SamplerAsset, stellar::platform::Error>
load_sampler(const cgltf_sampler& sampler) {
    stellar::assets::SamplerAsset result;
    result.name = duplicate_to_string(sampler.name);
    result.mag_filter = to_texture_filter(sampler.mag_filter);
    result.min_filter = to_texture_filter(sampler.min_filter);
    result.wrap_s = to_texture_wrap_mode(sampler.wrap_s);
    result.wrap_t = to_texture_wrap_mode(sampler.wrap_t);
    return result;
}

std::expected<stellar::assets::TextureAsset, stellar::platform::Error>
load_texture(const cgltf_data* data, const cgltf_texture& texture) {
    stellar::assets::TextureAsset result;
    result.name = duplicate_to_string(texture.name);

    if (texture.image) {
        auto image_index = checked_index(cgltf_image_index(data, texture.image),
                                         "Failed to resolve texture image index");
        if (!image_index) {
            return std::unexpected(image_index.error());
        }
        result.image_index = *image_index;
    }

    if (texture.sampler) {
        auto sampler_index = checked_index(cgltf_sampler_index(data, texture.sampler),
                                           "Failed to resolve texture sampler index");
        if (!sampler_index) {
            return std::unexpected(sampler_index.error());
        }
        result.sampler_index = *sampler_index;
    }

    return result;
}

} // namespace stellar::import::gltf
