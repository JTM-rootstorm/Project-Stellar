#include "stellar/graphics/GraphicsDevice.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <memory>
#include <utility>

namespace stellar::graphics {

namespace {

/**
 * @brief OpenGL 4.5+ implementation of GraphicsDevice.
 *
 * Manages basic framebuffer operations. The Window class owns the
 * OpenGL context and VSync configuration.
 */
class OpenGLDevice : public GraphicsDevice {
public:
    OpenGLDevice() = default;
    ~OpenGLDevice() override { shutdown(); }

    std::expected<void, Error> initialize(void* window_handle) override {
        window_ = static_cast<sf::Window*>(window_handle);
        if (!window_) {
            return std::unexpected<Error>(
                Error{-1, "Invalid window handle"});
        }

        // Set viewport to match drawable size.
        const auto size = window_->getSize();
        glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));

        return {};
    }

    void shutdown() override {
        window_ = nullptr;
    }

    void begin_frame() override {
        // No per-frame OpenGL setup required.
    }

    void end_frame() override {
        // No per-frame OpenGL teardown required.
    }

    void clear(float r, float g, float b, float a) override {
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void present() override {
        window_->display();
    }

private:
    sf::Window* window_ = nullptr;
};

} // anonymous namespace

std::unique_ptr<GraphicsDevice> make_opengl_device() {
    return std::make_unique<OpenGLDevice>();
}

} // namespace stellar::graphics
