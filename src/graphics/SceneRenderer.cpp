#include "stellar/graphics/SceneRenderer.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "stellar/graphics/GraphicsDeviceFactory.hpp"

namespace stellar::graphics {

namespace {

using Vec3 = std::array<float, 3>;
constexpr float kDefaultFovDegrees = 45.0F;
constexpr float kRotationSpeedDegreesPerSecond = 45.0F;

stellar::assets::MeshPrimitive make_face_primitive(const std::array<Vec3, 4>& positions,
                                                   const Vec3& normal,
                                                   std::size_t material_index) {
    stellar::assets::MeshPrimitive primitive;
    primitive.topology = stellar::assets::PrimitiveTopology::kTriangles;
    primitive.vertices = {
        {positions[0], normal, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
        {positions[1], normal, {1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
        {positions[2], normal, {1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
        {positions[3], normal, {0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
    };
    primitive.indices = {0, 1, 2, 0, 2, 3};
    primitive.bounds_min = {-0.5f, -0.5f, -0.5f};
    primitive.bounds_max = {0.5f, 0.5f, 0.5f};
    primitive.material_index = material_index;
    return primitive;
}

std::array<float, 16> to_array(const glm::mat4& matrix) {
    std::array<float, 16> result{};
    const float* data = &matrix[0][0];
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = data[index];
    }
    return result;
}

glm::mat4 to_glm_mat4(const std::array<float, 16>& data) noexcept {
    return glm::make_mat4(data.data());
}

void include_point(SceneBounds& bounds, const glm::vec3& point) noexcept {
    bounds.min[0] = std::min(bounds.min[0], point.x);
    bounds.min[1] = std::min(bounds.min[1], point.y);
    bounds.min[2] = std::min(bounds.min[2], point.z);
    bounds.max[0] = std::max(bounds.max[0], point.x);
    bounds.max[1] = std::max(bounds.max[1], point.y);
    bounds.max[2] = std::max(bounds.max[2], point.z);
}

void include_primitive_bounds(SceneBounds& bounds,
                              const glm::mat4& world,
                              const stellar::assets::MeshPrimitive& primitive) noexcept {
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                const glm::vec4 corner(
                    x == 0 ? primitive.bounds_min[0] : primitive.bounds_max[0],
                    y == 0 ? primitive.bounds_min[1] : primitive.bounds_max[1],
                    z == 0 ? primitive.bounds_min[2] : primitive.bounds_max[2], 1.0F);
                include_point(bounds, glm::vec3(world * corner));
            }
        }
    }
}

void collect_bounds(const stellar::assets::SceneAsset& scene,
                    std::size_t node_index,
                    const glm::mat4& parent_world,
                    std::vector<bool>& visited,
                    SceneBounds& bounds,
                    bool& has_bounds) noexcept {
    if (node_index >= scene.nodes.size() || visited[node_index]) {
        return;
    }
    visited[node_index] = true;

    const auto& node = scene.nodes[node_index];
    const glm::mat4 world =
        parent_world * to_glm_mat4(stellar::scene::compose_transform(node.local_transform));
    for (const auto& mesh_instance : node.mesh_instances) {
        if (mesh_instance.mesh_index >= scene.meshes.size()) {
            continue;
        }
        for (const auto& primitive : scene.meshes[mesh_instance.mesh_index].primitives) {
            if (!has_bounds) {
                bounds.min = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max()};
                bounds.max = {std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest()};
                has_bounds = true;
            }
            include_primitive_bounds(bounds, world, primitive);
        }
    }

    for (std::size_t child_index : node.children) {
        collect_bounds(scene, child_index, world, visited, bounds, has_bounds);
    }
}

} // namespace

SceneBounds compute_scene_bounds(const stellar::assets::SceneAsset& scene) noexcept {
    SceneBounds bounds;
    bool has_bounds = false;
    std::vector<bool> visited(scene.nodes.size(), false);
    const glm::mat4 identity(1.0F);

    if (scene.default_scene_index.has_value() &&
        *scene.default_scene_index < scene.scenes.size()) {
        for (std::size_t root_node : scene.scenes[*scene.default_scene_index].root_nodes) {
            collect_bounds(scene, root_node, identity, visited, bounds, has_bounds);
        }
    } else if (!scene.scenes.empty()) {
        for (std::size_t root_node : scene.scenes[0].root_nodes) {
            collect_bounds(scene, root_node, identity, visited, bounds, has_bounds);
        }
    }
    for (std::size_t node_index = 0; node_index < scene.nodes.size(); ++node_index) {
        if (!scene.nodes[node_index].parent_index.has_value()) {
            collect_bounds(scene, node_index, identity, visited, bounds, has_bounds);
        }
    }

    if (!has_bounds) {
        return bounds;
    }

    bounds.center = {(bounds.min[0] + bounds.max[0]) * 0.5F,
                     (bounds.min[1] + bounds.max[1]) * 0.5F,
                     (bounds.min[2] + bounds.max[2]) * 0.5F};
    const glm::vec3 extent(bounds.max[0] - bounds.center[0], bounds.max[1] - bounds.center[1],
                           bounds.max[2] - bounds.center[2]);
    bounds.radius = std::max(glm::length(extent), 0.5F);
    return bounds;
}

SceneCameraFit fit_camera_to_bounds(const SceneBounds& bounds,
                                    float vertical_fov_degrees,
                                    float aspect_ratio) noexcept {
    const float safe_aspect =
        std::isfinite(aspect_ratio) && aspect_ratio > 0.0F ? aspect_ratio : 1.0F;
    const float safe_fov = std::isfinite(vertical_fov_degrees) && vertical_fov_degrees > 1.0F
        ? vertical_fov_degrees
        : kDefaultFovDegrees;
    const float vertical_half = glm::radians(safe_fov) * 0.5F;
    const float horizontal_half = std::atan(std::tan(vertical_half) * safe_aspect);
    const float limiting_half = std::max(0.01F, std::min(vertical_half, horizontal_half));
    const float radius = std::max(bounds.radius, 0.5F);
    const float distance = (radius / std::sin(limiting_half)) * 1.15F;

    SceneCameraFit fit;
    fit.target = bounds.center;
    fit.eye = {bounds.center[0], bounds.center[1], bounds.center[2] + distance};
    fit.near_plane = std::max(0.01F, distance - radius * 2.0F);
    fit.far_plane = std::max(fit.near_plane + 1.0F, distance + radius * 2.0F);
    return fit;
}

SceneRenderer::SceneRenderer(std::optional<stellar::assets::SceneAsset> scene) noexcept
    : SceneRenderer(GraphicsBackend::kOpenGL, std::move(scene), {}) {}

SceneRenderer::SceneRenderer(GraphicsBackend backend,
                             std::optional<stellar::assets::SceneAsset> scene,
                             SceneRendererAnimationOptions animation_options) noexcept
    : backend_(backend), animation_options_(std::move(animation_options)),
      source_scene_(std::move(scene)) {}

SceneRenderer::~SceneRenderer() noexcept = default;

std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
SceneRenderer::create_cube_mesh() {
    stellar::assets::MeshAsset mesh;
    mesh.name = "debug_cube";

    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f},
          {-0.5f, 0.5f, 0.5f}}},
        {0.0f, 0.0f, 1.0f}, 0));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f},
          {0.5f, -0.5f, -0.5f}}},
        {0.0f, 0.0f, -1.0f}, 0));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
          {-0.5f, 0.5f, -0.5f}}},
        {-1.0f, 0.0f, 0.0f}, 1));
    mesh.primitives.push_back(make_face_primitive(
        {{{0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f},
          {0.5f, 0.5f, -0.5f}}},
        {1.0f, 0.0f, 0.0f}, 1));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f},
          {-0.5f, 0.5f, 0.5f}}},
        {0.0f, 1.0f, 0.0f}, 2));
    mesh.primitives.push_back(make_face_primitive(
        {{{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f},
          {-0.5f, -0.5f, 0.5f}}},
        {0.0f, -1.0f, 0.0f}, 2));

    return mesh;
}

stellar::assets::SceneAsset SceneRenderer::create_cube_scene() {
    stellar::assets::SceneAsset scene;
    scene.source_uri = "debug:cube";
    scene.meshes.push_back(create_cube_mesh().value());
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "debug_red",
        .base_color_factor = {1.0f, 0.0f, 0.0f, 1.0f},
    });
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "debug_green",
        .base_color_factor = {0.0f, 1.0f, 0.0f, 1.0f},
    });
    scene.materials.push_back(stellar::assets::MaterialAsset{
        .name = "debug_blue",
        .base_color_factor = {0.0f, 0.0f, 1.0f, 1.0f},
    });
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.nodes.push_back(stellar::scene::Node{
        .name = "cube",
        .parent_index = std::nullopt,
        .children = {},
        .local_transform = stellar::scene::Transform{},
        .mesh_instances = {stellar::scene::MeshInstance{0}},
        .skin_index = std::nullopt,
    });
    scene.default_scene_index = 0;
    return scene;
}

std::expected<void, stellar::platform::Error>
SceneRenderer::initialize(stellar::platform::Window& window) {
    using_debug_cube_ = !source_scene_.has_value();
    auto scene = source_scene_.has_value() ? std::move(*source_scene_) : create_cube_scene();
    scene_bounds_ = compute_scene_bounds(scene);
    if (!using_debug_cube_ && !scene.animations.empty()) {
        animation_scene_ = scene;
        animation_player_.emplace();
        animation_player_->bind(*animation_scene_);
        animation_player_->set_looping(animation_options_.loop);
        std::expected<void, stellar::platform::Error> selected = {};
        if (animation_options_.animation_name.has_value()) {
            selected = animation_player_->select_animation(*animation_options_.animation_name);
        } else {
            selected = animation_player_->select_animation(
                animation_options_.animation_index.value_or(0));
        }
        if (!selected) {
            animation_player_.reset();
            animation_scene_.reset();
            return std::unexpected(selected.error());
        }
        animation_player_->play();
    }
    auto device = create_graphics_device(backend_);
    if (!device) {
        return std::unexpected(stellar::platform::Error("Failed to create graphics device"));
    }

    return scene_.initialize(std::move(device), window, std::move(scene));
}

void SceneRenderer::render(float elapsed_seconds,
                           float delta_seconds,
                           int width,
                           int height) noexcept {
    const float aspect =
        height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0F;
    const SceneCameraFit camera = fit_camera_to_bounds(scene_bounds_, kDefaultFovDegrees, aspect);
    const glm::mat4 projection = glm::perspective(
        glm::radians(kDefaultFovDegrees), aspect, camera.near_plane, camera.far_plane);
    const glm::mat4 view = glm::lookAt(glm::vec3(camera.eye[0], camera.eye[1], camera.eye[2]),
                                       glm::vec3(camera.target[0], camera.target[1],
                                                 camera.target[2]),
                                        glm::vec3(0.0f, 1.0f, 0.0f));

    if (using_debug_cube_) {
        const float rotation_degrees = elapsed_seconds * kRotationSpeedDegreesPerSecond;
        if (scene_.node_transform(0).matrix.has_value()) {
            scene_.node_transform(0).matrix = std::nullopt;
        }
        const glm::mat4 model = glm::rotate(
            glm::rotate(glm::mat4(1.0f), glm::radians(rotation_degrees),
                        glm::vec3(0.0f, 1.0f, 0.0f)),
            glm::radians(rotation_degrees), glm::vec3(1.0f, 0.0f, 0.0f));
        scene_.node_transform(0).matrix = to_array(model);
    }

    if (animation_player_.has_value()) {
        const float safe_delta =
            std::isfinite(delta_seconds) && delta_seconds >= 0.0F ? delta_seconds : 0.0F;
        static_cast<void>(animation_player_->update(safe_delta));
        scene_.render(width, height, to_array(projection * view), to_array(view),
                      animation_player_->pose());
        return;
    }

    scene_.render(width, height, to_array(projection * view), to_array(view));
}

} // namespace stellar::graphics
