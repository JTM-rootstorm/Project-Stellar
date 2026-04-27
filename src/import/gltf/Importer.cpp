#include "stellar/import/gltf/Importer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace stellar::import::gltf {

namespace {

class CgltfImporter final : public Importer {
public:
    [[nodiscard]] std::expected<stellar::assets::SceneAsset, stellar::platform::Error>
    load_file(std::string_view path) override {
        return load_scene_from_file(path);
    }

private:
    [[nodiscard]] static std::expected<stellar::assets::SceneAsset, stellar::platform::Error>
    load_scene_from_file(std::string_view path);
    [[nodiscard]] static std::expected<stellar::assets::ImageAsset, stellar::platform::Error>
    load_image_from_file(const std::filesystem::path& path, const char* name);
    [[nodiscard]] static std::expected<stellar::assets::ImageAsset, stellar::platform::Error>
    load_image_from_memory(std::span<const std::uint8_t> bytes, const char* name,
                           std::string source_uri, const char* error_message);
    [[nodiscard]] static std::expected<std::vector<std::uint8_t>, stellar::platform::Error>
    decode_data_uri(std::string_view uri);
    [[nodiscard]] static std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
    load_mesh(const cgltf_data* data, const cgltf_mesh& source_mesh);
    [[nodiscard]] static std::expected<stellar::assets::SkinAsset, stellar::platform::Error>
    load_skin(const cgltf_data* data, const cgltf_skin& source_skin);
    [[nodiscard]] static std::expected<stellar::assets::AnimationAsset, stellar::platform::Error>
    load_animation(const cgltf_data* data, const cgltf_animation& source_animation);
    [[nodiscard]] static std::expected<stellar::assets::MaterialAsset, stellar::platform::Error>
    load_material(const cgltf_data* data, const cgltf_material* material);
    [[nodiscard]] static std::expected<stellar::assets::SamplerAsset, stellar::platform::Error>
    load_sampler(const cgltf_sampler& sampler);
    [[nodiscard]] static std::expected<stellar::assets::TextureAsset, stellar::platform::Error>
    load_texture(const cgltf_data* data, const cgltf_texture& texture);
    [[nodiscard]] static std::expected<stellar::scene::Node, stellar::platform::Error>
    load_node(const cgltf_data* data, const cgltf_node& node);
};

struct CgltfDataDeleter {
    void operator()(cgltf_data* data) const noexcept {
        if (data) {
            cgltf_free(data);
        }
    }
};

using CgltfDataPtr = std::unique_ptr<cgltf_data, CgltfDataDeleter>;

std::string duplicate_to_string(const char* value) {
    return value ? std::string(value) : std::string();
}

std::filesystem::path resolve_relative_path(const std::filesystem::path& base,
                                            const char* relative) {
    if (!relative || !*relative) {
        return {};
    }

    return base.parent_path() / std::filesystem::path(relative);
}

[[nodiscard]] std::expected<std::size_t, stellar::platform::Error>
checked_index(cgltf_size value, const char* error_message) {
    if (value == static_cast<cgltf_size>(-1)) {
        return std::unexpected(stellar::platform::Error(error_message));
    }

    return static_cast<std::size_t>(value);
}

[[nodiscard]] std::unexpected<stellar::platform::Error> import_error(const char* message) {
    return std::unexpected(stellar::platform::Error(message));
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_float_accessor(const cgltf_accessor* accessor, cgltf_type type, cgltf_size expected_count,
                        const char* label) {
    if (!accessor) {
        return import_error(label);
    }
    if (accessor->component_type != cgltf_component_type_r_32f || accessor->type != type) {
        return import_error(label);
    }
    if (accessor->count != expected_count) {
        return import_error(label);
    }

    return {};
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_float_compatible_accessor(const cgltf_accessor* accessor, cgltf_type type,
                                   cgltf_size expected_count, const char* label) {
    if (!accessor) {
        return import_error(label);
    }
    if (accessor->type != type || accessor->count != expected_count) {
        return import_error(label);
    }

    switch (accessor->component_type) {
        case cgltf_component_type_r_8u:
        case cgltf_component_type_r_16u:
        case cgltf_component_type_r_32f:
            return {};
        default:
            return import_error(label);
    }
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_index_accessor(const cgltf_accessor* accessor) {
    if (!accessor || accessor->type != cgltf_type_scalar) {
        return import_error("Invalid index accessor");
    }

    switch (accessor->component_type) {
        case cgltf_component_type_r_8u:
        case cgltf_component_type_r_16u:
        case cgltf_component_type_r_32u:
            return {};
        default:
            return import_error("Invalid index accessor component type");
    }
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_joints_accessor(const cgltf_accessor* accessor, cgltf_size expected_count) {
    if (!accessor || accessor->type != cgltf_type_vec4 || accessor->count != expected_count) {
        return import_error("Invalid JOINTS_0 accessor");
    }

    switch (accessor->component_type) {
        case cgltf_component_type_r_8u:
        case cgltf_component_type_r_16u:
            return {};
        default:
            return import_error("Invalid JOINTS_0 accessor component type");
    }
}

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_weights_accessor(const cgltf_accessor* accessor, cgltf_size expected_count) {
    if (!accessor || accessor->type != cgltf_type_vec4 || accessor->count != expected_count) {
        return import_error("Invalid WEIGHTS_0 accessor");
    }

    switch (accessor->component_type) {
        case cgltf_component_type_r_8u:
        case cgltf_component_type_r_16u:
            if (!accessor->normalized) {
                return import_error("Integer WEIGHTS_0 accessor must be normalized");
            }
            return {};
        case cgltf_component_type_r_32f:
            return {};
        default:
            return import_error("Invalid WEIGHTS_0 accessor component type");
    }
}

[[nodiscard]] std::expected<std::array<float, 2>, stellar::platform::Error>
read_vec2(const cgltf_accessor* accessor, std::size_t index, const char* error_message) {
    cgltf_float value[4] = {};
    if (!cgltf_accessor_read_float(accessor, index, value, 2)) {
        return import_error(error_message);
    }

    return std::array<float, 2>{value[0], value[1]};
}

[[nodiscard]] std::expected<std::array<float, 3>, stellar::platform::Error>
read_vec3(const cgltf_accessor* accessor, std::size_t index, const char* error_message) {
    cgltf_float value[4] = {};
    if (!cgltf_accessor_read_float(accessor, index, value, 3)) {
        return import_error(error_message);
    }

    return std::array<float, 3>{value[0], value[1], value[2]};
}

[[nodiscard]] std::expected<std::array<float, 4>, stellar::platform::Error>
read_vec4(const cgltf_accessor* accessor, std::size_t index, const char* error_message) {
    cgltf_float value[4] = {};
    if (!cgltf_accessor_read_float(accessor, index, value, 4)) {
        return import_error(error_message);
    }

    return std::array<float, 4>{value[0], value[1], value[2], value[3]};
}

[[nodiscard]] std::expected<std::array<std::uint16_t, 4>, stellar::platform::Error>
read_u16_vec4(const cgltf_accessor* accessor, std::size_t index, const char* error_message) {
    cgltf_uint value[4] = {};
    if (!cgltf_accessor_read_uint(accessor, index, value, 4)) {
        return import_error(error_message);
    }

    for (cgltf_uint component : value) {
        if (component > std::numeric_limits<std::uint16_t>::max()) {
            return import_error(error_message);
        }
    }

    return std::array<std::uint16_t, 4>{static_cast<std::uint16_t>(value[0]),
                                       static_cast<std::uint16_t>(value[1]),
                                       static_cast<std::uint16_t>(value[2]),
                                       static_cast<std::uint16_t>(value[3])};
}

[[nodiscard]] std::expected<std::array<float, 16>, stellar::platform::Error>
read_mat4(const cgltf_accessor* accessor, std::size_t index, const char* error_message) {
    cgltf_float value[16] = {};
    if (!cgltf_accessor_read_float(accessor, index, value, 16)) {
        return import_error(error_message);
    }

    std::array<float, 16> result{};
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = value[i];
    }
    return result;
}

bool normalize_weights(std::array<float, 4>& weights) noexcept {
    float sum = 0.0f;
    for (float weight : weights) {
        if (!std::isfinite(weight) || weight < 0.0f) {
            return false;
        }
        sum += weight;
    }

    if (!std::isfinite(sum) || sum <= 0.0f) {
        return false;
    }

    for (float& weight : weights) {
        weight /= sum;
    }
    return true;
}

std::array<float, 16> identity_matrix() noexcept {
    return {1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f};
}

std::array<int, 256> base64_decode_table() {
    std::array<int, 256> table{};
    table.fill(-1);

    for (int c = 'A'; c <= 'Z'; ++c) {
        table[static_cast<std::size_t>(c)] = c - 'A';
    }
    for (int c = 'a'; c <= 'z'; ++c) {
        table[static_cast<std::size_t>(c)] = 26 + (c - 'a');
    }
    for (int c = '0'; c <= '9'; ++c) {
        table[static_cast<std::size_t>(c)] = 52 + (c - '0');
    }
    table[static_cast<std::size_t>('+')] = 62;
    table[static_cast<std::size_t>('/')] = 63;

    return table;
}

std::expected<std::vector<std::uint8_t>, stellar::platform::Error>
decode_base64(std::string_view payload) {
    static const auto kDecodeTable = base64_decode_table();

    std::vector<std::uint8_t> decoded;
    decoded.reserve((payload.size() * 3) / 4);

    int value = 0;
    int bits = -8;
    for (unsigned char ch : payload) {
        if (std::isspace(ch)) {
            continue;
        }

        if (ch == '=') {
            break;
        }

        const int decoded_value = kDecodeTable[ch];
        if (decoded_value < 0) {
            return std::unexpected(stellar::platform::Error("Invalid base64 data URI payload"));
        }

        value = (value << 6) | decoded_value;
        bits += 6;
        if (bits >= 0) {
            decoded.push_back(static_cast<std::uint8_t>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }

    return decoded;
}

std::expected<std::vector<std::uint8_t>, stellar::platform::Error>
decode_percent_encoded(std::string_view payload) {
    std::vector<std::uint8_t> decoded;
    decoded.reserve(payload.size());

    auto hex_value = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return 10 + (ch - 'a');
        }
        if (ch >= 'A' && ch <= 'F') {
            return 10 + (ch - 'A');
        }
        return -1;
    };

    for (std::size_t i = 0; i < payload.size(); ++i) {
        const char ch = payload[i];
        if (ch != '%') {
            decoded.push_back(static_cast<std::uint8_t>(ch));
            continue;
        }

        if (i + 2 >= payload.size()) {
            return std::unexpected(stellar::platform::Error("Invalid percent-encoded data URI payload"));
        }

        const int high = hex_value(payload[i + 1]);
        const int low = hex_value(payload[i + 2]);
        if (high < 0 || low < 0) {
            return std::unexpected(stellar::platform::Error("Invalid percent-encoded data URI payload"));
        }

        decoded.push_back(static_cast<std::uint8_t>((high << 4) | low));
        i += 2;
    }

    return decoded;
}

std::array<float, 16> node_matrix(const cgltf_node& node) {
    std::array<float, 16> matrix{};
    if (node.has_matrix) {
        for (std::size_t i = 0; i < 16; ++i) {
            matrix[i] = node.matrix[i];
        }
    } else {
        matrix = {1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f};
    }
    return matrix;
}

bool is_finite_vec3(const std::array<float, 3>& value) noexcept {
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

std::array<float, 3> subtract(const std::array<float, 3>& lhs,
                              const std::array<float, 3>& rhs) noexcept {
    return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
}

std::array<float, 3> add(const std::array<float, 3>& lhs,
                         const std::array<float, 3>& rhs) noexcept {
    return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
}

std::array<float, 3> multiply(const std::array<float, 3>& value, float scale) noexcept {
    return {value[0] * scale, value[1] * scale, value[2] * scale};
}

float dot(const std::array<float, 3>& lhs, const std::array<float, 3>& rhs) noexcept {
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

std::array<float, 3> cross(const std::array<float, 3>& lhs,
                           const std::array<float, 3>& rhs) noexcept {
    return {lhs[1] * rhs[2] - lhs[2] * rhs[1], lhs[2] * rhs[0] - lhs[0] * rhs[2],
            lhs[0] * rhs[1] - lhs[1] * rhs[0]};
}

float length_squared(const std::array<float, 3>& value) noexcept {
    return dot(value, value);
}

bool normalize_in_place(std::array<float, 3>& value) noexcept {
    const float len_sq = length_squared(value);
    if (!std::isfinite(len_sq) || len_sq <= 0.00000001f) {
        return false;
    }

    const float inv_len = 1.0f / std::sqrt(len_sq);
    value = multiply(value, inv_len);
    return is_finite_vec3(value);
}

bool generate_tangents(std::vector<stellar::assets::StaticVertex>& vertices,
                       const std::vector<std::uint32_t>& indices) noexcept {
    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0) {
        return false;
    }

    std::vector<std::array<float, 3>> tangent_sums(vertices.size(), {0.0f, 0.0f, 0.0f});
    std::vector<std::array<float, 3>> bitangent_sums(vertices.size(), {0.0f, 0.0f, 0.0f});

    for (std::size_t i = 0; i < indices.size(); i += 3) {
        const std::uint32_t i0 = indices[i];
        const std::uint32_t i1 = indices[i + 1];
        const std::uint32_t i2 = indices[i + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            return false;
        }

        const auto edge1 = subtract(vertices[i1].position, vertices[i0].position);
        const auto edge2 = subtract(vertices[i2].position, vertices[i0].position);
        const float du1 = vertices[i1].uv0[0] - vertices[i0].uv0[0];
        const float dv1 = vertices[i1].uv0[1] - vertices[i0].uv0[1];
        const float du2 = vertices[i2].uv0[0] - vertices[i0].uv0[0];
        const float dv2 = vertices[i2].uv0[1] - vertices[i0].uv0[1];
        const float determinant = du1 * dv2 - du2 * dv1;
        if (!std::isfinite(determinant) || std::abs(determinant) <= 0.00000001f) {
            return false;
        }

        const float scale = 1.0f / determinant;
        const auto tangent = multiply(subtract(multiply(edge1, dv2), multiply(edge2, dv1)), scale);
        const auto bitangent = multiply(subtract(multiply(edge2, du1), multiply(edge1, du2)), scale);
        if (!is_finite_vec3(tangent) || !is_finite_vec3(bitangent) ||
            length_squared(tangent) <= 0.00000001f || length_squared(bitangent) <= 0.00000001f) {
            return false;
        }

        tangent_sums[i0] = add(tangent_sums[i0], tangent);
        tangent_sums[i1] = add(tangent_sums[i1], tangent);
        tangent_sums[i2] = add(tangent_sums[i2], tangent);
        bitangent_sums[i0] = add(bitangent_sums[i0], bitangent);
        bitangent_sums[i1] = add(bitangent_sums[i1], bitangent);
        bitangent_sums[i2] = add(bitangent_sums[i2], bitangent);
    }

    for (std::size_t i = 0; i < vertices.size(); ++i) {
        std::array<float, 3> normal = vertices[i].normal;
        if (!normalize_in_place(normal)) {
            return false;
        }

        std::array<float, 3> tangent = subtract(tangent_sums[i], multiply(normal, dot(normal, tangent_sums[i])));
        if (!normalize_in_place(tangent)) {
            return false;
        }

        const auto bitangent_cross = cross(normal, tangent);
        const float handedness = dot(bitangent_cross, bitangent_sums[i]) < 0.0f ? -1.0f : 1.0f;
        vertices[i].tangent = {tangent[0], tangent[1], tangent[2], handedness};
    }

    return true;
}

bool primitive_uses_normal_texture(const cgltf_data* data,
                                   const cgltf_primitive& primitive) noexcept {
    if (!primitive.material || !primitive.material->normal_texture.texture) {
        return false;
    }

    return cgltf_material_index(data, primitive.material) != static_cast<cgltf_size>(-1);
}

} // namespace

std::expected<std::unique_ptr<Importer>, stellar::platform::Error> create_importer() {
    return std::make_unique<CgltfImporter>();
}

std::expected<stellar::assets::ImageAsset, stellar::platform::Error>
CgltfImporter::load_image_from_file(const std::filesystem::path& path, const char* name) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        return std::unexpected(stellar::platform::Error("Failed to decode image file"));
    }

    stellar::assets::ImageAsset image;
    image.name = duplicate_to_string(name);
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.format = stellar::assets::ImageFormat::kR8G8B8A8;
    image.pixels.assign(pixels, pixels + static_cast<std::size_t>(width * height * 4));
    image.source_uri = path.string();

    stbi_image_free(pixels);
    return image;
}

std::expected<stellar::assets::ImageAsset, stellar::platform::Error>
CgltfImporter::load_image_from_memory(std::span<const std::uint8_t> bytes, const char* name,
                                      std::string source_uri, const char* error_message) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
                                            static_cast<int>(bytes.size()), &width, &height,
                                            &channels, 4);
    if (!pixels) {
        return std::unexpected(stellar::platform::Error(error_message));
    }

    stellar::assets::ImageAsset image;
    image.name = duplicate_to_string(name);
    image.width = static_cast<std::uint32_t>(width);
    image.height = static_cast<std::uint32_t>(height);
    image.format = stellar::assets::ImageFormat::kR8G8B8A8;
    image.pixels.assign(pixels, pixels + static_cast<std::size_t>(width * height * 4));
    image.source_uri = std::move(source_uri);

    stbi_image_free(pixels);
    return image;
}

std::expected<std::vector<std::uint8_t>, stellar::platform::Error>
CgltfImporter::decode_data_uri(std::string_view uri) {
    if (!uri.starts_with("data:")) {
        return std::unexpected(stellar::platform::Error("Expected a data URI"));
    }

    const std::size_t comma = uri.find(',');
    if (comma == std::string_view::npos) {
        return std::unexpected(stellar::platform::Error("Invalid data URI"));
    }

    const std::string_view metadata = uri.substr(5, comma - 5);
    const std::string_view payload = uri.substr(comma + 1);
    if (metadata.find(";base64") == std::string_view::npos) {
        return decode_percent_encoded(payload);
    }

    return decode_base64(payload);
}

std::expected<stellar::assets::MaterialAsset, stellar::platform::Error>
CgltfImporter::load_material(const cgltf_data* data, const cgltf_material* material) {
    stellar::assets::MaterialAsset result;
    if (!material) {
        return result;
    }

    result.name = duplicate_to_string(material->name);
    for (std::size_t i = 0; i < 4; ++i) {
        result.base_color_factor[i] = material->pbr_metallic_roughness.base_color_factor[i];
    }
    result.metallic_factor = material->pbr_metallic_roughness.metallic_factor;
    result.roughness_factor = material->pbr_metallic_roughness.roughness_factor;
    result.alpha_cutoff = material->alpha_cutoff;
    result.double_sided = material->double_sided;

    switch (material->alpha_mode) {
        case cgltf_alpha_mode_mask:
            result.alpha_mode = stellar::assets::AlphaMode::kMask;
            break;
        case cgltf_alpha_mode_blend:
            result.alpha_mode = stellar::assets::AlphaMode::kBlend;
            break;
        case cgltf_alpha_mode_opaque:
        default:
            result.alpha_mode = stellar::assets::AlphaMode::kOpaque;
            break;
    }

    auto load_texture_slot = [data](const cgltf_texture_view& view, const char* error_message)
        -> std::expected<stellar::assets::MaterialTextureSlot, stellar::platform::Error> {
        auto index = checked_index(cgltf_texture_index(data, view.texture), error_message);
        if (!index) {
            return std::unexpected(index.error());
        }

        stellar::assets::MaterialTextureSlot slot;
        slot.texture_index = *index;
        slot.texcoord_set = view.texcoord < 0 ? 0u : static_cast<std::uint32_t>(view.texcoord);
        slot.scale = view.scale;
        return slot;
    };

    if (material->has_pbr_metallic_roughness &&
        material->pbr_metallic_roughness.base_color_texture.texture) {
        auto slot = load_texture_slot(material->pbr_metallic_roughness.base_color_texture,
                                      "Failed to resolve base color texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.base_color_texture = *slot;
    }

    if (material->normal_texture.texture) {
        auto slot = load_texture_slot(material->normal_texture,
                                      "Failed to resolve normal texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.normal_texture = *slot;
        result.normal_texture->scale = material->normal_texture.scale;
    }

    if (material->pbr_metallic_roughness.metallic_roughness_texture.texture) {
        auto slot = load_texture_slot(material->pbr_metallic_roughness.metallic_roughness_texture,
                                      "Failed to resolve metallic/roughness texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.metallic_roughness_texture = *slot;
    }

    if (material->occlusion_texture.texture) {
        auto slot = load_texture_slot(material->occlusion_texture,
                                      "Failed to resolve occlusion texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.occlusion_texture = *slot;
        result.occlusion_strength = material->occlusion_texture.scale;
    }

    if (material->emissive_texture.texture) {
        auto slot = load_texture_slot(material->emissive_texture,
                                      "Failed to resolve emissive texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.emissive_texture = *slot;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        result.emissive_factor[i] = material->emissive_factor[i];
    }

    return result;
}

static void classify_texture_color_spaces(stellar::assets::SceneAsset& scene) noexcept {
    for (auto& texture : scene.textures) {
        texture.color_space = stellar::assets::TextureColorSpace::kLinear;
    }

    auto mark_srgb = [&scene](const std::optional<stellar::assets::MaterialTextureSlot>& slot) {
        if (slot.has_value() && slot->texture_index < scene.textures.size()) {
            scene.textures[slot->texture_index].color_space =
                stellar::assets::TextureColorSpace::kSrgb;
        }
    };

    for (const auto& material : scene.materials) {
        mark_srgb(material.base_color_texture);
        mark_srgb(material.emissive_texture);
        // Normal, metallic-roughness, and occlusion textures remain linear non-color data. If a
        // glTF reuses one texture for color and non-color slots, the current one-upload-per-texture
        // asset model cannot assign per-use color spaces; color usage wins until texture views are
        // split during upload in a later renderer phase.
    }
}

static stellar::assets::TextureFilter to_texture_filter(cgltf_filter_type filter) {
    switch (filter) {
        case cgltf_filter_type_nearest:
            return stellar::assets::TextureFilter::kNearest;
        case cgltf_filter_type_linear:
            return stellar::assets::TextureFilter::kLinear;
        case cgltf_filter_type_nearest_mipmap_nearest:
            return stellar::assets::TextureFilter::kNearestMipmapNearest;
        case cgltf_filter_type_linear_mipmap_nearest:
            return stellar::assets::TextureFilter::kLinearMipmapNearest;
        case cgltf_filter_type_nearest_mipmap_linear:
            return stellar::assets::TextureFilter::kNearestMipmapLinear;
        case cgltf_filter_type_linear_mipmap_linear:
            return stellar::assets::TextureFilter::kLinearMipmapLinear;
        case cgltf_filter_type_undefined:
        default:
            return stellar::assets::TextureFilter::kUnspecified;
    }
}

static stellar::assets::TextureWrapMode to_texture_wrap_mode(cgltf_wrap_mode mode) {
    switch (mode) {
        case cgltf_wrap_mode_clamp_to_edge:
            return stellar::assets::TextureWrapMode::kClampToEdge;
        case cgltf_wrap_mode_mirrored_repeat:
            return stellar::assets::TextureWrapMode::kMirroredRepeat;
        case cgltf_wrap_mode_repeat:
        default:
            return stellar::assets::TextureWrapMode::kRepeat;
    }
}

std::expected<stellar::assets::SamplerAsset, stellar::platform::Error>
CgltfImporter::load_sampler(const cgltf_sampler& sampler) {
    stellar::assets::SamplerAsset result;
    result.name = duplicate_to_string(sampler.name);
    result.mag_filter = to_texture_filter(sampler.mag_filter);
    result.min_filter = to_texture_filter(sampler.min_filter);
    result.wrap_s = to_texture_wrap_mode(sampler.wrap_s);
    result.wrap_t = to_texture_wrap_mode(sampler.wrap_t);
    return result;
}

std::expected<stellar::assets::TextureAsset, stellar::platform::Error>
CgltfImporter::load_texture(const cgltf_data* data, const cgltf_texture& texture) {
    stellar::assets::TextureAsset result;
    result.name = duplicate_to_string(texture.name);

    if (texture.image) {
        auto image_index = checked_index(cgltf_image_index(data, texture.image),
                                         "Failed to resolve texture image index");
        if (!image_index) {
            return std::unexpected(image_index.error());
        }
        result.image_index = *image_index;
    }

    if (texture.sampler) {
        auto sampler_index = checked_index(cgltf_sampler_index(data, texture.sampler),
                                           "Failed to resolve texture sampler index");
        if (!sampler_index) {
            return std::unexpected(sampler_index.error());
        }
        result.sampler_index = *sampler_index;
    }

    return result;
}

std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
CgltfImporter::load_mesh(const cgltf_data* data, const cgltf_mesh& source_mesh) {
    stellar::assets::MeshAsset mesh;
    mesh.name = duplicate_to_string(source_mesh.name);

    // Static import policy: fail on data needed to draw the primitive correctly, import common
    // draw-affecting optional attributes with asset/runtime support, and ignore optional features
    // without a runtime representation such as UV2+, morph targets, cameras, lights, and extensions.
    for (cgltf_size p = 0; p < source_mesh.primitives_count; ++p) {
        const cgltf_primitive& primitive = source_mesh.primitives[p];
        if (primitive.type != cgltf_primitive_type_triangles) {
            return std::unexpected(stellar::platform::Error("Unsupported primitive topology"));
        }

        const cgltf_accessor* position_accessor =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
        if (!position_accessor) {
            return std::unexpected(stellar::platform::Error("Primitive is missing positions"));
        }
        if (position_accessor->count == 0) {
            return std::unexpected(stellar::platform::Error("Primitive has no positions"));
        }
        if (auto valid = validate_float_accessor(position_accessor, cgltf_type_vec3,
                                                 position_accessor->count,
                                                 "Invalid POSITION accessor");
            !valid) {
            return std::unexpected(valid.error());
        }

        std::vector<stellar::assets::StaticVertex> vertices(position_accessor->count);
        std::array<float, 3> bounds_min{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        };
        std::array<float, 3> bounds_max{
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
        };

        for (std::size_t i = 0; i < position_accessor->count; ++i) {
            auto position = read_vec3(position_accessor, i, "Failed to read POSITION accessor");
            if (!position) {
                return std::unexpected(position.error());
            }
            vertices[i].position = *position;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                bounds_min[axis] = std::min(bounds_min[axis], (*position)[axis]);
                bounds_max[axis] = std::max(bounds_max[axis], (*position)[axis]);
            }
        }

        if (const auto* normal_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0)) {
            if (auto valid = validate_float_accessor(normal_accessor, cgltf_type_vec3,
                                                     position_accessor->count,
                                                     "Invalid NORMAL accessor");
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                auto normal = read_vec3(normal_accessor, i, "Failed to read NORMAL accessor");
                if (!normal) {
                    return std::unexpected(normal.error());
                }
                vertices[i].normal = *normal;
            }
        }

        if (const auto* texcoord_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0)) {
            if (auto valid = validate_float_accessor(texcoord_accessor, cgltf_type_vec2,
                                                     position_accessor->count,
                                                     "Invalid TEXCOORD_0 accessor");
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                auto texcoord =
                    read_vec2(texcoord_accessor, i, "Failed to read TEXCOORD_0 accessor");
                if (!texcoord) {
                    return std::unexpected(texcoord.error());
                }
                vertices[i].uv0 = *texcoord;
            }
        }

        if (const auto* texcoord_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 1)) {
            if (auto valid = validate_float_compatible_accessor(texcoord_accessor, cgltf_type_vec2,
                                                                position_accessor->count,
                                                                "Invalid TEXCOORD_1 accessor");
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                auto texcoord =
                    read_vec2(texcoord_accessor, i, "Failed to read TEXCOORD_1 accessor");
                if (!texcoord) {
                    return std::unexpected(texcoord.error());
                }
                vertices[i].uv1 = *texcoord;
            }
        }

        bool has_colors = false;
        if (const auto* color_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0)) {
            const bool is_vec3 = color_accessor->type == cgltf_type_vec3;
            const bool is_vec4 = color_accessor->type == cgltf_type_vec4;
            if (!is_vec3 && !is_vec4) {
                return std::unexpected(stellar::platform::Error("Invalid COLOR_0 accessor"));
            }
            if (auto valid = validate_float_compatible_accessor(color_accessor, color_accessor->type,
                                                                position_accessor->count,
                                                                "Invalid COLOR_0 accessor");
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                if (is_vec4) {
                    auto color = read_vec4(color_accessor, i, "Failed to read COLOR_0 accessor");
                    if (!color) {
                        return std::unexpected(color.error());
                    }
                    vertices[i].color = *color;
                } else {
                    auto color = read_vec3(color_accessor, i, "Failed to read COLOR_0 accessor");
                    if (!color) {
                        return std::unexpected(color.error());
                    }
                    vertices[i].color = {(*color)[0], (*color)[1], (*color)[2], 1.0f};
                }
            }
            has_colors = true;
        }

        bool has_skinning = false;
        const auto* joints_accessor =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0);
        const auto* weights_accessor =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0);
        if (joints_accessor || weights_accessor) {
            if (!joints_accessor || !weights_accessor) {
                return std::unexpected(
                    stellar::platform::Error("JOINTS_0 and WEIGHTS_0 must be provided together"));
            }
            if (auto valid = validate_joints_accessor(joints_accessor, position_accessor->count);
                !valid) {
                return std::unexpected(valid.error());
            }
            if (auto valid = validate_weights_accessor(weights_accessor, position_accessor->count);
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                auto joints = read_u16_vec4(joints_accessor, i, "Failed to read JOINTS_0 accessor");
                if (!joints) {
                    return std::unexpected(joints.error());
                }
                auto weights = read_vec4(weights_accessor, i, "Failed to read WEIGHTS_0 accessor");
                if (!weights) {
                    return std::unexpected(weights.error());
                }
                if (!normalize_weights(*weights)) {
                    return std::unexpected(stellar::platform::Error("Invalid WEIGHTS_0 values"));
                }
                vertices[i].joints0 = *joints;
                vertices[i].weights0 = *weights;
            }
            has_skinning = true;
        }

        bool has_tangents = false;
        if (const auto* tangent_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_tangent, 0)) {
            if (auto valid = validate_float_accessor(tangent_accessor, cgltf_type_vec4,
                                                     position_accessor->count,
                                                     "Invalid TANGENT accessor");
                !valid) {
                return std::unexpected(valid.error());
            }
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                auto tangent = read_vec4(tangent_accessor, i, "Failed to read TANGENT accessor");
                if (!tangent) {
                    return std::unexpected(tangent.error());
                }
                vertices[i].tangent = *tangent;
            }
            has_tangents = true;
        }

        std::vector<std::uint32_t> indices;
        if (primitive.indices) {
            if (auto valid = validate_index_accessor(primitive.indices); !valid) {
                return std::unexpected(valid.error());
            }
            indices.resize(primitive.indices->count);
            for (std::size_t i = 0; i < primitive.indices->count; ++i) {
                const auto index = cgltf_accessor_read_index(primitive.indices, i);
                if (index >= vertices.size() ||
                    index > static_cast<cgltf_size>(std::numeric_limits<std::uint32_t>::max())) {
                    return std::unexpected(stellar::platform::Error("Index exceeds vertex count"));
                }
                indices[i] = static_cast<std::uint32_t>(index);
            }
        } else {
            if (vertices.size() > std::numeric_limits<std::uint32_t>::max()) {
                return std::unexpected(stellar::platform::Error("Primitive has too many vertices"));
            }
            indices.reserve(vertices.size());
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                indices.push_back(static_cast<std::uint32_t>(i));
            }
        }

        if (!has_tangents && primitive_uses_normal_texture(data, primitive)) {
            has_tangents = generate_tangents(vertices, indices);
        }

        stellar::assets::MeshPrimitive mesh_primitive;
        mesh_primitive.topology = stellar::assets::PrimitiveTopology::kTriangles;
        mesh_primitive.vertices = std::move(vertices);
        mesh_primitive.indices = std::move(indices);
        mesh_primitive.has_tangents = has_tangents;
        mesh_primitive.has_colors = has_colors;
        mesh_primitive.has_skinning = has_skinning;
        mesh_primitive.bounds_min = bounds_min;
        mesh_primitive.bounds_max = bounds_max;
        if (primitive.material) {
            auto index = checked_index(cgltf_material_index(data, primitive.material),
                                       "Failed to resolve primitive material index");
            if (!index) {
                return std::unexpected(index.error());
            }
            mesh_primitive.material_index = *index;
        }
        mesh.primitives.push_back(std::move(mesh_primitive));
    }

    return mesh;
}

std::expected<stellar::assets::SkinAsset, stellar::platform::Error>
CgltfImporter::load_skin(const cgltf_data* data, const cgltf_skin& source_skin) {
    stellar::assets::SkinAsset skin;
    skin.name = duplicate_to_string(source_skin.name);

    if (source_skin.joints_count == 0) {
        return std::unexpected(stellar::platform::Error("Skin is missing joints"));
    }

    skin.joints.reserve(source_skin.joints_count);
    for (cgltf_size i = 0; i < source_skin.joints_count; ++i) {
        auto joint_index = checked_index(cgltf_node_index(data, source_skin.joints[i]),
                                         "Failed to resolve skin joint index");
        if (!joint_index) {
            return std::unexpected(joint_index.error());
        }
        skin.joints.push_back(*joint_index);
    }

    if (source_skin.skeleton) {
        auto skeleton_index = checked_index(cgltf_node_index(data, source_skin.skeleton),
                                            "Failed to resolve skin skeleton root index");
        if (!skeleton_index) {
            return std::unexpected(skeleton_index.error());
        }
        skin.skeleton_root = *skeleton_index;
    }

    skin.inverse_bind_matrices.reserve(source_skin.joints_count);
    if (source_skin.inverse_bind_matrices) {
        if (auto valid = validate_float_accessor(source_skin.inverse_bind_matrices, cgltf_type_mat4,
                                                 source_skin.joints_count,
                                                 "Invalid inverse bind matrices accessor");
            !valid) {
            return std::unexpected(valid.error());
        }
        for (std::size_t i = 0; i < source_skin.joints_count; ++i) {
            auto matrix = read_mat4(source_skin.inverse_bind_matrices, i,
                                    "Failed to read inverse bind matrices accessor");
            if (!matrix) {
                return std::unexpected(matrix.error());
            }
            skin.inverse_bind_matrices.push_back(*matrix);
        }
    } else {
        skin.inverse_bind_matrices.assign(static_cast<std::size_t>(source_skin.joints_count),
                                          identity_matrix());
    }

    return skin;
}

static std::expected<stellar::assets::AnimationInterpolation, stellar::platform::Error>
to_animation_interpolation(cgltf_interpolation_type interpolation) {
    switch (interpolation) {
        case cgltf_interpolation_type_step:
            return stellar::assets::AnimationInterpolation::kStep;
        case cgltf_interpolation_type_cubic_spline:
            return stellar::assets::AnimationInterpolation::kCubicSpline;
        case cgltf_interpolation_type_linear:
            return stellar::assets::AnimationInterpolation::kLinear;
        default:
            return import_error("Unsupported animation interpolation mode");
    }
}

static std::expected<stellar::assets::AnimationTargetPath, stellar::platform::Error>
to_animation_target_path(cgltf_animation_path_type path) {
    switch (path) {
        case cgltf_animation_path_type_translation:
            return stellar::assets::AnimationTargetPath::kTranslation;
        case cgltf_animation_path_type_rotation:
            return stellar::assets::AnimationTargetPath::kRotation;
        case cgltf_animation_path_type_scale:
            return stellar::assets::AnimationTargetPath::kScale;
        case cgltf_animation_path_type_weights:
            return stellar::assets::AnimationTargetPath::kWeights;
        default:
            return import_error("Unsupported animation target path");
    }
}

std::expected<stellar::assets::AnimationAsset, stellar::platform::Error>
CgltfImporter::load_animation(const cgltf_data* data, const cgltf_animation& source_animation) {
    stellar::assets::AnimationAsset animation;
    animation.name = duplicate_to_string(source_animation.name);

    animation.samplers.reserve(source_animation.samplers_count);
    for (cgltf_size i = 0; i < source_animation.samplers_count; ++i) {
        const cgltf_animation_sampler& source_sampler = source_animation.samplers[i];
        if (!source_sampler.input || source_sampler.input->component_type != cgltf_component_type_r_32f ||
            source_sampler.input->type != cgltf_type_scalar || source_sampler.input->count == 0) {
            return std::unexpected(stellar::platform::Error("Invalid animation sampler input"));
        }
        if (!source_sampler.output || source_sampler.output->count == 0) {
            return std::unexpected(stellar::platform::Error("Invalid animation sampler output"));
        }

        auto interpolation = to_animation_interpolation(source_sampler.interpolation);
        if (!interpolation) {
            return std::unexpected(interpolation.error());
        }

        const std::size_t output_components = cgltf_num_components(source_sampler.output->type);
        const std::size_t expected_outputs =
            source_sampler.input->count *
            (*interpolation == stellar::assets::AnimationInterpolation::kCubicSpline ? 3u : 1u);
        if (output_components == 0 || source_sampler.output->count != expected_outputs) {
            return std::unexpected(stellar::platform::Error("Animation sampler output count mismatch"));
        }
        if (source_sampler.output->component_type != cgltf_component_type_r_32f) {
            return std::unexpected(
                stellar::platform::Error("Animation sampler output must use float components"));
        }

        stellar::assets::AnimationSamplerAsset sampler;
        sampler.interpolation = *interpolation;
        sampler.output_components = output_components;
        sampler.input_times.resize(source_sampler.input->count);
        for (std::size_t key = 0; key < source_sampler.input->count; ++key) {
            cgltf_float time = 0.0f;
            if (!cgltf_accessor_read_float(source_sampler.input, key, &time, 1) ||
                !std::isfinite(time)) {
                return std::unexpected(stellar::platform::Error("Failed to read animation input"));
            }
            sampler.input_times[key] = time;
        }

        sampler.output_values.resize(static_cast<std::size_t>(source_sampler.output->count) *
                                     output_components);
        std::vector<cgltf_float> value(output_components);
        for (std::size_t key = 0; key < source_sampler.output->count; ++key) {
            if (!cgltf_accessor_read_float(source_sampler.output, key, value.data(),
                                           output_components)) {
                return std::unexpected(stellar::platform::Error("Failed to read animation output"));
            }
            for (std::size_t component = 0; component < output_components; ++component) {
                if (!std::isfinite(value[component])) {
                    return std::unexpected(
                        stellar::platform::Error("Invalid animation output value"));
                }
                sampler.output_values[key * output_components + component] = value[component];
            }
        }

        animation.samplers.push_back(std::move(sampler));
    }

    animation.channels.reserve(source_animation.channels_count);
    for (cgltf_size i = 0; i < source_animation.channels_count; ++i) {
        const cgltf_animation_channel& source_channel = source_animation.channels[i];
        auto sampler_index = checked_index(
            cgltf_animation_sampler_index(&source_animation, source_channel.sampler),
            "Failed to resolve animation channel sampler index");
        if (!sampler_index) {
            return std::unexpected(sampler_index.error());
        }
        auto target_path = to_animation_target_path(source_channel.target_path);
        if (!target_path) {
            return std::unexpected(target_path.error());
        }

        stellar::assets::AnimationChannelAsset channel;
        channel.sampler_index = *sampler_index;
        channel.target_path = *target_path;
        if (source_channel.target_node) {
            auto node_index = checked_index(cgltf_node_index(data, source_channel.target_node),
                                            "Failed to resolve animation channel target node");
            if (!node_index) {
                return std::unexpected(node_index.error());
            }
            channel.target_node = *node_index;
        }
        animation.channels.push_back(channel);
    }

    return animation;
}

std::expected<stellar::scene::Node, stellar::platform::Error>
CgltfImporter::load_node(const cgltf_data* data, const cgltf_node& node) {
    stellar::scene::Node result;
    result.name = duplicate_to_string(node.name);
    if (node.has_translation) {
        result.local_transform.translation = {node.translation[0], node.translation[1],
                                              node.translation[2]};
    }
    if (node.has_rotation) {
        result.local_transform.rotation = {node.rotation[0], node.rotation[1], node.rotation[2],
                                           node.rotation[3]};
    }
    if (node.has_scale) {
        result.local_transform.scale = {node.scale[0], node.scale[1], node.scale[2]};
    }
    if (node.has_matrix) {
        result.local_transform.matrix = node_matrix(node);
    }

    if (node.mesh) {
        auto mesh_index = checked_index(cgltf_mesh_index(data, node.mesh),
                                        "Failed to resolve node mesh index");
        if (!mesh_index) {
            return std::unexpected(mesh_index.error());
        }
        result.mesh_instances.push_back(stellar::scene::MeshInstance{.mesh_index = *mesh_index});
    }

    if (node.skin) {
        auto skin_index = checked_index(cgltf_skin_index(data, node.skin),
                                        "Failed to resolve node skin index");
        if (!skin_index) {
            return std::unexpected(skin_index.error());
        }
        result.skin_index = *skin_index;
    }

    result.children.reserve(node.children_count);
    for (cgltf_size i = 0; i < node.children_count; ++i) {
        auto child_index = checked_index(cgltf_node_index(data, node.children[i]),
                                         "Failed to resolve node child index");
        if (!child_index) {
            return std::unexpected(child_index.error());
        }
        result.children.push_back(*child_index);
    }

    return result;
}

std::expected<stellar::assets::SceneAsset, stellar::platform::Error>
CgltfImporter::load_scene_from_file(std::string_view path) {
    const std::string path_string(path);
    cgltf_options options{};
    cgltf_data* raw_data = nullptr;

    if (cgltf_parse_file(&options, path_string.c_str(), &raw_data) != cgltf_result_success) {
        return std::unexpected(stellar::platform::Error("Failed to parse glTF file"));
    }

    CgltfDataPtr data(raw_data);

    if (cgltf_load_buffers(&options, data.get(), path_string.c_str()) != cgltf_result_success) {
        return std::unexpected(stellar::platform::Error("Failed to load glTF buffers"));
    }

    if (cgltf_validate(data.get()) != cgltf_result_success) {
        return std::unexpected(stellar::platform::Error("Invalid glTF scene"));
    }

    stellar::assets::SceneAsset scene;
    scene.source_uri = path_string;
    const std::filesystem::path source_path(path_string);

    scene.images.reserve(data->images_count);
    for (cgltf_size i = 0; i < data->images_count; ++i) {
        const cgltf_image& image = data->images[i];

        if (image.buffer_view) {
            const cgltf_buffer_view* view = image.buffer_view;
            const auto* bytes = static_cast<const std::uint8_t*>(
                view->data ? view->data
                           : (view->buffer ? static_cast<const std::uint8_t*>(view->buffer->data) + view->offset
                                           : nullptr));
            if (!bytes) {
                return std::unexpected(
                    stellar::platform::Error("Embedded image buffer view has no backing data"));
            }

            auto decoded = load_image_from_memory(
                std::span<const std::uint8_t>(bytes, static_cast<std::size_t>(view->size)), image.name,
                image.uri ? std::string(image.uri) : std::string(), "Failed to decode embedded image");
            if (!decoded) {
                return std::unexpected(decoded.error());
            }
            scene.images.push_back(*decoded);
            continue;
        }

        if (image.uri) {
            std::string_view uri(image.uri);
            if (uri.starts_with("data:")) {
                auto payload = decode_data_uri(uri);
                if (!payload) {
                    return std::unexpected(payload.error());
                }

                auto decoded = load_image_from_memory(
                    std::span<const std::uint8_t>(*payload), image.name, std::string(image.uri),
                    "Failed to decode embedded image");
                if (!decoded) {
                    return std::unexpected(decoded.error());
                }
                scene.images.push_back(*decoded);
                continue;
            }

            auto decoded = load_image_from_file(resolve_relative_path(source_path, image.uri), image.name);
            if (!decoded) {
                return std::unexpected(decoded.error());
            }
            scene.images.push_back(*decoded);
            continue;
        }

        return std::unexpected(
            stellar::platform::Error("Image is missing both a uri and a buffer view"));
    }

    scene.samplers.reserve(data->samplers_count);
    for (cgltf_size i = 0; i < data->samplers_count; ++i) {
        auto sampler = load_sampler(data->samplers[i]);
        if (!sampler) {
            return std::unexpected(sampler.error());
        }
        scene.samplers.push_back(*sampler);
    }

    scene.textures.reserve(data->textures_count);
    for (cgltf_size i = 0; i < data->textures_count; ++i) {
        auto texture = load_texture(data.get(), data->textures[i]);
        if (!texture) {
            return std::unexpected(texture.error());
        }
        scene.textures.push_back(*texture);
    }

    scene.materials.reserve(data->materials_count);
    for (cgltf_size i = 0; i < data->materials_count; ++i) {
        auto material = load_material(data.get(), &data->materials[i]);
        if (!material) {
            return std::unexpected(material.error());
        }
        scene.materials.push_back(*material);
    }

    classify_texture_color_spaces(scene);

    scene.meshes.reserve(data->meshes_count);
    for (cgltf_size i = 0; i < data->meshes_count; ++i) {
        auto mesh = load_mesh(data.get(), data->meshes[i]);
        if (!mesh) {
            return std::unexpected(mesh.error());
        }
        scene.meshes.push_back(*mesh);
    }

    scene.nodes.reserve(data->nodes_count);
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        auto node = load_node(data.get(), data->nodes[i]);
        if (!node) {
            return std::unexpected(node.error());
        }
        scene.nodes.push_back(*node);
    }

    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        if (data->nodes[i].parent) {
            auto parent_index = checked_index(cgltf_node_index(data.get(), data->nodes[i].parent),
                                              "Failed to resolve node parent index");
            if (!parent_index) {
                return std::unexpected(parent_index.error());
            }
            scene.nodes[i].parent_index = *parent_index;
        }
    }

    scene.skins.reserve(data->skins_count);
    for (cgltf_size i = 0; i < data->skins_count; ++i) {
        auto skin = load_skin(data.get(), data->skins[i]);
        if (!skin) {
            return std::unexpected(skin.error());
        }
        scene.skins.push_back(*skin);
    }

    scene.animations.reserve(data->animations_count);
    for (cgltf_size i = 0; i < data->animations_count; ++i) {
        auto animation = load_animation(data.get(), data->animations[i]);
        if (!animation) {
            return std::unexpected(animation.error());
        }
        scene.animations.push_back(*animation);
    }

    scene.scenes.reserve(data->scenes_count);
    for (cgltf_size i = 0; i < data->scenes_count; ++i) {
        const cgltf_scene& source_scene = data->scenes[i];
        stellar::scene::Scene out_scene;
        out_scene.name = duplicate_to_string(source_scene.name);
        for (cgltf_size n = 0; n < source_scene.nodes_count; ++n) {
            auto node_index = checked_index(cgltf_node_index(data.get(), source_scene.nodes[n]),
                                            "Failed to resolve scene root node index");
            if (!node_index) {
                return std::unexpected(node_index.error());
            }
            out_scene.root_nodes.push_back(*node_index);
        }
        scene.scenes.push_back(std::move(out_scene));
    }

    if (data->scene) {
        auto default_scene_index = checked_index(cgltf_scene_index(data.get(), data->scene),
                                                 "Failed to resolve default scene index");
        if (!default_scene_index) {
            return std::unexpected(default_scene_index.error());
        }
        scene.default_scene_index = *default_scene_index;
    }

    return scene;
}

} // namespace stellar::import::gltf
