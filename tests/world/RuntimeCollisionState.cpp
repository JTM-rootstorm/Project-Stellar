#include "stellar/world/RuntimeCollisionState.hpp"

#include <cassert>
#include <string>
#include <string_view>

namespace {

stellar::assets::CollisionTriangle make_triangle() {
    return stellar::assets::CollisionTriangle{.a = {0.0F, 0.0F, 0.0F},
                                              .b = {1.0F, 0.0F, 0.0F},
                                              .c = {0.0F, 1.0F, 0.0F},
                                              .normal = {0.0F, 0.0F, 1.0F}};
}

stellar::assets::CollisionMesh make_mesh(std::string_view name) {
    stellar::assets::CollisionMesh mesh;
    mesh.name = std::string{name};
    mesh.triangles = {make_triangle()};
    mesh.bounds_min = {0.0F, 0.0F, 0.0F};
    mesh.bounds_max = {1.0F, 1.0F, 0.0F};
    return mesh;
}

stellar::world::RuntimeWorld make_world(std::initializer_list<stellar::assets::CollisionMesh> meshes,
                                        stellar::assets::LevelAsset& scene) {
    scene.level_collision = stellar::assets::LevelCollisionAsset{};
    scene.level_collision->meshes.assign(meshes.begin(), meshes.end());
    return stellar::world::build_runtime_world(scene);
}

void empty_world_has_empty_collision_state() {
    const stellar::assets::LevelAsset scene;
    const auto world = stellar::world::build_runtime_world(scene);
    auto state = stellar::world::RuntimeCollisionState::from_world(world);

    assert(state.meshes().empty());
    assert(state.diagnostics().empty());
    assert(!state.is_mesh_enabled(0));
    const auto result = state.set_mesh_enabled("COL_floor", false);
    assert(!result.applied);
    assert(result.code == "not_found");
}

void all_collision_meshes_enabled_by_default() {
    stellar::assets::LevelAsset scene;
    const auto world = make_world({make_mesh("COL_floor"), make_mesh("COL_wall")}, scene);
    const auto state = stellar::world::RuntimeCollisionState::from_world(world);

    assert(state.is_mesh_enabled(0));
    assert(state.is_mesh_enabled(1));
    assert(!state.is_mesh_enabled(2));
}

void set_mesh_enabled_disables_named_mesh() {
    stellar::assets::LevelAsset scene;
    const auto world = make_world({make_mesh("COL_floor"), make_mesh("COL_wall")}, scene);
    auto state = stellar::world::RuntimeCollisionState::from_world(world);

    const auto result = state.set_mesh_enabled("COL_wall", false);
    assert(result.applied);
    assert(result.code == "ok");
    assert(state.is_mesh_enabled(0));
    assert(!state.is_mesh_enabled(1));
}

void set_mesh_enabled_enables_named_mesh() {
    stellar::assets::LevelAsset scene;
    const auto world = make_world({make_mesh("COL_floor")}, scene);
    auto state = stellar::world::RuntimeCollisionState::from_world(world);

    assert(state.set_mesh_enabled("COL_floor", false).applied);
    assert(!state.is_mesh_enabled(0));
    const auto result = state.set_mesh_enabled("COL_floor", true);
    assert(result.applied);
    assert(result.code == "ok");
    assert(state.is_mesh_enabled(0));
}

void unknown_mesh_name_reports_deterministic_not_found() {
    stellar::assets::LevelAsset scene;
    const auto world = make_world({make_mesh("COL_floor")}, scene);
    auto state = stellar::world::RuntimeCollisionState::from_world(world);

    const auto first = state.set_mesh_enabled("Missing", false);
    const auto second = state.set_mesh_enabled("Missing", true);
    assert(!first.applied);
    assert(!second.applied);
    assert(first.code == "not_found");
    assert(second.code == first.code);
    assert(second.message == first.message);
    assert(state.is_mesh_enabled(0));
}

void duplicate_mesh_names_toggle_together() {
    stellar::assets::LevelAsset scene;
    const auto world = make_world(
        {make_mesh("Door"), make_mesh("Floor"), make_mesh("Door")}, scene);
    auto state = stellar::world::RuntimeCollisionState::from_world(world);

    const auto result = state.set_mesh_enabled("Door", false);
    assert(result.applied);
    assert(result.code == "duplicate_name");
    assert(!state.is_mesh_enabled(0));
    assert(state.is_mesh_enabled(1));
    assert(!state.is_mesh_enabled(2));

    assert(state.set_mesh_enabled("Door", true).applied);
    assert(state.is_mesh_enabled(0));
    assert(state.is_mesh_enabled(2));
}

void empty_mesh_names_warn_and_are_not_targetable() {
    stellar::assets::LevelAsset scene;
    const auto world = make_world({make_mesh(""), make_mesh("Named")}, scene);
    auto state = stellar::world::RuntimeCollisionState::from_world(world);

    const auto diagnostics = state.diagnostics();
    assert(diagnostics.size() == 1);
    assert(diagnostics[0] == "collision mesh 0 has an empty name and cannot be targeted");

    const auto result = state.set_mesh_enabled("", false);
    assert(!result.applied);
    assert(result.code == "empty_name");
    assert(state.is_mesh_enabled(0));
}

void mesh_iteration_order_is_deterministic() {
    stellar::assets::LevelAsset scene;
    const auto world = make_world({make_mesh("B"), make_mesh(""), make_mesh("A")}, scene);
    const auto state = stellar::world::RuntimeCollisionState::from_world(world);

    const auto first = state.meshes();
    const auto second = state.meshes();
    assert(first.size() == 3);
    assert(second.size() == first.size());
    assert(first[0].mesh_index == 0 && first[0].name == "B");
    assert(first[1].mesh_index == 1 && first[1].name.empty());
    assert(first[2].mesh_index == 2 && first[2].name == "A");
    for (std::size_t index = 0; index < first.size(); ++index) {
        assert(second[index].mesh_index == first[index].mesh_index);
        assert(second[index].name == first[index].name);
    }
}

void diagnostics_are_deterministic_for_empty_and_duplicate_names() {
    stellar::assets::LevelAsset scene;
    const auto world = make_world(
        {make_mesh("Door"), make_mesh(""), make_mesh("Door"), make_mesh("Gate"),
         make_mesh("Gate")},
        scene);
    const auto state = stellar::world::RuntimeCollisionState::from_world(world);

    const auto diagnostics = state.diagnostics();
    assert(diagnostics.size() == 3);
    assert(diagnostics[0] ==
           "collision mesh name is duplicated and will toggle all matches: Door");
    assert(diagnostics[1] == "collision mesh 1 has an empty name and cannot be targeted");
    assert(diagnostics[2] ==
           "collision mesh name is duplicated and will toggle all matches: Gate");
}

void runtime_world_lifetime_requirements_remain_unchanged() {
    stellar::assets::LevelAsset scene;
    const auto world = make_world({make_mesh("Original")}, scene);
    const auto state = stellar::world::RuntimeCollisionState::from_world(world);

    scene.level_collision->meshes[0].name = "Mutated";
    const auto meshes = state.meshes();
    assert(meshes.size() == 1);
    assert(meshes[0].name == "Original");
    assert(state.is_mesh_enabled(0));
}

} // namespace

int main() {
    empty_world_has_empty_collision_state();
    all_collision_meshes_enabled_by_default();
    set_mesh_enabled_disables_named_mesh();
    set_mesh_enabled_enables_named_mesh();
    unknown_mesh_name_reports_deterministic_not_found();
    duplicate_mesh_names_toggle_together();
    empty_mesh_names_warn_and_are_not_targetable();
    mesh_iteration_order_is_deterministic();
    diagnostics_are_deterministic_for_empty_and_duplicate_names();
    runtime_world_lifetime_requirements_remain_unchanged();
    return 0;
}
