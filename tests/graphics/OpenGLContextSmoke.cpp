#include <algorithm>
#include <array>
#include <cstdlib>
#include <expected>
#include <iostream>
#include <map>
#include <span>
#include <string_view>
#include <vector>

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "stellar/graphics/GraphicsDevice.hpp"
#include "stellar/platform/Window.hpp"

#define private public
#include "stellar/graphics/opengl/OpenGLGraphicsDevice.hpp"
#undef private

namespace {

constexpr int kSkip = 77;

bool context_tests_enabled() noexcept {
    const char* enabled = std::getenv("STELLAR_RUN_OPENGL_CONTEXT_TESTS");
    return enabled != nullptr && std::string_view(enabled) == "1";
}

bool is_black_clear_pixel(const std::array<unsigned char, 4>& pixel) noexcept {
    return pixel[0] <= 1 && pixel[1] <= 1 && pixel[2] <= 1 && pixel[3] >= 254;
}

class ScopedPackAlignment {
public:
    explicit ScopedPackAlignment(GLint alignment) noexcept {
        glGetIntegerv(GL_PACK_ALIGNMENT, &previous_alignment_);
        glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    }

    ScopedPackAlignment(const ScopedPackAlignment&) = delete;
    ScopedPackAlignment& operator=(const ScopedPackAlignment&) = delete;

    ~ScopedPackAlignment() noexcept {
        glPixelStorei(GL_PACK_ALIGNMENT, previous_alignment_);
    }

private:
    GLint previous_alignment_ = 4;
};

stellar::graphics::TextureUpload make_unaligned_rgb_texture_upload() {
    stellar::graphics::TextureUpload upload;
    upload.image.name = "unaligned-rgb-upload";
    upload.image.width = 3;
    upload.image.height = 2;
    upload.image.format = stellar::assets::ImageFormat::kR8G8B8;
    upload.color_space = stellar::assets::TextureColorSpace::kLinear;
    upload.image.pixels = {
        255, 0,   0,   0,   255, 0,   0,   0,   255,
        255, 255, 255, 128, 128, 128, 32,  64,  96,
    };
    return upload;
}

bool texture_level_zero_matches_upload(
    const stellar::graphics::opengl::OpenGLGraphicsDevice& device,
    stellar::graphics::TextureHandle texture,
    const stellar::graphics::TextureUpload& upload) {
    const auto record = device.textures_.find(texture.value);
    if (record == device.textures_.end()) {
        std::cerr << "Uploaded texture handle was not registered by the OpenGL device\n";
        return false;
    }

    std::array<unsigned char, 18> pixels{};
    glBindTexture(GL_TEXTURE_2D, record->second.texture);
    {
        const ScopedPackAlignment pack_alignment(1);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL texture readback failed with error " << error << '\n';
        return false;
    }
    if (!std::equal(pixels.begin(), pixels.end(), upload.image.pixels.begin(),
                    upload.image.pixels.end())) {
        std::cerr << "Unaligned RGB texture upload readback did not match source pixels\n";
        return false;
    }
    return true;
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

    auto unaligned_rgb_upload = make_unaligned_rgb_texture_upload();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    auto texture = device.create_texture(unaligned_rgb_upload);
    if (!texture) {
        std::cerr << texture.error().message << '\n';
        return 1;
    }

    GLint unpack_alignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
    if (unpack_alignment != 4) {
        std::cerr << "Texture upload did not restore GL_UNPACK_ALIGNMENT: "
                  << unpack_alignment << '\n';
        return 1;
    }
    if (!texture_level_zero_matches_upload(device, *texture, unaligned_rgb_upload)) {
        return 1;
    }
    device.destroy_texture(*texture);

    return 0;
}
