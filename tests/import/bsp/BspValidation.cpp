#include "stellar/import/bsp/Validation.hpp"

#include "../../fixtures/BspFixture.hpp"
#include "BspFixture.hpp"

#include <cassert>
#include <string_view>

namespace {

bool has_code(const stellar::import::bsp::ImportReport &report,
              stellar::import::bsp::DiagnosticCode code) {
  for (const auto &diagnostic : report.diagnostics) {
    if (diagnostic.code == code) {
      return true;
    }
  }
  return false;
}

void minimal_valid_map_validates() {
  const auto bytes = stellar::tests::fixtures::build_bsp_minimal_valid_fixture();
  const auto validation = stellar::import::bsp::validate_level_from_memory(
      bytes, "minimal_valid.bsp");
  assert(validation.has_value());
  assert(validation->valid);
  assert(validation->loaded_level.has_value());
  assert(validation->loaded_level->asset.level_collision.has_value());
}

void pvs_fixture_validates_visibility_without_renderer() {
  const auto bytes = stellar::tests::fixtures::build_bsp_pvs_fixture();
  const auto validation = stellar::import::bsp::validate_level_from_memory(
      bytes, "pvs_valid.bsp");
  assert(validation.has_value());
  assert(validation->valid);
  assert(validation->loaded_level->asset.visibility.available);
  assert(validation->loaded_level->asset.visibility.leaves.size() == 1);
}

void material_and_lightmap_diagnostics_are_structured() {
  const auto missing_texture = stellar::tests::bsp_fixture::single_face_bsp(false);
  const auto missing_validation = stellar::import::bsp::validate_level_from_memory(
      missing_texture, "missing_texture.bsp");
  assert(missing_validation.has_value());
  assert(has_code(missing_validation->report,
                  stellar::import::bsp::DiagnosticCode::kMissingTexture));
  assert(has_code(missing_validation->report,
                  stellar::import::bsp::DiagnosticCode::kMaterialFallbackUsed));

  const auto embedded_texture = stellar::tests::bsp_fixture::single_face_bsp(true);
  const auto embedded_validation = stellar::import::bsp::validate_level_from_memory(
      embedded_texture, "embedded_texture.bsp");
  assert(embedded_validation.has_value());
  assert(embedded_validation->valid);
  assert(!has_code(embedded_validation->report,
                   stellar::import::bsp::DiagnosticCode::kMissingTexture));

  const auto bad_lightmap = stellar::tests::bsp_fixture::single_face_bsp(false, 100, 3);
  const auto light_validation = stellar::import::bsp::validate_level_from_memory(
      bad_lightmap, "bad_lightmap.bsp");
  assert(light_validation.has_value());
  assert(has_code(light_validation->report,
                  stellar::import::bsp::DiagnosticCode::kInvalidLightingData));

  auto black_lightmap = stellar::tests::bsp_fixture::single_face_bsp(false, 0, 12);
  const std::size_t lighting_offset = black_lightmap.size() - 12;
  for (std::size_t i = 0; i < 12; ++i) {
    black_lightmap[lighting_offset + i] = std::byte{0U};
  }
  const auto black_validation = stellar::import::bsp::validate_level_from_memory(
      black_lightmap, "black_lightmap.bsp");
  assert(black_validation.has_value());
  assert(black_validation->valid);
  assert(has_code(black_validation->report,
                  stellar::import::bsp::DiagnosticCode::kAllBlackLightmap));

  const auto nonzero_lightmap = stellar::tests::bsp_fixture::single_face_bsp(false, 0, 12);
  const auto nonzero_validation = stellar::import::bsp::validate_level_from_memory(
      nonzero_lightmap, "nonzero_lightmap.bsp");
  assert(nonzero_validation.has_value());
  assert(nonzero_validation->valid);
  assert(has_code(nonzero_validation->report,
                  stellar::import::bsp::DiagnosticCode::kLightmapStats));
}

void malformed_binary_and_face_references_are_diagnostic_codes() {
  const auto bad_lump =
      stellar::tests::fixtures::build_bsp_malformed_lump_bounds_fixture();
  const auto lump_validation = stellar::import::bsp::validate_level_from_memory(
      bad_lump, "bad_lump.bsp");
  assert(lump_validation.has_value());
  assert(!lump_validation->valid);
  assert(lump_validation->report.has_errors);
  assert(has_code(lump_validation->report,
                  stellar::import::bsp::DiagnosticCode::kLumpOutOfBounds));

  const auto bad_face =
      stellar::tests::fixtures::build_bsp_invalid_face_reference_fixture();
  const auto face_validation = stellar::import::bsp::validate_level_from_memory(
      bad_face, "bad_face.bsp");
  assert(face_validation.has_value());
  assert(face_validation->valid);
  assert(has_code(face_validation->report,
                  stellar::import::bsp::DiagnosticCode::kInvalidFaceReference));
  assert(has_code(face_validation->report,
                  stellar::import::bsp::DiagnosticCode::kNoWorldspawnGeometry));
}

void entity_authoring_validation_is_deterministic() {
  const auto bad_vector =
      stellar::tests::fixtures::build_bsp_invalid_entity_vector_fixture();
  const auto vector_validation = stellar::import::bsp::validate_level_from_memory(
      bad_vector, "bad_vector.bsp");
  assert(vector_validation.has_value());
  assert(vector_validation->valid);
  assert(has_code(vector_validation->report,
                  stellar::import::bsp::DiagnosticCode::kUnsupportedEntityKey));

  const auto bad_script =
      stellar::tests::fixtures::build_bsp_invalid_script_path_fixture();
  const auto script_validation = stellar::import::bsp::validate_level_from_memory(
      bad_script, "bad_script.bsp");
  assert(script_validation.has_value());
  assert(!script_validation->valid);
  assert(has_code(script_validation->report,
                  stellar::import::bsp::DiagnosticCode::kScriptPathEscape));

  const auto missing_spawn =
      stellar::tests::fixtures::build_bsp_missing_player_spawn_fixture();
  const auto spawn_validation = stellar::import::bsp::validate_level_from_memory(
      missing_spawn, "missing_spawn.bsp");
  assert(spawn_validation.has_value());
  assert(spawn_validation->valid);
  assert(has_code(spawn_validation->report,
                  stellar::import::bsp::DiagnosticCode::kMissingPlayerSpawn));
}

void no_collision_policy_warns_without_failing() {
  stellar::import::bsp::ValidationOptions options;
  options.load_options.build_triangle_collision_from_faces = false;
  const auto bytes = stellar::tests::fixtures::build_bsp_minimal_valid_fixture();
  const auto validation = stellar::import::bsp::validate_level_from_memory(
      bytes, "no_collision.bsp", options);
  assert(validation.has_value());
  assert(validation->valid);
  assert(has_code(validation->report,
                  stellar::import::bsp::DiagnosticCode::kNoCollisionTriangles));
}

} // namespace

int main() {
  minimal_valid_map_validates();
  pvs_fixture_validates_visibility_without_renderer();
  material_and_lightmap_diagnostics_are_structured();
  malformed_binary_and_face_references_are_diagnostic_codes();
  entity_authoring_validation_is_deterministic();
  no_collision_policy_warns_without_failing();
  return 0;
}
