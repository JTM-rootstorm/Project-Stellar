#include "stellar/import/gltf/Importer.hpp"

#include <array>
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
    [[nodiscard]] static std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
    load_mesh(const cgltf_data* data, const cgltf_mesh& source_mesh);
    [[nodiscard]] static std::expected<stellar::assets::MaterialAsset, stellar::platform::Error>
    load_material(const cgltf_data* data, const cgltf_material* material);
    [[nodiscard]] static stellar::scene::Node load_node(const cgltf_data* data,
                                                       const cgltf_node& node);
};

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

std::optional<std::size_t> to_index(std::size_t value) {
    return value == static_cast<std::size_t>(-1) ? std::nullopt : std::optional<std::size_t>(value);
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

    if (material->has_pbr_metallic_roughness &&
        material->pbr_metallic_roughness.base_color_texture.texture) {
        result.base_color_texture_index = static_cast<std::size_t>(
            cgltf_texture_index(data, material->pbr_metallic_roughness.base_color_texture.texture));
    }

    if (material->normal_texture.texture) {
        result.normal_texture_index = static_cast<std::size_t>(
            cgltf_texture_index(data, material->normal_texture.texture));
    }

    if (material->pbr_metallic_roughness.metallic_roughness_texture.texture) {
        result.metallic_roughness_texture_index = static_cast<std::size_t>(
            cgltf_texture_index(data,
                                material->pbr_metallic_roughness.metallic_roughness_texture.texture));
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

        if (const auto* normal_accessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0)) {
            for (std::size_t i = 0; i < position_accessor->count; ++i) {
                cgltf_float value[4] = {};
                if (cgltf_accessor_read_float(normal_accessor, i, value, 3)) {
                    vertices[i].normal = {value[0], value[1], value[2]};
                }
            }
        }

        if (const auto* texcoord_accessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0)) {
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
                indices[i] = static_cast<std::uint32_t>(cgltf_accessor_read_index(primitive.indices, i));
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
        mesh_primitive.material_index = primitive.material
                                            ? to_index(cgltf_material_index(data, primitive.material))
                                            : std::nullopt;
        mesh.primitives.push_back(std::move(mesh_primitive));
    }

    return mesh;
}

stellar::scene::Node CgltfImporter::load_node(const cgltf_data* data, const cgltf_node& node) {
    stellar::scene::Node result;
    result.name = duplicate_to_string(node.name);
    result.local_transform.translation = {node.translation[0], node.translation[1], node.translation[2]};
    result.local_transform.rotation = {node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]};
    result.local_transform.scale = {node.scale[0], node.scale[1], node.scale[2]};
    if (node.has_matrix) {
        result.local_transform.matrix = node_matrix(node);
    }

    if (node.mesh) {
        result.mesh_instances.push_back(stellar::scene::MeshInstance{
            .mesh_index = static_cast<std::size_t>(cgltf_mesh_index(data, node.mesh))});
    }

    result.children.reserve(node.children_count);
    for (cgltf_size i = 0; i < node.children_count; ++i) {
        result.children.push_back(static_cast<std::size_t>(cgltf_node_index(data, node.children[i])));
    }

    return result;
}

std::expected<stellar::assets::SceneAsset, stellar::platform::Error>
CgltfImporter::load_scene_from_file(std::string_view path) {
    const std::string path_string(path);
    cgltf_options options{};
    cgltf_data* data = nullptr;

    if (cgltf_parse_file(&options, path_string.c_str(), &data) != cgltf_result_success) {
        return std::unexpected(stellar::platform::Error("Failed to parse glTF file"));
    }

    const auto cleanup = [&]() {
        if (data) {
            cgltf_free(data);
            data = nullptr;
        }
    };

    if (cgltf_load_buffers(&options, data, path_string.c_str()) != cgltf_result_success) {
        cleanup();
        return std::unexpected(stellar::platform::Error("Failed to load glTF buffers"));
    }

    if (cgltf_validate(data) != cgltf_result_success) {
        cleanup();
        return std::unexpected(stellar::platform::Error("Invalid glTF scene"));
    }

    stellar::assets::SceneAsset scene;
    scene.source_uri = path_string;
    const std::filesystem::path source_path(path_string);

    scene.materials.reserve(data->materials_count);
    for (cgltf_size i = 0; i < data->materials_count; ++i) {
        auto material = load_material(data, &data->materials[i]);
        if (!material) {
            cleanup();
            return std::unexpected(material.error());
        }
        scene.materials.push_back(*material);
    }

    scene.meshes.reserve(data->meshes_count);
    for (cgltf_size i = 0; i < data->meshes_count; ++i) {
        auto mesh = load_mesh(data, data->meshes[i]);
        if (!mesh) {
            cleanup();
            return std::unexpected(mesh.error());
        }
        scene.meshes.push_back(*mesh);
    }

    scene.images.reserve(data->images_count);
    for (cgltf_size i = 0; i < data->images_count; ++i) {
        const cgltf_image& image = data->images[i];
        if (!image.uri) {
            cleanup();
            return std::unexpected(stellar::platform::Error("Only external images are supported in this importer pass"));
        }

        auto decoded = load_image_from_file(resolve_relative_path(source_path, image.uri), image.name);
        if (!decoded) {
            cleanup();
            return std::unexpected(decoded.error());
        }
        scene.images.push_back(*decoded);
    }

    scene.nodes.reserve(data->nodes_count);
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        scene.nodes.push_back(load_node(data, data->nodes[i]));
    }

    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        if (data->nodes[i].parent) {
            scene.nodes[i].parent_index = static_cast<std::size_t>(cgltf_node_index(data, data->nodes[i].parent));
        }
    }

    scene.scenes.reserve(data->scenes_count);
    for (cgltf_size i = 0; i < data->scenes_count; ++i) {
        const cgltf_scene& source_scene = data->scenes[i];
        stellar::scene::Scene out_scene;
        out_scene.name = duplicate_to_string(source_scene.name);
        for (cgltf_size n = 0; n < source_scene.nodes_count; ++n) {
            out_scene.root_nodes.push_back(static_cast<std::size_t>(cgltf_node_index(data, source_scene.nodes[n])));
        }
        scene.scenes.push_back(std::move(out_scene));
    }

    if (data->scene) {
        scene.default_scene_index = static_cast<std::size_t>(cgltf_scene_index(data, data->scene));
    }

    cleanup();
    return scene;
}

} // namespace stellar::import::gltf
