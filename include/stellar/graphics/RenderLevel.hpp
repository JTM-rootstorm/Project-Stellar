#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>

#include "stellar/assets/LevelAsset.hpp"
#include "stellar/graphics/BillboardSprite.hpp"
#include "stellar/graphics/GraphicsDevice.hpp"

namespace stellar::graphics {

/**
 * @brief Complete public request for rendering one level presentation frame.
 */
struct RenderLevelFrame {
  /** @brief Viewport width in pixels. */
  int width = 0;

  /** @brief Viewport height in pixels. */
  int height = 0;

  /** @brief Column-major view-projection matrix. */
  std::array<float, 16> view_projection{};

  /** @brief Column-major view matrix used for transparent primitive sorting. */
  std::array<float, 16> view{};

  /** @brief Optional camera position used for static level visibility culling. */
  std::optional<std::array<float, 3>> camera_world_position;

  /** @brief Optional billboard camera basis used to generate sprite quads. */
  const BillboardView *billboard_view = nullptr;

  /** @brief Backend-neutral 3D billboard sprites submitted after static geometry. */
  std::span<const BillboardSprite> sprites{};
};

/**
 * @brief Runtime level representation backed by uploaded GPU handles.
 *
 * Owns a CPU-side LevelAsset snapshot and GPU resources uploaded through the
 * shared GraphicsDevice abstraction. The renderer treats level geometry as
 * static world-space primitives and does not consume scene graph poses,
 * skinning palettes, animations, or PBR material extensions.
 */
class RenderLevel {
public:
  /** @brief Construct an empty runtime level with no uploaded graphics resources. */
  RenderLevel() noexcept = default;

  /** @brief Release GPU handles owned by this runtime level. */
  ~RenderLevel() noexcept;

  /** @brief Runtime levels own GPU handles and cannot be copied. */
  RenderLevel(const RenderLevel &) = delete;

  /** @brief Runtime levels own GPU handles and cannot be copy-assigned. */
  RenderLevel &operator=(const RenderLevel &) = delete;

  /**
   * @brief Initialize the runtime level.
   * @param device Graphics device that will own the uploaded resources.
   * @param window Platform window used to initialize the device.
   * @param level CPU-side level asset to upload.
   * @return std::expected<void, stellar::platform::Error> on failure.
   */
  [[nodiscard]] std::expected<void, stellar::platform::Error>
  initialize(std::unique_ptr<GraphicsDevice> device,
             stellar::platform::Window &window,
             stellar::assets::LevelAsset level);

  /**
   * @brief Render a level frame from the canonical public request payload.
   * @param frame Viewport, camera, visibility, and optional billboard draw data.
   */
  void render(const RenderLevelFrame &frame) noexcept;

  /**
   * @brief Render static level geometry.
   * @param width Viewport width in pixels.
   * @param height Viewport height in pixels.
   * @param view_projection Column-major view-projection matrix.
   * @param view Column-major view matrix used for transparent primitive
   * sorting.
   */
  void render(int width, int height,
               const std::array<float, 16> &view_projection,
               const std::array<float, 16> &view) noexcept;

  /**
   * @brief Render static level geometry with optional visibility culling from
   * a camera point.
   * @param width Viewport width in pixels.
   * @param height Viewport height in pixels.
   * @param view_projection Column-major view-projection matrix.
   * @param view Column-major view matrix used for transparent primitive
   * sorting.
   * @param camera_world_position Optional camera position used for static level
   * visibility culling.
   */
  void render(int width, int height,
              const std::array<float, 16> &view_projection,
              const std::array<float, 16> &view,
              std::optional<std::array<float, 3>> camera_world_position)
      noexcept;

  /**
   * @brief Render static level geometry plus backend-neutral 3D billboard
   * sprites.
   * @param width Viewport width in pixels.
   * @param height Viewport height in pixels.
   * @param view_projection Column-major view-projection matrix.
   * @param view Column-major view matrix used for transparent primitive
   * sorting.
   * @param billboard_view Camera basis and matrices used to generate sprite
   * quads.
   * @param sprites Sprite draw data to submit after static opaque/mask
   * geometry.
   */
  void render(int width, int height,
              const std::array<float, 16> &view_projection,
              const std::array<float, 16> &view,
              const BillboardView &billboard_view,
              std::span<const BillboardSprite> sprites) noexcept;

  /**
   * @brief Render static level geometry with optional visibility culling, then
   * billboards.
   * @param width Viewport width in pixels.
   * @param height Viewport height in pixels.
   * @param view_projection Column-major view-projection matrix.
   * @param view Column-major view matrix used for transparent primitive
   * sorting.
   * @param camera_world_position Optional camera position used for static level
   * visibility culling.
   * @param billboard_view Camera basis and matrices used to generate sprite
   * quads.
   * @param sprites Sprite draw data to submit after static opaque/mask
   * geometry.
   */
  void render(int width, int height,
              const std::array<float, 16> &view_projection,
              const std::array<float, 16> &view,
              std::optional<std::array<float, 3>> camera_world_position,
              const BillboardView &billboard_view,
              std::span<const BillboardSprite> sprites) noexcept;

  /**
   * @brief Render static level geometry using view-projection depth as a
   * compatibility fallback.
   * @param width Viewport width in pixels.
   * @param height Viewport height in pixels.
   * @param view_projection Column-major view-projection matrix.
   */
  void render(int width, int height,
              const std::array<float, 16> &view_projection) noexcept;

private:
  struct StaticDrawQueueStats {
    bool visibility_used = false;
    std::size_t visibility_visible_count = 0;
  };

  [[nodiscard]] StaticDrawQueueStats
  queue_static_draws(const glm::mat4 &view_projection, const glm::mat4 &view,
                     std::optional<std::array<float, 3>> camera_world_position,
                     std::vector<struct QueuedLevelDraw> &opaque_draws,
                     std::vector<struct QueuedLevelDraw> &blend_draws) noexcept;
  void draw_billboard_quads(std::span<const BillboardQuad> quads,
                            const glm::mat4 &view_projection) noexcept;
  void destroy() noexcept;

  std::unique_ptr<GraphicsDevice> device_;
  stellar::assets::LevelAsset level_;
  std::array<float, 256> lightstyle_multipliers_{};
  std::vector<MeshHandle> mesh_handles_;
  std::vector<TextureHandle> texture_handles_;
  std::vector<TextureHandle> lightmap_texture_handles_;
  TextureHandle black_lightmap_texture_;
  std::vector<TextureHandle> owned_texture_handles_;
  std::vector<MaterialHandle> material_handles_;
  MaterialHandle default_material_;
  bool debug_render_enabled_ = false;
  std::size_t debug_render_frame_limit_ = 0;
  std::size_t debug_render_frame_index_ = 0;
  bool disable_lightmaps_ = false;
  bool debug_lightmaps_enabled_ = false;
  bool force_lightmap_visualization_ = false;
};

} // namespace stellar::graphics
