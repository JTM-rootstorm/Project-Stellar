#include "ImporterPrivate.hpp"


namespace stellar::import::gltf {

std::expected<stellar::assets::MaterialAsset, stellar::platform::Error>
load_material(const cgltf_data* data, const cgltf_material* material) {
    stellar::assets::MaterialAsset result;
    if (!material) {
        return result;
    }

    result.name = duplicate_to_string(material->name);
    for (std::size_t i = 0; i < 4; ++i) {
        result.base_color_factor[i] = material->pbr_metallic_roughness.base_color_factor[i];
    }
    result.metallic_factor = material->pbr_metallic_roughness.metallic_factor;
    result.roughness_factor = material->pbr_metallic_roughness.roughness_factor;
    result.alpha_cutoff = material->alpha_cutoff;
    result.double_sided = material->double_sided;
    result.unlit = material->unlit;

    switch (material->alpha_mode) {
        case cgltf_alpha_mode_mask:
            result.alpha_mode = stellar::assets::AlphaMode::kMask;
            break;
        case cgltf_alpha_mode_blend:
            result.alpha_mode = stellar::assets::AlphaMode::kBlend;
            break;
        case cgltf_alpha_mode_opaque:
        default:
            result.alpha_mode = stellar::assets::AlphaMode::kOpaque;
            break;
    }

    auto load_texture_slot = [data](const cgltf_texture_view& view, const char* error_message)
        -> std::expected<stellar::assets::MaterialTextureSlot, stellar::platform::Error> {
        auto index = checked_index(cgltf_texture_index(data, view.texture), error_message);
        if (!index) {
            return std::unexpected(index.error());
        }

        stellar::assets::MaterialTextureSlot slot;
        slot.texture_index = *index;
        slot.texcoord_set = view.texcoord < 0 ? 0u : static_cast<std::uint32_t>(view.texcoord);
        slot.scale = view.scale;
        if (view.has_transform) {
            slot.transform.enabled = true;
            slot.transform.offset = {view.transform.offset[0], view.transform.offset[1]};
            slot.transform.rotation = view.transform.rotation;
            slot.transform.scale = {view.transform.scale[0], view.transform.scale[1]};
            if (view.transform.has_texcoord && view.transform.texcoord >= 0) {
                slot.transform.texcoord_set = static_cast<std::uint32_t>(view.transform.texcoord);
            }
        }
        return slot;
    };

    if (material->has_pbr_metallic_roughness &&
        material->pbr_metallic_roughness.base_color_texture.texture) {
        auto slot = load_texture_slot(material->pbr_metallic_roughness.base_color_texture,
                                      "Failed to resolve base color texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.base_color_texture = *slot;
    }

    if (material->normal_texture.texture) {
        auto slot = load_texture_slot(material->normal_texture,
                                      "Failed to resolve normal texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.normal_texture = *slot;
        result.normal_texture->scale = material->normal_texture.scale;
    }

    if (material->pbr_metallic_roughness.metallic_roughness_texture.texture) {
        auto slot = load_texture_slot(material->pbr_metallic_roughness.metallic_roughness_texture,
                                      "Failed to resolve metallic/roughness texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.metallic_roughness_texture = *slot;
    }

    if (material->occlusion_texture.texture) {
        auto slot = load_texture_slot(material->occlusion_texture,
                                      "Failed to resolve occlusion texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.occlusion_texture = *slot;
        result.occlusion_strength = material->occlusion_texture.scale;
    }

    if (material->emissive_texture.texture) {
        auto slot = load_texture_slot(material->emissive_texture,
                                      "Failed to resolve emissive texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.emissive_texture = *slot;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        result.emissive_factor[i] = material->emissive_factor[i];
    }

    return result;
}

} // namespace stellar::import::gltf
