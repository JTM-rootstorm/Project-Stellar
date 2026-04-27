#pragma once

#include <expected>

#include "stellar/platform/Window.hpp"

namespace stellar::graphics {

/**
 * @brief Backend-neutral renderer interface.
 *
 * Renderers own the graphics backend state and translate engine-facing frame
 * submission into backend-specific work.
 */
class Renderer {
public:
    virtual ~Renderer() = default;

    /**
     * @brief Initialize the renderer against an existing platform window.
     * @param window Platform window the renderer should bind to.
     * @return std::expected<void, stellar::platform::Error> on failure.
     */
    [[nodiscard]] virtual std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& window) = 0;

    /**
     * @brief Render a frame.
     * @param elapsed_seconds Monotonic application time in seconds.
     * @param delta_seconds Time elapsed since the previous frame in seconds.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     */
    virtual void render(float elapsed_seconds,
                        float delta_seconds,
                        int width,
                        int height) noexcept = 0;
};

} // namespace stellar::graphics
