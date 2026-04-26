#include "stellar/graphics/RenderScene.hpp"

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace stellar::graphics {

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
        auto handle = device_->create_texture(image);
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
            const auto& slot = *material.base_color_texture;
            if (slot.texture_index < texture_handles_.size() && texture_handles_[slot.texture_index]) {
                stellar::assets::SamplerAsset sampler;
                const auto& texture = scene_.textures[slot.texture_index];
                if (texture.sampler_index.has_value() &&
                    *texture.sampler_index < scene_.samplers.size()) {
                    sampler = scene_.samplers[*texture.sampler_index];
                }
                upload.base_color_texture = MaterialTextureBinding{
                    .texture = texture_handles_[slot.texture_index],
                    .sampler = sampler,
                    .texcoord_set = slot.texcoord_set,
                };
            }
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
                         const std::array<float, 16>& view_projection) noexcept {
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
    const auto& scene = scene_.scenes[scene_index];
    for (std::size_t root_node : scene.root_nodes) {
        render_node(root_node, identity_array, view_projection);
    }

    device_->end_frame();
}

stellar::scene::Transform& RenderScene::node_transform(std::size_t node_index) noexcept {
    return scene_.nodes[node_index].local_transform;
}

const stellar::scene::Transform& RenderScene::node_transform(std::size_t node_index) const noexcept {
    return scene_.nodes[node_index].local_transform;
}

std::array<float, 16>
RenderScene::compose_transform(const stellar::scene::Transform& transform) noexcept {
    if (transform.matrix.has_value()) {
        return *transform.matrix;
    }

    glm::mat4 translation = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(transform.translation[0], transform.translation[1], transform.translation[2]));
    glm::quat rotation(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                       transform.rotation[2]);
    glm::mat4 rotation_matrix = glm::mat4_cast(rotation);
    glm::mat4 scale = glm::scale(
        glm::mat4(1.0f), glm::vec3(transform.scale[0], transform.scale[1], transform.scale[2]));

    return to_array(translation * rotation_matrix * scale);
}

void RenderScene::render_node(std::size_t node_index,
                              const std::array<float, 16>& parent_world,
                              const std::array<float, 16>& view_projection) noexcept {
    if (node_index >= scene_.nodes.size()) {
        return;
    }

    const auto& node = scene_.nodes[node_index];
    const glm::mat4 parent = to_glm_mat4(parent_world);
    const glm::mat4 local = to_glm_mat4(compose_transform(node.local_transform));
    const glm::mat4 world = parent * local;
    const glm::mat4 vp = to_glm_mat4(view_projection);
    const std::array<float, 16> mvp = to_array(vp * world);

    for (const auto& mesh_instance : node.mesh_instances) {
        if (mesh_instance.mesh_index < mesh_handles_.size()) {
            std::vector<MaterialHandle> materials;
            const auto& mesh_asset = scene_.meshes[mesh_instance.mesh_index];
            materials.reserve(mesh_asset.primitives.size());
            for (const auto& primitive : mesh_asset.primitives) {
                if (primitive.material_index.has_value() &&
                    *primitive.material_index < material_handles_.size()) {
                    materials.push_back(material_handles_[*primitive.material_index]);
                } else {
                    materials.push_back(MaterialHandle{});
                }
            }

            device_->draw_mesh(mesh_handles_[mesh_instance.mesh_index], materials, mvp);
        }
    }

    const std::array<float, 16> world_array = to_array(world);
    for (std::size_t child_index : node.children) {
        render_node(child_index, world_array, view_projection);
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
