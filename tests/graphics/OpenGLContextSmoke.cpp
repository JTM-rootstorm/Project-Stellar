#include "stellar/graphics/opengl/OpenGLGraphicsDevice.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "stellar/platform/Window.hpp"

namespace {

constexpr int kSkip = 77;

bool context_tests_enabled() noexcept {
    const char* enabled = std::getenv("STELLAR_RUN_OPENGL_CONTEXT_TESTS");
    return enabled != nullptr && std::string_view(enabled) == "1";
}

bool is_black_clear_pixel(const std::array<unsigned char, 4>& pixel) noexcept {
    return pixel[0] <= 1 && pixel[1] <= 1 && pixel[2] <= 1 && pixel[3] >= 254;
}

} // namespace

int main() {
    if (!context_tests_enabled()) {
        std::cout << "Skipping OpenGL context smoke; set STELLAR_RUN_OPENGL_CONTEXT_TESTS=1\n";
        return kSkip;
    }

    stellar::platform::Window window;
    auto window_result = window.create(16, 16, "stellar-opengl-smoke",
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window_result) {
        std::cerr << window_result.error().message << '\n';
        return 1;
    }

    stellar::graphics::opengl::OpenGLGraphicsDevice device;
    auto init_result = device.initialize(window);
    if (!init_result) {
        std::cerr << init_result.error().message << '\n';
        return 1;
    }

    device.begin_frame(16, 16);
    std::array<unsigned char, 4> pixel{};
    glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL readback failed with error " << error << '\n';
        return 1;
    }
    if (!is_black_clear_pixel(pixel)) {
        std::cerr << "Unexpected clear pixel: " << static_cast<int>(pixel[0]) << ','
                  << static_cast<int>(pixel[1]) << ',' << static_cast<int>(pixel[2]) << ','
                  << static_cast<int>(pixel[3]) << '\n';
        return 1;
    }

    return 0;
}
