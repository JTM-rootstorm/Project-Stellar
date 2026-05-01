#include "stellar/graphics/LevelRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "stellar/graphics/DebugCubeMesh.hpp"
#include "stellar/graphics/GraphicsDeviceFactory.hpp"

namespace stellar::graphics {

namespace {

constexpr float kDefaultFovDegrees = 45.0F;

float finite_or_zero(float value) noexcept {
  return std::isfinite(value) ? value : 0.0F;
}

std::array<float, 16> to_array(const glm::mat4 &matrix) noexcept {
  std::array<float, 16> result{};
  const float *data = &matrix[0][0];
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index] = data[index];
  }
  return result;
}

glm::mat4 make_projection_for_backend(GraphicsBackend backend,
                                      float vertical_fov_degrees, float aspect,
                                      float near_plane, float far_plane) {
  glm::mat4 projection = glm::perspective(glm::radians(vertical_fov_degrees),
                                          aspect, near_plane, far_plane);

  if (backend == GraphicsBackend::kVulkan) {
    // Convert GLM/OpenGL clip space to Vulkan clip space while keeping the
    // public renderer API backend-neutral.
    glm::mat4 gl_to_vk_clip(1.0F);
    gl_to_vk_clip[1][1] = -1.0F;
    gl_to_vk_clip[2][2] = 0.5F;
    gl_to_vk_clip[3][2] = 0.5F;
    projection = gl_to_vk_clip * projection;
  }

  return projection;
}

void include_point(LevelBounds &bounds, const glm::vec3 &point) noexcept {
  bounds.min[0] = std::min(bounds.min[0], point.x);
  bounds.min[1] = std::min(bounds.min[1], point.y);
  bounds.min[2] = std::min(bounds.min[2], point.z);
  bounds.max[0] = std::max(bounds.max[0], point.x);
  bounds.max[1] = std::max(bounds.max[1], point.y);
  bounds.max[2] = std::max(bounds.max[2], point.z);
}

void include_primitive_bounds(
    LevelBounds &bounds,
    const stellar::assets::MeshPrimitive &primitive) noexcept {
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      for (int z = 0; z < 2; ++z) {
        include_point(
            bounds,
            glm::vec3(
                x == 0 ? primitive.bounds_min[0] : primitive.bounds_max[0],
                y == 0 ? primitive.bounds_min[1] : primitive.bounds_max[1],
                z == 0 ? primitive.bounds_min[2] : primitive.bounds_max[2]));
      }
    }
  }
}

} // namespace

LevelBounds
compute_level_bounds(const stellar::assets::LevelAsset &level) noexcept {
  LevelBounds bounds;
  bool has_bounds = false;
  for (const auto &mesh : level.geometry.meshes) {
    for (const auto &primitive : mesh.primitives) {
      if (!has_bounds) {
        bounds.min = {std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max()};
        bounds.max = {std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest()};
        has_bounds = true;
      }
      include_primitive_bounds(bounds, primitive);
    }
  }

  if (!has_bounds) {
    return bounds;
  }

  bounds.center = {(bounds.min[0] + bounds.max[0]) * 0.5F,
                   (bounds.min[1] + bounds.max[1]) * 0.5F,
                   (bounds.min[2] + bounds.max[2]) * 0.5F};
  const glm::vec3 extent(bounds.max[0] - bounds.center[0],
                         bounds.max[1] - bounds.center[1],
                         bounds.max[2] - bounds.center[2]);
  bounds.radius = std::max(glm::length(extent), 0.5F);
  return bounds;
}

LevelCameraFit fit_camera_to_bounds(const LevelBounds &bounds,
                                     float vertical_fov_degrees,
                                     float aspect_ratio) noexcept {
  const float safe_aspect =
      std::isfinite(aspect_ratio) && aspect_ratio > 0.0F ? aspect_ratio : 1.0F;
  const float safe_fov =
      std::isfinite(vertical_fov_degrees) && vertical_fov_degrees > 1.0F
          ? vertical_fov_degrees
          : kDefaultFovDegrees;
  const float vertical_half = glm::radians(safe_fov) * 0.5F;
  const float horizontal_half =
      std::atan(std::tan(vertical_half) * safe_aspect);
  const float limiting_half =
      std::max(0.01F, std::min(vertical_half, horizontal_half));
  const float radius = std::max(bounds.radius, 0.5F);
  const float distance = (radius / std::sin(limiting_half)) * 1.15F;

  LevelCameraFit fit;
  fit.target = bounds.center;
  fit.eye = {bounds.center[0], bounds.center[1], bounds.center[2] + distance};
  fit.near_plane = std::max(0.01F, distance - radius * 2.0F);
  fit.far_plane = std::max(fit.near_plane + 1.0F, distance + radius * 2.0F);
  return fit;
}

LevelRenderState compute_level_render_state(const LevelRenderView &view,
                                            GraphicsBackend backend,
                                            float aspect_ratio) noexcept {
  const float safe_aspect =
      std::isfinite(aspect_ratio) && aspect_ratio > 0.0F ? aspect_ratio : 1.0F;
  const float safe_fov = std::isfinite(view.vertical_fov_degrees) &&
                                 view.vertical_fov_degrees > 1.0F
                             ? view.vertical_fov_degrees
                             : kDefaultFovDegrees;
  const float safe_near =
      std::isfinite(view.near_plane) && view.near_plane > 0.0F
          ? std::max(view.near_plane, 0.01F)
          : 0.1F;
  const float safe_far =
      std::isfinite(view.far_plane) && view.far_plane > safe_near
          ? view.far_plane
          : safe_near + 1.0F;

  const glm::vec3 eye(finite_or_zero(view.eye[0]),
                      finite_or_zero(view.eye[1]),
                      finite_or_zero(view.eye[2]));
  glm::vec3 target(finite_or_zero(view.target[0]),
                   finite_or_zero(view.target[1]),
                   finite_or_zero(view.target[2]));
  glm::vec3 up(finite_or_zero(view.up[0]), finite_or_zero(view.up[1]),
               finite_or_zero(view.up[2]));
  if (!std::isfinite(target.x) || !std::isfinite(target.y) ||
      !std::isfinite(target.z) || glm::length(target - eye) < 0.0001F) {
    target = eye + glm::vec3(0.0F, 0.0F, -1.0F);
  }
  if (!std::isfinite(up.x) || !std::isfinite(up.y) || !std::isfinite(up.z) ||
      glm::length(up) < 0.0001F) {
    up = glm::vec3(0.0F, 1.0F, 0.0F);
  }

  const glm::mat4 projection = make_projection_for_backend(
      backend, safe_fov, safe_aspect, safe_near, safe_far);
  const glm::mat4 view_matrix = glm::lookAt(eye, target, up);

  LevelRenderState state;
  state.view_projection = to_array(projection * view_matrix);
  state.view = to_array(view_matrix);
  if (view.visibility_culling) {
    state.camera_world_position = {eye.x, eye.y, eye.z};
  }
  return state;
}

LevelRenderer::LevelRenderer(
    std::optional<stellar::assets::LevelAsset> level) noexcept
    : LevelRenderer(GraphicsBackend::kOpenGL, std::move(level)) {}

LevelRenderer::LevelRenderer(
    GraphicsBackend backend,
    std::optional<stellar::assets::LevelAsset> level) noexcept
    : backend_(backend), source_level_(std::move(level)) {}

LevelRenderer::~LevelRenderer() noexcept = default;

std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
LevelRenderer::create_cube_mesh() {
  return create_debug_cube_mesh();
}

stellar::assets::LevelAsset LevelRenderer::create_cube_level() {
  stellar::assets::LevelAsset level;
  level.source_uri = "debug:cube";
  level.geometry.meshes.push_back(create_cube_mesh().value());
  level.geometry.materials.push_back(stellar::assets::LevelSurfaceMaterial{
      .name = "debug_red",
      .source_name = "debug_red",
  });
  level.geometry.materials.push_back(stellar::assets::LevelSurfaceMaterial{
      .name = "debug_green",
      .source_name = "debug_green",
  });
  level.geometry.materials.push_back(stellar::assets::LevelSurfaceMaterial{
      .name = "debug_blue",
      .source_name = "debug_blue",
  });
  for (std::size_t primitive_index = 0;
       primitive_index < level.geometry.meshes[0].primitives.size();
       ++primitive_index) {
    const auto &primitive =
        level.geometry.meshes[0].primitives[primitive_index];
    level.geometry.surfaces.push_back(stellar::assets::LevelSurface{
        .name = "debug_cube_surface",
        .mesh_index = 0,
        .primitive_index = primitive_index,
        .material_index = primitive.material_index,
        .bounds_min = primitive.bounds_min,
        .bounds_max = primitive.bounds_max,
    });
  }
  return level;
}

std::expected<void, stellar::platform::Error>
LevelRenderer::initialize(stellar::platform::Window &window) {
  auto level = source_level_.has_value() ? std::move(*source_level_)
                                         : create_cube_level();
  level_bounds_ = compute_level_bounds(level);

  auto device = create_graphics_device(backend_);
  if (!device) {
    return std::unexpected(
        stellar::platform::Error("Failed to create graphics device"));
  }

  return level_.initialize(std::move(device), window, std::move(level));
}

void LevelRenderer::render(float /*elapsed_seconds*/, float /*delta_seconds*/,
                            int width, int height) noexcept {
  const float aspect =
      height > 0 ? static_cast<float>(width) / static_cast<float>(height)
                  : 1.0F;
  LevelRenderView view;
  if (render_view_.has_value()) {
    view = *render_view_;
  } else {
    const LevelCameraFit camera =
        fit_camera_to_bounds(level_bounds_, kDefaultFovDegrees, aspect);
    view.eye = camera.eye;
    view.target = camera.target;
    view.near_plane = camera.near_plane;
    view.far_plane = camera.far_plane;
    view.visibility_culling = false;
  }

  const LevelRenderState state =
      compute_level_render_state(view, backend_, aspect);
  level_.render(width, height, state.view_projection, state.view,
                state.camera_world_position);
}

void LevelRenderer::set_render_view(LevelRenderView view) noexcept {
  render_view_ = view;
}

void LevelRenderer::clear_render_view() noexcept { render_view_.reset(); }

} // namespace stellar::graphics
