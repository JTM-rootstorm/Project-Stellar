#include "stellar/graphics/BillboardSprite.hpp"

#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace stellar::graphics {
namespace {

glm::mat4 to_glm_mat4(const std::array<float, 16>& data) noexcept {
    return glm::make_mat4(data.data());
}

glm::vec3 to_vec3(const std::array<float, 3>& value) noexcept {
    return {value[0], value[1], value[2]};
}

std::array<float, 3> to_array(const glm::vec3& value) noexcept {
    return {value.x, value.y, value.z};
}

float view_space_depth(const glm::mat4& view, const std::array<float, 3>& position) noexcept {
    const glm::vec4 view_position = view * glm::vec4(position[0], position[1], position[2], 1.0F);
    return view_position.z;
}

} // namespace

std::vector<BillboardQuad> build_billboard_quads(std::span<const BillboardSprite> sprites,
                                                 const BillboardView& view) noexcept {
    const glm::mat4 view_matrix = to_glm_mat4(view.view);
    const glm::vec3 right = to_vec3(view.camera_right);
    const glm::vec3 up = to_vec3(view.camera_up);

    std::vector<BillboardQuad> opaque_or_mask;
    std::vector<BillboardQuad> blend;
    opaque_or_mask.reserve(sprites.size());
    blend.reserve(sprites.size());

    for (std::size_t index = 0; index < sprites.size(); ++index) {
        const BillboardSprite& sprite = sprites[index];
        const glm::vec3 center = to_vec3(sprite.position);
        const glm::vec3 half_right = right * (sprite.size[0] * 0.5F);
        const glm::vec3 half_up = up * (sprite.size[1] * 0.5F);

        BillboardQuad quad;
        quad.positions = {to_array(center - half_right - half_up),
                          to_array(center + half_right - half_up),
                          to_array(center + half_right + half_up),
                          to_array(center - half_right + half_up)};
        quad.texcoords = {std::array<float, 2>{0.0F, 1.0F}, {1.0F, 1.0F},
                          {1.0F, 0.0F}, {0.0F, 0.0F}};
        quad.center = sprite.position;
        quad.size = sprite.size;
        quad.color = sprite.color;
        quad.texture = sprite.texture;
        quad.sampler = sprite.sampler;
        quad.alpha_mask = sprite.alpha_mask;
        quad.alpha_blend = sprite.alpha_blend && !sprite.alpha_mask;
        quad.alpha_cutoff = sprite.alpha_cutoff;
        quad.depth = view_space_depth(view_matrix, sprite.position);
        quad.submission_index = index;

        if (quad.alpha_blend) {
            blend.push_back(quad);
        } else {
            opaque_or_mask.push_back(quad);
        }
    }

    std::stable_sort(blend.begin(), blend.end(), [](const BillboardQuad& lhs,
                                                    const BillboardQuad& rhs) {
        if (lhs.depth == rhs.depth) {
            return lhs.submission_index < rhs.submission_index;
        }
        return lhs.depth < rhs.depth;
    });

    opaque_or_mask.insert(opaque_or_mask.end(), blend.begin(), blend.end());
    return opaque_or_mask;
}

} // namespace stellar::graphics
