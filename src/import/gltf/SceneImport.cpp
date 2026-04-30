#include "ImporterPrivate.hpp"

#include <filesystem>

namespace stellar::import::gltf {

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_skinned_mesh_joint_indices(const stellar::assets::SceneAsset& scene) {
    for (const auto& node : scene.nodes) {
        if (!node.skin_index.has_value()) {
            continue;
        }
        if (*node.skin_index >= scene.skins.size()) {
            return import_error("Node skin index exceeds skin count");
        }

        const std::size_t palette_size = scene.skins[*node.skin_index].joints.size();
        for (const auto& instance : node.mesh_instances) {
            if (instance.mesh_index >= scene.meshes.size()) {
                return import_error("Node mesh index exceeds mesh count");
            }

            const auto& mesh = scene.meshes[instance.mesh_index];
            for (const auto& primitive : mesh.primitives) {
                if (!primitive.has_skinning) {
                    continue;
                }

                for (const auto& vertex : primitive.vertices) {
                    for (std::uint16_t joint : vertex.joints0) {
                        if (joint >= palette_size) {
                            return import_error("JOINTS_0 index exceeds bound skin joint palette");
                        }
                    }
                }
            }
        }
    }

    return {};
}

std::expected<stellar::assets::SceneAsset, stellar::platform::Error>
load_scene_from_file(std::string_view path) {
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

    if (auto required_extensions = validate_required_extensions(data.get()); !required_extensions) {
        return std::unexpected(required_extensions.error());
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

    if (auto valid = validate_skinned_mesh_joint_indices(scene); !valid) {
        return std::unexpected(valid.error());
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

    auto collision = extract_level_collision(scene);
    if (!collision) {
        return std::unexpected(collision.error());
    }
    if (!collision->meshes.empty()) {
        scene.level_collision = std::move(*collision);
    }

    auto world_metadata = extract_world_metadata(scene, data.get());
    if (!world_metadata) {
        return std::unexpected(world_metadata.error());
    }
    scene.world_metadata = std::move(*world_metadata);

    return scene;
}

} // namespace stellar::import::gltf
