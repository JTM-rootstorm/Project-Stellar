#include "stellar/world/CollisionValidation.hpp"

#include <cassert>
#include <limits>
#include <string>
#include <string_view>

namespace {

stellar::assets::CollisionTriangle make_floor_triangle() {
    return stellar::assets::CollisionTriangle{.a = {0.0F, 0.0F, 0.0F},
                                              .b = {1.0F, 0.0F, 0.0F},
                                              .c = {0.0F, 1.0F, 0.0F},
                                              .normal = {0.0F, 0.0F, 1.0F}};
}

stellar::assets::CollisionMesh make_mesh(std::string_view name) {
    stellar::assets::CollisionMesh mesh;
    mesh.name = std::string{name};
    mesh.triangles = {make_floor_triangle()};
    mesh.bounds_min = {0.0F, 0.0F, 0.0F};
    mesh.bounds_max = {1.0F, 1.0F, 0.0F};
    return mesh;
}

stellar::assets::LevelCollisionAsset make_valid_collision() {
    stellar::assets::LevelCollisionAsset collision;
    collision.meshes = {make_mesh("COL_floor")};
    collision.bounds_min = {0.0F, 0.0F, 0.0F};
    collision.bounds_max = {1.0F, 1.0F, 0.0F};
    return collision;
}

bool has_code(const stellar::world::CollisionValidationReport& report, std::string_view code) {
    for (const auto& finding : report.findings) {
        if (finding.code == code) {
            return true;
        }
    }
    return false;
}

void verify_empty_collision_asset_returns_warning_not_error() {
    const stellar::assets::LevelCollisionAsset collision;
    const auto report = stellar::world::validate_level_collision(collision);

    assert(!report.has_errors);
    assert(report.findings.size() == 1);
    assert(report.findings[0].severity == stellar::world::CollisionValidationSeverity::kWarning);
    assert(report.findings[0].code == "empty_level_collision");
}

void verify_valid_floor_mesh_returns_no_findings() {
    const auto report = stellar::world::validate_level_collision(make_valid_collision());
    assert(!report.has_errors);
    assert(report.findings.empty());
}

void verify_zero_area_triangle_returns_error() {
    auto collision = make_valid_collision();
    collision.meshes[0].triangles[0].c = collision.meshes[0].triangles[0].a;

    const auto report = stellar::world::validate_level_collision(collision);
    assert(report.has_errors);
    assert(has_code(report, "near_zero_triangle_area"));
}

void verify_non_finite_vertex_returns_error() {
    auto collision = make_valid_collision();
    collision.meshes[0].triangles[0].a[0] = std::numeric_limits<float>::infinity();

    const auto report = stellar::world::validate_level_collision(collision);
    assert(report.has_errors);
    assert(has_code(report, "non_finite_vertex"));
}

void verify_non_finite_and_near_zero_normals_return_findings() {
    auto non_finite = make_valid_collision();
    non_finite.meshes[0].triangles[0].normal[1] = std::numeric_limits<float>::quiet_NaN();
    const auto non_finite_report = stellar::world::validate_level_collision(non_finite);
    assert(non_finite_report.has_errors);
    assert(has_code(non_finite_report, "non_finite_normal"));

    auto near_zero = make_valid_collision();
    near_zero.meshes[0].triangles[0].normal = {0.0F, 0.0F, 0.0F};
    const auto near_zero_report = stellar::world::validate_level_collision(near_zero);
    assert(near_zero_report.has_errors);
    assert(has_code(near_zero_report, "near_zero_normal"));
}

void verify_mesh_bounds_mismatch_returns_finding() {
    auto collision = make_valid_collision();
    collision.meshes[0].bounds_max = {0.25F, 0.0F, 0.25F};

    const auto report = stellar::world::validate_level_collision(collision);
    assert(report.has_errors);
    assert(has_code(report, "mesh_bounds_do_not_contain_vertices"));
}

void verify_aggregate_bounds_mismatch_returns_finding() {
    auto collision = make_valid_collision();
    collision.bounds_max = {0.5F, 0.0F, 0.5F};

    const auto report = stellar::world::validate_level_collision(collision);
    assert(report.has_errors);
    assert(has_code(report, "aggregate_bounds_do_not_contain_mesh_bounds"));
}

void verify_empty_mesh_and_large_bounds_return_warnings() {
    auto collision = make_valid_collision();
    collision.meshes[0].triangles.clear();
    collision.meshes[0].bounds_max = {200000.0F, 0.0F, 0.0F};
    collision.bounds_max = {200000.0F, 0.0F, 0.0F};

    const auto report = stellar::world::validate_level_collision(collision);
    assert(!report.has_errors);
    assert(has_code(report, "empty_mesh"));
    assert(has_code(report, "mesh_bounds_extremely_large"));
    assert(has_code(report, "aggregate_bounds_extremely_large"));
}

void verify_duplicate_mesh_names_return_deterministic_warnings() {
    auto collision = make_valid_collision();
    collision.meshes.push_back(make_mesh("COL_floor"));

    const auto report = stellar::world::validate_level_collision(collision);
    assert(!report.has_errors);
    assert(report.findings.size() == 1);
    assert(report.findings[0].code == "duplicate_mesh_name");
    assert(report.findings[0].mesh_index == 1);
}

void verify_empty_collision_mesh_name_returns_warning_not_error() {
    auto collision = make_valid_collision();
    collision.meshes[0].name.clear();

    const auto report = stellar::world::validate_level_collision(collision);
    assert(!report.has_errors);
    assert(report.findings.size() == 1);
    assert(report.findings[0].code == "empty_mesh_name");
    assert(report.findings[0].mesh_index == 0);
    assert(report.findings[0].severity == stellar::world::CollisionValidationSeverity::kWarning);
}

void verify_no_walkable_surfaces_returns_warning() {
    auto collision = make_valid_collision();
    collision.meshes[0].triangles[0].normal = {1.0F, 0.0F, 0.0F};

    const auto report = stellar::world::validate_level_collision(collision);
    assert(!report.has_errors);
    assert(has_code(report, "no_walkable_upward_surfaces"));
}

void verify_findings_are_deterministically_sorted() {
    auto collision = make_valid_collision();
    collision.meshes[0].triangles.push_back(make_floor_triangle());
    collision.meshes[0].triangles[0].normal = {0.0F, 0.0F, 0.0F};
    collision.meshes[0].triangles[0].a[0] = std::numeric_limits<float>::infinity();
    collision.meshes[0].triangles[1].normal = {0.0F, 0.0F, 0.0F};

    const auto report = stellar::world::validate_level_collision(collision);
    assert(report.has_errors);
    for (std::size_t i = 1; i < report.findings.size(); ++i) {
        const auto& previous = report.findings[i - 1];
        const auto& current = report.findings[i];
        assert(previous.mesh_index < current.mesh_index ||
               (previous.mesh_index == current.mesh_index &&
                previous.triangle_index < current.triangle_index) ||
               (previous.mesh_index == current.mesh_index &&
                previous.triangle_index == current.triangle_index &&
                previous.code <= current.code));
    }
}

} // namespace

int main() {
    verify_empty_collision_asset_returns_warning_not_error();
    verify_valid_floor_mesh_returns_no_findings();
    verify_zero_area_triangle_returns_error();
    verify_non_finite_vertex_returns_error();
    verify_non_finite_and_near_zero_normals_return_findings();
    verify_mesh_bounds_mismatch_returns_finding();
    verify_aggregate_bounds_mismatch_returns_finding();
    verify_empty_mesh_and_large_bounds_return_warnings();
    verify_duplicate_mesh_names_return_deterministic_warnings();
    verify_empty_collision_mesh_name_returns_warning_not_error();
    verify_no_walkable_surfaces_returns_warning();
    verify_findings_are_deterministically_sorted();
    return 0;
}
