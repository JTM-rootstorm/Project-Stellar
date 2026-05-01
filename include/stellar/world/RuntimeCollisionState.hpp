#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "stellar/world/RuntimeWorld.hpp"

namespace stellar::world {

/** @brief Stable runtime reference to one imported static collision mesh. */
struct RuntimeCollisionMeshRef {
    /** @brief Mesh index in the imported LevelCollisionAsset::meshes array. */
    std::size_t mesh_index = 0;

    /** @brief Authored collision mesh name copied from CollisionMesh::name. */
    std::string name;
};

/** @brief Result of applying an authoritative collision-state change. */
struct RuntimeCollisionStateResult {
    /** @brief True when at least one runtime collision mesh state was changed or confirmed. */
    bool applied = false;

    /** @brief Stable machine-readable result code. */
    std::string code;

    /** @brief Human-readable result message for tools and logs. */
    std::string message;
};

/** @brief Authoritative enable/disable overlay for immutable imported static collision meshes. */
class RuntimeCollisionState {
public:
    /** @brief Build enabled state and name lookup from a runtime world's collision asset. */
    [[nodiscard]] static RuntimeCollisionState from_world(const RuntimeWorld& world);

    /** @brief Return true when a collision mesh index should participate in collision queries. */
    [[nodiscard]] bool is_mesh_enabled(std::size_t mesh_index) const noexcept;

    /** @brief Return the authoritative enabled mask indexed by collision mesh index. */
    [[nodiscard]] const std::vector<bool>& enabled_meshes() const noexcept;

    /** @brief Enable or disable all collision meshes with the supplied authored name. */
    [[nodiscard]] RuntimeCollisionStateResult set_mesh_enabled(std::string_view name,
                                                               bool enabled) noexcept;

    /** @brief Return authored collision mesh names and indices in deterministic mesh-index order. */
    [[nodiscard]] std::vector<RuntimeCollisionMeshRef> meshes() const;

    /** @brief Return deterministic diagnostics for duplicate or empty collision mesh names. */
    [[nodiscard]] std::vector<std::string> diagnostics() const;

private:
    std::vector<bool> enabled_meshes_;
    std::vector<RuntimeCollisionMeshRef> mesh_refs_;
};

} // namespace stellar::world
