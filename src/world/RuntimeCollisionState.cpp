#include "stellar/world/RuntimeCollisionState.hpp"

#include <algorithm>
#include <string>

namespace stellar::world {
namespace {

bool name_has_previous_duplicate(const std::vector<RuntimeCollisionMeshRef>& refs,
                                 std::size_t current_index) noexcept {
    const auto& name = refs[current_index].name;
    if (name.empty()) {
        return false;
    }

    for (std::size_t index = 0; index < current_index; ++index) {
        if (refs[index].name == name) {
            return true;
        }
    }
    return false;
}

bool name_has_later_duplicate(const std::vector<RuntimeCollisionMeshRef>& refs,
                              std::size_t current_index) noexcept {
    const auto& name = refs[current_index].name;
    if (name.empty()) {
        return false;
    }

    for (std::size_t index = current_index + 1; index < refs.size(); ++index) {
        if (refs[index].name == name) {
            return true;
        }
    }
    return false;
}

} // namespace

RuntimeCollisionState RuntimeCollisionState::from_world(const RuntimeWorld& world) {
    RuntimeCollisionState state;
    const stellar::assets::LevelCollisionAsset* collision = nullptr;
    if (world.collision_world.has_value()) {
        collision = &world.collision_world->asset();
    } else if (world.scene_asset != nullptr && world.scene_asset->level_collision.has_value()) {
        collision = &world.scene_asset->level_collision.value();
    }

    if (collision == nullptr) {
        return state;
    }

    const auto& meshes = collision->meshes;
    state.enabled_meshes_.assign(meshes.size(), true);
    state.mesh_refs_.reserve(meshes.size());
    for (std::size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
        state.mesh_refs_.push_back(RuntimeCollisionMeshRef{.mesh_index = mesh_index,
                                                           .name = meshes[mesh_index].name});
    }
    return state;
}

bool RuntimeCollisionState::is_mesh_enabled(std::size_t mesh_index) const noexcept {
    if (mesh_index >= enabled_meshes_.size()) {
        return false;
    }
    return enabled_meshes_[mesh_index];
}

const std::vector<bool>& RuntimeCollisionState::enabled_meshes() const noexcept {
    return enabled_meshes_;
}

RuntimeCollisionStateResult RuntimeCollisionState::set_mesh_enabled(std::string_view name,
                                                                    bool enabled) noexcept {
    if (name.empty()) {
        return RuntimeCollisionStateResult{.applied = false,
                                           .code = "empty_name",
                                           .message = "Empty collision mesh names are not targetable"};
    }

    std::size_t match_count = 0;
    for (const auto& mesh_ref : mesh_refs_) {
        if (mesh_ref.name == name && mesh_ref.mesh_index < enabled_meshes_.size()) {
            enabled_meshes_[mesh_ref.mesh_index] = enabled;
            ++match_count;
        }
    }

    if (match_count == 0) {
        return RuntimeCollisionStateResult{.applied = false,
                                           .code = "not_found",
                                           .message = "Collision mesh name was not found"};
    }

    if (match_count > 1) {
        return RuntimeCollisionStateResult{.applied = true,
                                           .code = "duplicate_name",
                                           .message = "Duplicate collision mesh names toggled together"};
    }

    return RuntimeCollisionStateResult{.applied = true,
                                       .code = "ok",
                                       .message = "Collision mesh state updated"};
}

std::vector<RuntimeCollisionMeshRef> RuntimeCollisionState::meshes() const {
    return mesh_refs_;
}

std::vector<std::string> RuntimeCollisionState::diagnostics() const {
    std::vector<std::string> messages;
    for (std::size_t index = 0; index < mesh_refs_.size(); ++index) {
        const auto& mesh_ref = mesh_refs_[index];
        if (mesh_ref.name.empty()) {
            messages.push_back("collision mesh " + std::to_string(mesh_ref.mesh_index) +
                               " has an empty name and cannot be targeted");
            continue;
        }

        if (!name_has_previous_duplicate(mesh_refs_, index) &&
            name_has_later_duplicate(mesh_refs_, index)) {
            messages.push_back("collision mesh name is duplicated and will toggle all matches: " +
                               mesh_ref.name);
        }
    }
    return messages;
}

} // namespace stellar::world
