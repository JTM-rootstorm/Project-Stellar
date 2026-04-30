#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>

#include "stellar/assets/SceneAsset.hpp"
#include "stellar/graphics/BillboardSprite.hpp"
#include "stellar/graphics/GraphicsDevice.hpp"
#include "stellar/scene/AnimationRuntime.hpp"

namespace stellar::graphics {

/**
 * @brief Runtime scene representation backed by uploaded GPU handles.
 *
 * Owns a CPU-side scene snapshot and the GPU resources uploaded from it.
 */
class RenderScene {
public:
    RenderScene() noexcept = default;
    ~RenderScene() noexcept;

    RenderScene(const RenderScene&) = delete;
    RenderScene& operator=(const RenderScene&) = delete;

    /**
     * @brief Initialize the runtime scene.
     * @param device Graphics device that will own the uploaded resources.
     * @param window Platform window used to initialize the device.
     * @param scene CPU-side scene asset to upload.
     * @return std::expected<void, stellar::platform::Error> on failure.
     */
    [[nodiscard]] std::expected<void, stellar::platform::Error>
    initialize(std::unique_ptr<GraphicsDevice> device,
               stellar::platform::Window& window,
               stellar::assets::SceneAsset scene);

    /**
     * @brief Render the active scene.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     * @param view_projection Column-major view-projection matrix.
     * @param view Column-major view matrix used for transparent primitive sorting.
     */
    void render(int width,
                  int height,
                  const std::array<float, 16>& view_projection,
                  const std::array<float, 16>& view) noexcept;

    /**
     * @brief Render the active scene with an evaluated runtime pose.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     * @param view_projection Column-major view-projection matrix.
     * @param view Column-major view matrix used for transparent primitive sorting.
     * @param pose Evaluated node and skin pose to use for animated/skinned draws.
     */
    void render(int width,
                int height,
                const std::array<float, 16>& view_projection,
                const std::array<float, 16>& view,
                const stellar::scene::ScenePose& pose) noexcept;

    /**
     * @brief Render the active scene plus backend-neutral 3D billboard sprites.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     * @param view_projection Column-major view-projection matrix.
     * @param view Column-major view matrix used for transparent primitive sorting.
     * @param billboard_view Camera basis and matrices used to generate sprite quads.
     * @param sprites Sprite draw data to submit after static opaque/mask geometry.
     */
    void render(int width,
                int height,
                const std::array<float, 16>& view_projection,
                const std::array<float, 16>& view,
                const BillboardView& billboard_view,
                std::span<const BillboardSprite> sprites) noexcept;

    /**
     * @brief Render the active scene using view-projection depth as a compatibility fallback.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     * @param view_projection Column-major view-projection matrix.
     */
    void render(int width,
                int height,
                const std::array<float, 16>& view_projection) noexcept;

    /**
     * @brief Access a node transform for animation updates.
     * @param node_index Node index in the stored scene.
     * @return Mutable node transform.
     */
    [[nodiscard]] stellar::scene::Transform& node_transform(std::size_t node_index) noexcept;

    /**
     * @brief Access a node transform for animation updates.
     * @param node_index Node index in the stored scene.
     * @return Const node transform.
     */
    [[nodiscard]] const stellar::scene::Transform& node_transform(
        std::size_t node_index) const noexcept;

private:
    [[nodiscard]] static std::array<float, 16>
    compose_transform(const stellar::scene::Transform& transform) noexcept;
    void collect_node_draws(std::size_t node_index,
                            const std::array<float, 16>& parent_world,
                            const glm::mat4& view_projection,
                            const glm::mat4& view,
                            const stellar::scene::ScenePose* pose,
                            std::vector<struct QueuedMeshDraw>& opaque_draws,
                            std::vector<struct QueuedMeshDraw>& blend_draws) noexcept;
    void render(int width,
                int height,
                const std::array<float, 16>& view_projection,
                const std::array<float, 16>& view,
                const stellar::scene::ScenePose* pose,
                const BillboardView* billboard_view = nullptr,
                std::span<const BillboardSprite> sprites = {}) noexcept;
    void draw_billboard_quads(std::span<const BillboardQuad> quads,
                              const glm::mat4& view_projection) noexcept;
    void destroy() noexcept;

    std::unique_ptr<GraphicsDevice> device_;
    stellar::assets::SceneAsset scene_;
    std::vector<MeshHandle> mesh_handles_;
    std::vector<TextureHandle> texture_handles_;
    std::vector<TextureHandle> owned_texture_handles_;
    std::vector<MaterialHandle> material_handles_;
    std::optional<std::size_t> active_scene_index_;
};

} // namespace stellar::graphics
