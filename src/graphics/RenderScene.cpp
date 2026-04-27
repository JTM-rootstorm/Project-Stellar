#include "stellar/graphics/RenderScene.hpp"

#include <cmath>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "stellar/scene/AnimationRuntime.hpp"

namespace stellar::graphics {

struct QueuedMeshDraw {
    MeshHandle mesh;
    MeshPrimitiveDrawCommand command;
    MeshDrawTransforms transforms;
    float depth = 0.0f;
};

namespace {

glm::mat4 to_glm_mat4(const std::array<float, 16>& data) {
    return glm::make_mat4(data.data());
}

std::array<float, 16> to_array(const glm::mat4& matrix) {
    std::array<float, 16> result{};
    const float* data = &matrix[0][0];
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = data[index];
    }
    return result;
}

std::array<float, 9> to_array(const glm::mat3& matrix) {
    std::array<float, 9> result{};
    const float* data = &matrix[0][0];
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = data[index];
    }
    return result;
}

std::array<float, 9> normal_matrix_for(const glm::mat4& world) {
    const glm::mat3 linear(world);
    if (std::abs(glm::determinant(linear)) < 0.000001f) {
        return to_array(glm::mat3(1.0f));
    }

    return to_array(glm::transpose(glm::inverse(linear)));
}

std::optional<MaterialTextureBinding>
resolve_material_texture_binding(const stellar::assets::MaterialTextureSlot& slot,
                                 const stellar::assets::SceneAsset& scene,
                                 const std::vector<TextureHandle>& texture_handles) {
    if (slot.texture_index >= texture_handles.size() || !texture_handles[slot.texture_index]) {
        return std::nullopt;
    }

    stellar::assets::SamplerAsset sampler;
    const auto& texture = scene.textures[slot.texture_index];
    if (texture.sampler_index.has_value() && *texture.sampler_index < scene.samplers.size()) {
        sampler = scene.samplers[*texture.sampler_index];
    }

    return MaterialTextureBinding{
        .texture = texture_handles[slot.texture_index],
        .sampler = sampler,
        .texcoord_set = slot.texcoord_set,
    };
}

std::array<float, 3> primitive_center(const stellar::assets::MeshPrimitive& primitive) noexcept {
    return {(primitive.bounds_min[0] + primitive.bounds_max[0]) * 0.5f,
            (primitive.bounds_min[1] + primitive.bounds_max[1]) * 0.5f,
            (primitive.bounds_min[2] + primitive.bounds_max[2]) * 0.5f};
}

float view_space_depth(const glm::mat4& view,
                       const glm::mat4& world,
                       const stellar::assets::MeshPrimitive& primitive) noexcept {
    const auto center = primitive_center(primitive);
    const glm::vec4 view_position = view * world * glm::vec4(center[0], center[1], center[2], 1.0f);
    return view_position.z;
}

} // namespace

RenderScene::~RenderScene() noexcept {
    destroy();
}

std::expected<void, stellar::platform::Error>
RenderScene::initialize(std::unique_ptr<GraphicsDevice> device,
                        stellar::platform::Window& window,
                        stellar::assets::SceneAsset scene) {
    destroy();

    if (!device) {
        return std::unexpected(stellar::platform::Error("Graphics device is null"));
    }

    device_ = std::move(device);
    scene_ = std::move(scene);

    if (auto result = device_->initialize(window); !result) {
        destroy();
        return result;
    }

    mesh_handles_.reserve(scene_.meshes.size());
    for (const auto& mesh : scene_.meshes) {
        auto handle = device_->create_mesh(mesh);
        if (!handle) {
            destroy();
            return std::unexpected(handle.error());
        }
        mesh_handles_.push_back(*handle);
    }

    texture_handles_.resize(scene_.textures.size());
    owned_texture_handles_.reserve(scene_.textures.size());
    for (std::size_t texture_index = 0; texture_index < scene_.textures.size(); ++texture_index) {
        const auto& texture = scene_.textures[texture_index];
        if (!texture.image_index.has_value() || *texture.image_index >= scene_.images.size()) {
            continue;
        }

        const auto& image = scene_.images[*texture.image_index];
        auto handle = device_->create_texture(TextureUpload{.image = image,
                                                            .color_space = texture.color_space});
        if (!handle) {
            destroy();
            return std::unexpected(handle.error());
        }
        texture_handles_[texture_index] = *handle;
        owned_texture_handles_.push_back(*handle);
    }

    material_handles_.reserve(scene_.materials.size());
    for (const auto& material : scene_.materials) {
        MaterialUpload upload;
        upload.material = material;
        if (material.base_color_texture.has_value()) {
            upload.base_color_texture = resolve_material_texture_binding(
                *material.base_color_texture, scene_, texture_handles_);
        }
        if (material.normal_texture.has_value()) {
            upload.normal_texture = resolve_material_texture_binding(
                *material.normal_texture, scene_, texture_handles_);
        }
        if (material.metallic_roughness_texture.has_value()) {
            upload.metallic_roughness_texture = resolve_material_texture_binding(
                *material.metallic_roughness_texture, scene_, texture_handles_);
        }
        if (material.occlusion_texture.has_value()) {
            upload.occlusion_texture = resolve_material_texture_binding(
                *material.occlusion_texture, scene_, texture_handles_);
        }
        if (material.emissive_texture.has_value()) {
            upload.emissive_texture = resolve_material_texture_binding(
                *material.emissive_texture, scene_, texture_handles_);
        }

        auto handle = device_->create_material(upload);
        if (!handle) {
            destroy();
            return std::unexpected(handle.error());
        }
        material_handles_.push_back(*handle);
    }

    if (scene_.default_scene_index.has_value()) {
        active_scene_index_ = scene_.default_scene_index;
    } else if (!scene_.scenes.empty()) {
        active_scene_index_ = 0;
    } else {
        active_scene_index_.reset();
    }

    return {};
}

void RenderScene::render(int width,
                         int height,
                         const std::array<float, 16>& view_projection,
                         const std::array<float, 16>& view) noexcept {
    if (!device_ || !active_scene_index_.has_value()) {
        return;
    }

    const std::size_t scene_index = *active_scene_index_;
    if (scene_index >= scene_.scenes.size()) {
        return;
    }

    device_->begin_frame(width, height);

    const glm::mat4 identity = glm::mat4(1.0f);
    const std::array<float, 16> identity_array = to_array(identity);
    const glm::mat4 vp = to_glm_mat4(view_projection);
    const glm::mat4 view_matrix = to_glm_mat4(view);
    std::vector<QueuedMeshDraw> opaque_draws;
    std::vector<QueuedMeshDraw> blend_draws;
    const auto& scene = scene_.scenes[scene_index];
    for (std::size_t root_node : scene.root_nodes) {
        collect_node_draws(root_node, identity_array, vp, view_matrix, opaque_draws, blend_draws);
    }

    std::sort(blend_draws.begin(), blend_draws.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.depth < rhs.depth;
    });
    // Alpha BLEND primitives are sorted by primitive bounds center in view space. This keeps the
    // static path display-free and backend-neutral, but intersecting or concave transparent
    // geometry can still require asset-side splitting or later order-independent transparency.

    for (const QueuedMeshDraw& draw : opaque_draws) {
        device_->draw_mesh(draw.mesh, std::span<const MeshPrimitiveDrawCommand>(&draw.command, 1),
                           draw.transforms);
    }
    for (const QueuedMeshDraw& draw : blend_draws) {
        device_->draw_mesh(draw.mesh, std::span<const MeshPrimitiveDrawCommand>(&draw.command, 1),
                           draw.transforms);
    }

    device_->end_frame();
}

void RenderScene::render(int width,
                         int height,
                         const std::array<float, 16>& view_projection) noexcept {
    render(width, height, view_projection, view_projection);
}

stellar::scene::Transform& RenderScene::node_transform(std::size_t node_index) noexcept {
    return scene_.nodes[node_index].local_transform;
}

const stellar::scene::Transform&
RenderScene::node_transform(std::size_t node_index) const noexcept {
    return scene_.nodes[node_index].local_transform;
}

std::array<float, 16>
RenderScene::compose_transform(const stellar::scene::Transform& transform) noexcept {
    return stellar::scene::compose_transform(transform);
}

void RenderScene::collect_node_draws(std::size_t node_index,
                                      const std::array<float, 16>& parent_world,
                                      const glm::mat4& view_projection,
                                      const glm::mat4& view,
                                      std::vector<QueuedMeshDraw>& opaque_draws,
                                      std::vector<QueuedMeshDraw>& blend_draws) noexcept {
    if (node_index >= scene_.nodes.size()) {
        return;
    }

    const auto& node = scene_.nodes[node_index];
    const glm::mat4 parent = to_glm_mat4(parent_world);
    const glm::mat4 local = to_glm_mat4(compose_transform(node.local_transform));
    const glm::mat4 world = parent * local;
    const std::array<float, 16> mvp = to_array(view_projection * world);

    for (const auto& mesh_instance : node.mesh_instances) {
        if (mesh_instance.mesh_index < mesh_handles_.size()) {
            const auto& mesh_asset = scene_.meshes[mesh_instance.mesh_index];
            const MeshDrawTransforms transforms{
                .mvp = mvp,
                .world = to_array(world),
                .normal = normal_matrix_for(world),
            };
            for (std::size_t primitive_index = 0; primitive_index < mesh_asset.primitives.size();
                 ++primitive_index) {
                const auto& primitive = mesh_asset.primitives[primitive_index];
                MaterialHandle material_handle{};
                if (primitive.material_index.has_value() &&
                    *primitive.material_index < material_handles_.size()) {
                    material_handle = material_handles_[*primitive.material_index];
                }

                const bool is_blend = primitive.material_index.has_value() &&
                    *primitive.material_index < scene_.materials.size() &&
                    scene_.materials[*primitive.material_index].alpha_mode ==
                        stellar::assets::AlphaMode::kBlend;
                QueuedMeshDraw draw{
                    .mesh = mesh_handles_[mesh_instance.mesh_index],
                    .command = MeshPrimitiveDrawCommand{.primitive_index = primitive_index,
                                                        .material = material_handle},
                    .transforms = transforms,
                    .depth = view_space_depth(view, world, primitive),
                };
                if (is_blend) {
                    blend_draws.push_back(draw);
                } else {
                    opaque_draws.push_back(draw);
                }
            }
        }
    }

    const std::array<float, 16> world_array = to_array(world);
    for (std::size_t child_index : node.children) {
        collect_node_draws(child_index, world_array, view_projection, view, opaque_draws,
                           blend_draws);
    }
}

void RenderScene::destroy() noexcept {
    if (device_) {
        for (auto handle : material_handles_) {
            device_->destroy_material(handle);
        }
        for (auto handle : owned_texture_handles_) {
            device_->destroy_texture(handle);
        }
        for (auto handle : mesh_handles_) {
            device_->destroy_mesh(handle);
        }
    }

    material_handles_.clear();
    texture_handles_.clear();
    owned_texture_handles_.clear();
    mesh_handles_.clear();
    scene_ = {};
    active_scene_index_.reset();
    device_.reset();
}

} // namespace stellar::graphics
