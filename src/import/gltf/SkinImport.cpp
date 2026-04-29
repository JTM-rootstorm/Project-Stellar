#include "ImporterPrivate.hpp"


namespace stellar::import::gltf {

std::expected<stellar::assets::SkinAsset, stellar::platform::Error>
load_skin(const cgltf_data* data, const cgltf_skin& source_skin) {
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

} // namespace stellar::import::gltf
