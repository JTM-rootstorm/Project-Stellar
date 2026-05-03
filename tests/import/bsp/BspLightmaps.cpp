#include "stellar/import/bsp/Loader.hpp"

#include "BspFixture.hpp"

#include <cassert>
#include <string_view>

namespace {

bool has_diagnostic(stellar::import::bsp::ImportReport const &report,
                    stellar::import::bsp::DiagnosticCode code,
                    stellar::import::bsp::DiagnosticSeverity severity,
                    std::string_view message_fragment) {
    for (const auto &diagnostic : report.diagnostics) {
        if (diagnostic.code == code && diagnostic.severity == severity &&
            diagnostic.message.find(message_fragment) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void valid_lighting_lump_creates_surface_lightmap_metadata() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false, 0, 12);
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "lightmap.bsp");
    assert(result);
    assert(result->asset.geometry.raw_lighting.size() == 12);
    assert(result->asset.geometry.lightmaps.size() == 1);
    assert(result->asset.geometry.images.size() == 1);
    assert(result->asset.geometry.lightmaps[0].size[0] == 2);
    assert(result->asset.geometry.lightmaps[0].size[1] == 2);
    assert(result->asset.geometry.materials.size() == 1);
    assert(result->asset.geometry.materials[0].lightmap_index == 0);
    const auto &primitive = result->asset.geometry.meshes[0].primitives[0];
    assert(primitive.vertices[0].uv1[0] == 0.0F);
    assert(primitive.vertices[0].uv1[1] == 0.0F);
    assert(primitive.vertices[2].uv1[0] == 1.0F);
    assert(primitive.vertices[2].uv1[1] == 1.0F);
}

void embedded_texture_and_lightmap_preserve_both_uv_sets() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(true, 0, 12);
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "textured_lightmap.bsp");
    assert(result);
    assert(result->asset.geometry.textures.size() == 1);
    assert(result->asset.geometry.lightmaps.size() == 1);
    assert(result->asset.geometry.materials.size() == 1);
    const auto &material = result->asset.geometry.materials[0];
    assert(material.texture_index == 0);
    assert(material.lightmap_index == 0);
    const auto &primitive = result->asset.geometry.meshes[0].primitives[0];
    assert(primitive.vertices[2].uv0[0] == 8.0F);
    assert(primitive.vertices[2].uv0[1] == 8.0F);
    assert(primitive.vertices[2].uv1[0] == 1.0F);
    assert(primitive.vertices[2].uv1[1] == 1.0F);
}

void multiple_faces_with_same_texture_get_distinct_lightmap_materials() {
    const auto bytes = stellar::tests::bsp_fixture::two_lit_faces_bsp();
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "two_lightmaps.bsp");
    assert(result);
    assert(result->asset.geometry.lightmaps.size() == 2);
    assert(result->asset.geometry.materials.size() == 2);
    assert(result->asset.geometry.materials[0].source_name ==
           result->asset.geometry.materials[1].source_name);
    assert(result->asset.geometry.materials[0].lightmap_index == 0);
    assert(result->asset.geometry.materials[1].lightmap_index == 1);
    assert(result->asset.geometry.lightmaps[0].style == 0);
    assert(result->asset.geometry.lightmaps[1].style == 2);
}

void invalid_light_offset_warns_and_falls_back_unlit() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false, 100, 3);
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "bad_lightmap.bsp");
    assert(result);
    assert(result->asset.geometry.raw_lighting.size() == 3);
    assert(result->asset.geometry.lightmaps.empty());
    assert(!result->asset.geometry.materials[0].lightmap_index.has_value());

    bool found_invalid_lighting = false;
    for (const auto &diagnostic : result->report.diagnostics) {
        found_invalid_lighting = found_invalid_lighting ||
                                 diagnostic.code ==
                                     stellar::import::bsp::DiagnosticCode::kInvalidLightingData;
    }
    assert(found_invalid_lighting);
}

void all_black_lightmap_warns_with_deterministic_stats() {
    auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false, 0, 12);
    const std::size_t lighting_offset = bytes.size() - 12;
    for (std::size_t i = 0; i < 12; ++i) {
        bytes[lighting_offset + i] = std::byte{0U};
    }

    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "black_lightmap.bsp");
    assert(result);
    assert(result->asset.geometry.raw_lighting.size() == 12);
    assert(result->asset.geometry.lightmaps.size() == 1);
    assert(has_diagnostic(result->report,
                          stellar::import::bsp::DiagnosticCode::kAllBlackLightmap,
                          stellar::import::bsp::DiagnosticSeverity::kWarning,
                          "raw_lighting_bytes=12 imported_lightmap_count=1"));
    assert(has_diagnostic(result->report,
                          stellar::import::bsp::DiagnosticCode::kAllBlackLightmap,
                          stellar::import::bsp::DiagnosticSeverity::kWarning,
                          "min_rgb=(0,0,0) max_rgb=(0,0,0)"));
    assert(has_diagnostic(result->report,
                          stellar::import::bsp::DiagnosticCode::kAllBlackLightmap,
                          stellar::import::bsp::DiagnosticSeverity::kWarning,
                          "all_black=true"));
}

void nonzero_lightmap_emits_info_stats_without_warning() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false, 0, 12);
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "nonzero_lightmap.bsp");
    assert(result);
    assert(result->asset.geometry.raw_lighting.size() == 12);
    assert(result->asset.geometry.lightmaps.size() == 1);
    assert(has_diagnostic(result->report,
                          stellar::import::bsp::DiagnosticCode::kLightmapStats,
                          stellar::import::bsp::DiagnosticSeverity::kInfo,
                          "BSP lightmap summary raw_lighting_bytes=12 imported_lightmap_count=1"));
    assert(has_diagnostic(result->report,
                          stellar::import::bsp::DiagnosticCode::kLightmapStats,
                          stellar::import::bsp::DiagnosticSeverity::kInfo,
                          "raw_lighting_bytes=12 imported_lightmap_count=1"));
    assert(has_diagnostic(result->report,
                          stellar::import::bsp::DiagnosticCode::kLightmapStats,
                          stellar::import::bsp::DiagnosticSeverity::kInfo,
                          "min_rgb=(0,1,2) max_rgb=(9,10,11)"));
    assert(has_diagnostic(result->report,
                          stellar::import::bsp::DiagnosticCode::kLightmapStats,
                          stellar::import::bsp::DiagnosticSeverity::kInfo,
                          "average_rgb=(4.5,5.5,6.5) all_black=false"));
    assert(has_diagnostic(result->report,
                          stellar::import::bsp::DiagnosticCode::kLightmapStats,
                          stellar::import::bsp::DiagnosticSeverity::kInfo,
                          "BSP material lightmap binding material_index=0"));
    assert(!has_diagnostic(result->report,
                           stellar::import::bsp::DiagnosticCode::kAllBlackLightmap,
                           stellar::import::bsp::DiagnosticSeverity::kWarning,
                           "all_black=true"));
}

void missing_lighting_lump_emits_zero_lightmap_summary() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false);
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "unlit_summary.bsp");
    assert(result);
    assert(result->asset.geometry.raw_lighting.empty());
    assert(result->asset.geometry.lightmaps.empty());
    assert(has_diagnostic(result->report,
                          stellar::import::bsp::DiagnosticCode::kLightmapStats,
                          stellar::import::bsp::DiagnosticSeverity::kInfo,
                          "BSP lightmap summary raw_lighting_bytes=0 imported_lightmap_count=0"));
}

} // namespace

int main() {
    valid_lighting_lump_creates_surface_lightmap_metadata();
    embedded_texture_and_lightmap_preserve_both_uv_sets();
    multiple_faces_with_same_texture_get_distinct_lightmap_materials();
    invalid_light_offset_warns_and_falls_back_unlit();
    all_black_lightmap_warns_with_deterministic_stats();
    nonzero_lightmap_emits_info_stats_without_warning();
    missing_lighting_lump_emits_zero_lightmap_summary();
    return 0;
}
