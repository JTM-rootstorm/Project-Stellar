#include "ImporterPrivate.hpp"


namespace stellar::import::gltf {

std::expected<stellar::scene::Node, stellar::platform::Error>
load_node(const cgltf_data* data, const cgltf_node& node) {
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

} // namespace stellar::import::gltf
