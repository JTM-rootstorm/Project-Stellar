#include "stellar/graphics/Texture.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

namespace stellar::graphics {

Texture2D::Texture2D(unsigned int id) : texture_id_(id) {}

Texture2D::~Texture2D() {
    if (texture_id_ != 0) {
        glDeleteTextures(1, &texture_id_);
    }
}

Texture2D::Texture2D(Texture2D&& other) noexcept
    : texture_id_(other.texture_id_) {
    other.texture_id_ = 0;
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this != &other) {
        if (texture_id_ != 0) {
            glDeleteTextures(1, &texture_id_);
        }
        texture_id_ = other.texture_id_;
        other.texture_id_ = 0;
    }
    return *this;
}

void Texture2D::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
}

std::expected<Texture2D, Error> create_texture_from_data(
    int width, int height, const uint8_t* data) {
    unsigned int id = 0;
    glGenTextures(1, &id);
    if (id == 0) {
        return std::unexpected<Error>(
            Error{-5, "Failed to generate texture"});
    }

    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    return Texture2D(id);
}

} // namespace stellar::graphics
