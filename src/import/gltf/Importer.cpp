#include "stellar/import/gltf/Importer.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
    }

    if (material->pbr_metallic_roughness.metallic_roughness_texture.texture) {
        auto slot = load_texture_slot(material->pbr_metallic_roughness.metallic_roughness_texture,
                                      "Failed to resolve metallic/roughness texture index");
        if (!slot) {
            return std::unexpected(slot.error());
        }
        result.metallic_roughness_texture = *slot;
    }

    return result;
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

        std::vector<stellar::assets::StaticVertex> vertices(position_accessor->count);
        for (std::size_t i = 0; i < position_accessor->count; ++i) {
            cgltf_float value[4] = {};
            if (cgltf_accessor_read_float(position_accessor, i, value, 3)) {
                vertices[i].position = {value[0], value[1], value[2]};
            }
        }

        if (const auto* normal_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0)) {
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                cgltf_float value[4] = {};
                if (cgltf_accessor_read_float(normal_accessor, i, value, 3)) {
                    vertices[i].normal = {value[0], value[1], value[2]};
                }
            }
        }

        if (const auto* texcoord_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0)) {
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                cgltf_float value[4] = {};
                if (cgltf_accessor_read_float(texcoord_accessor, i, value, 2)) {
                    vertices[i].uv0 = {value[0], value[1]};
                }
            }
        }

        std::vector<std::uint32_t> indices;
        if (primitive.indices) {
            indices.resize(primitive.indices->count);
            for (std::size_t i = 0; i < primitive.indices->count; ++i) {
                const auto index = cgltf_accessor_read_index(primitive.indices, i);
                indices[i] = static_cast<std::uint32_t>(index);
            }
        } else {
            indices.reserve(vertices.size());
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                indices.push_back(static_cast<std::uint32_t>(i));
            }
        }

        stellar::assets::MeshPrimitive mesh_primitive;
        mesh_primitive.topology = stellar::assets::PrimitiveTopology::kTriangles;
        mesh_primitive.vertices = std::move(vertices);
        mesh_primitive.indices = std::move(indices);
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
